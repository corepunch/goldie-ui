#!/usr/bin/env python3
"""
orion_rc_mcp.py — MCP server that wraps the Orion TCP remote-control protocol.

Usage:
  python tools/orion_rc_mcp.py [--port PORT] [--host HOST]

  PORT defaults to 17777 (the Orion -rc default).
  Can also be set via ORION_RC_PORT / ORION_RC_HOST environment variables.

The app must be running with the -rc flag:
  ./my_app -rc 17777

Add to your MCP client config (e.g. Claude Desktop):
  {
    "mcpServers": {
      "orion": {
        "command": "python",
        "args": ["/path/to/orion-ui/tools/orion_rc_mcp.py"]
      }
    }
  }

Dependencies:
  pip install mcp
"""

import argparse
import os
import socket
import sys
from typing import Optional

# ---------------------------------------------------------------------------
# TCP client
# ---------------------------------------------------------------------------

class _RCClient:
    """Persistent TCP connection to the Orion RC server."""

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._sock: Optional[socket.socket] = None
        self._buf  = b""

    def _connect(self) -> None:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((self._host, self._port))
        self._sock = s
        self._buf  = b""

    def _ensure(self) -> None:
        if self._sock is None:
            self._connect()

    def _readline(self) -> str:
        assert self._sock
        while b"\n" not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ConnectionError("RC server closed connection")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line.decode("utf-8", errors="replace")

    def send(self, command: str) -> list[str]:
        """Send one command; return all reply lines (excluding trailing ok)."""
        try:
            self._ensure()
        except OSError as exc:
            raise ConnectionError(
                f"Cannot connect to Orion RC server at "
                f"{self._host}:{self._port} — is the app running with -rc?"
            ) from exc

        assert self._sock
        try:
            self._sock.sendall((command + "\n").encode())
            lines: list[str] = []
            while True:
                line = self._readline()
                if line == "ok":
                    return lines
                if line.startswith("err "):
                    raise RuntimeError(line[4:])
                lines.append(line)
        except (OSError, ConnectionError):
            self._sock = None
            raise

    def close(self) -> None:
        if self._sock:
            try:
                self._sock.sendall(b"quit\n")
            except OSError:
                pass
            self._sock.close()
            self._sock = None


# ---------------------------------------------------------------------------
# Globals — set by parse_args() before the server starts
# ---------------------------------------------------------------------------

_client: Optional[_RCClient] = None


def _rc(command: str) -> list[str]:
    assert _client is not None
    return _client.send(command)


# ---------------------------------------------------------------------------
# MCP server
# ---------------------------------------------------------------------------

from mcp.server.fastmcp import FastMCP  # noqa: E402

mcp = FastMCP(
    "orion-rc",
    instructions=(
        "Remote-control server for an Orion UI application. "
        "Use list_windows to discover what is on screen before clicking. "
        "Prefer click_ctrl over raw click when you know the control ID — "
        "it is layout-independent. "
        "Always call screenshot after interactions to verify the result."
    ),
)


# --- Mouse input ---

@mcp.tool()
def click(x: int, y: int) -> str:
    """Left-click at absolute screen coordinates (x, y)."""
    _rc(f"click {x} {y}")
    return "ok"


@mcp.tool()
def right_click(x: int, y: int) -> str:
    """Right-click at absolute screen coordinates (x, y)."""
    _rc(f"rclick {x} {y}")
    return "ok"


@mcp.tool()
def double_click(x: int, y: int) -> str:
    """Double-click at absolute screen coordinates (x, y)."""
    _rc(f"dblclick {x} {y}")
    return "ok"


@mcp.tool()
def drag(x1: int, y1: int, x2: int, y2: int) -> str:
    """Press at (x1, y1), drag to (x2, y2), and release."""
    _rc(f"drag {x1} {y1} {x2} {y2}")
    return "ok"


@mcp.tool()
def mouse_move(x: int, y: int) -> str:
    """Move the mouse cursor to (x, y) without clicking."""
    _rc(f"move {x} {y}")
    return "ok"


@mcp.tool()
def scroll(x: int, y: int, dx: int, dy: int) -> str:
    """Scroll at (x, y) by (dx, dy). Positive dy scrolls down."""
    _rc(f"scroll {x} {y} {dx} {dy}")
    return "ok"


# --- Keyboard input ---

@mcp.tool()
def key_press(code: int, mods: int = 0) -> str:
    """Press and release a key by virtual key code.

    code: integer virtual key code (e.g. 13=Enter, 27=Escape, 65='A').
    mods: bitmask — 1=Shift, 2=Ctrl, 4=Alt, 8=Cmd.

    Common codes: 8=Backspace, 9=Tab, 13=Enter, 27=Escape, 32=Space,
    33=PageUp, 34=PageDown, 35=End, 36=Home, 37=Left, 38=Up, 39=Right,
    40=Down, 46=Delete, 65-90=A-Z, 112-123=F1-F12.
    """
    _rc(f"key {code} {mods}" if mods else f"key {code}")
    return "ok"


@mcp.tool()
def type_text(text: str) -> str:
    """Type a string of text character by character.

    Each character is injected as a key-down/key-up pair. For single-line
    text fields this is equivalent to the user typing on the keyboard.
    """
    _rc(f"type {text}")
    return "ok"


# --- Screenshot ---

@mcp.tool()
def screenshot(path: str) -> str:
    """Schedule a screenshot to be saved to path on the next painted frame.

    Returns immediately; the file is written asynchronously. Wait for the
    next event-loop iteration before reading the file.

    path: absolute file path for the output JPEG (e.g. /tmp/screen.jpg).
    """
    _rc(f"screenshot {path}")
    return "ok"


# --- Window inspection ---

@mcp.tool()
def list_windows() -> list[dict]:
    """Return all root windows currently open in the application.

    Each entry has: title (str), x, y, w, h (int, screen coordinates).
    Use title values with get_rect, get_ctrl_rect, get_text, get_value,
    and click_ctrl.
    """
    lines = _rc("list_windows")
    result = []
    for line in lines:
        parts = line.split(" ", 5)
        if parts[0] == "window" and len(parts) == 6:
            result.append({
                "title": parts[5],
                "x": int(parts[1]),
                "y": int(parts[2]),
                "w": int(parts[3]),
                "h": int(parts[4]),
            })
    return result


@mcp.tool()
def get_focus() -> str:
    """Return the title of the currently focused window (empty string if none)."""
    lines = _rc("get_focus")
    for line in lines:
        if line.startswith("focused "):
            return line[8:]
    return ""


@mcp.tool()
def get_rect(title: str) -> dict:
    """Return the screen rectangle of the root window with the given title.

    Returns: {x, y, w, h} in screen coordinates.
    Use the centre (x + w//2, y + h//2) to click in the middle of a window.
    """
    lines = _rc(f"get_rect {title}")
    for line in lines:
        parts = line.split()
        if parts[0] == "rect" and len(parts) == 5:
            return {"x": int(parts[1]), "y": int(parts[2]),
                    "w": int(parts[3]), "h": int(parts[4])}
    raise RuntimeError("no rect in response")


@mcp.tool()
def get_ctrl_rect(ctrl_id: int, title: str) -> dict:
    """Return the screen rectangle of a control inside a named window.

    ctrl_id: the integer control ID (e.g. ID_INSPECTOR_INPUT = 2).
    title:   exact title of the parent window.

    Returns: {x, y, w, h} in screen coordinates — ready to pass to click().
    """
    lines = _rc(f"get_ctrl_rect {ctrl_id} {title}")
    for line in lines:
        parts = line.split()
        if parts[0] == "rect" and len(parts) == 5:
            return {"x": int(parts[1]), "y": int(parts[2]),
                    "w": int(parts[3]), "h": int(parts[4])}
    raise RuntimeError("no rect in response")


@mcp.tool()
def get_text(ctrl_id: int, title: str) -> str:
    """Read the text content (title field) of a control inside a named window.

    ctrl_id: integer control ID.
    title:   exact title of the parent window.
    """
    lines = _rc(f"get_text {ctrl_id} {title}")
    for line in lines:
        if line.startswith("text "):
            return line[5:]
        if line == "text":
            return ""
    return ""


@mcp.tool()
def get_value(ctrl_id: int, title: str) -> int:
    """Read the numeric value of a control (e.g. checkbox state, slider pos).

    ctrl_id: integer control ID.
    title:   exact title of the parent window.
    """
    lines = _rc(f"get_value {ctrl_id} {title}")
    for line in lines:
        if line.startswith("value "):
            return int(line[6:])
    return 0


# --- Semantic control interaction ---

@mcp.tool()
def click_ctrl(ctrl_id: int, title: str) -> str:
    """Click the centre of a named control inside a window.

    Preferred over raw click() when you know the control ID — coordinates are
    computed by the app itself so layout changes don't break automation.

    ctrl_id: integer control ID.
    title:   exact title of the parent window.
    """
    _rc(f"click_ctrl {ctrl_id} {title}")
    return "ok"


# --- App lifecycle ---

@mcp.tool()
def stop_app() -> str:
    """Send a window-close event to quit the application gracefully."""
    _rc("stop")
    return "ok"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    global _client

    parser = argparse.ArgumentParser(description="Orion RC MCP bridge")
    parser.add_argument("--port", type=int,
                        default=int(os.environ.get("ORION_RC_PORT", "17777")))
    parser.add_argument("--host",
                        default=os.environ.get("ORION_RC_HOST", "127.0.0.1"))
    args, _ = parser.parse_known_args()

    _client = _RCClient(args.host, args.port)
    try:
        mcp.run()
    finally:
        _client.close()


if __name__ == "__main__":
    main()
