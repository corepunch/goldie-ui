#include "pty.h"
#include "vgat.h"

#if defined(__unix__) || defined(__APPLE__)

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#else
#include <libutil.h>
#endif

int vgat_pty_open(const char *shell, int rows, int cols, int *pid_out) {
  if (!pid_out) return -1;

  struct winsize ws = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
  int master_fd = -1;
  int pid = forkpty(&master_fd, NULL, NULL, &ws);
  if (pid == 0) {
    // Child process
    const char *sh = shell ? shell : (getenv("SHELL") ? getenv("SHELL") : "/bin/sh");
    execl(sh, sh, NULL);
    _exit(127);
  }

  if (pid < 0) {
    if (master_fd >= 0) close(master_fd);
    return -1;
  }

  // Set non-blocking on master
  int flags = fcntl(master_fd, F_GETFL, 0);
  if (flags >= 0) fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

  // Set initial window size
  vgat_pty_resize(master_fd, rows, cols);

  *pid_out = pid;
  return master_fd;
}

int vgat_pty_exec(const char *const *argv, int rows, int cols, int *pid_out) {
  if (!pid_out || !argv || !argv[0]) return -1;

  struct winsize ws = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
  int master_fd = -1;
  int pid = forkpty(&master_fd, NULL, NULL, &ws);
  if (pid == 0) {
    setenv("TERM", "linux", 1);
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }

  if (pid < 0) {
    if (master_fd >= 0) close(master_fd);
    return -1;
  }

  int flags = fcntl(master_fd, F_GETFL, 0);
  if (flags >= 0) fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

  vgat_pty_resize(master_fd, rows, cols);

  *pid_out = pid;
  return master_fd;
}

void vgat_pty_close(int pid) {
  if (pid > 0) {
    kill(pid, SIGTERM);
    int status;
    waitpid(pid, &status, 0);
  }
}

bool vgat_pty_resize(int master_fd, int rows, int cols) {
  if (master_fd < 0 || rows <= 0 || cols <= 0) return false;
  struct winsize ws = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
  return ioctl(master_fd, TIOCSWINSZ, &ws) == 0;
}

int vgat_pty_read(int master_fd, void *buf, int sz) {
  if (master_fd < 0 || !buf || sz <= 0) return -1;
  return read(master_fd, buf, (size_t)sz);
}

int vgat_pty_write(int master_fd, const void *buf, int sz) {
  if (master_fd < 0 || !buf || sz <= 0) return -1;
  return write(master_fd, buf, (size_t)sz);
}

#elif defined(_WIN32) || defined(_WIN64)

// Windows PTY stubs — ConPTY integration is more involved
// and requires creating a Pseudo Console and attaching pipes.

int vgat_pty_open(const char *shell, int rows, int cols, int *pid_out) {
  (void)shell; (void)rows; (void)cols; (void)pid_out;
  return -1;  // Not yet implemented on Windows
}

int vgat_pty_exec(const char *const *argv, int rows, int cols, int *pid_out) {
  (void)argv; (void)rows; (void)cols; (void)pid_out;
  return -1;  // Not yet implemented on Windows
}

void vgat_pty_close(int pid) {
  (void)pid;
}

bool vgat_pty_resize(int master_fd, int rows, int cols) {
  (void)master_fd; (void)rows; (void)cols;
  return false;
}

int vgat_pty_read(int master_fd, void *buf, int sz) {
  (void)master_fd; (void)buf; (void)sz;
  return -1;
}

int vgat_pty_write(int master_fd, const void *buf, int sz) {
  (void)master_fd; (void)buf; (void)sz;
  return -1;
}

#else

#error "Unsupported platform for PTY"

#endif
