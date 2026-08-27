#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


def forms(text, kind):
	pattern = re.compile(r"<" + kind + r"\s+([A-Z0-9_-]+)\b")
	for match in pattern.finditer(text):
		depth = 0
		quoted = False
		escaped = False
		for i in range(match.start(), len(text)):
			ch = text[i]
			if quoted:
				if escaped:
					escaped = False
				elif ch == "\\":
					escaped = True
				elif ch == '"':
					quoted = False
				continue
			if ch == '"':
				quoted = True
			elif ch == '<':
				depth += 1
			elif ch == '>':
				depth -= 1
				if depth == 0:
					yield match.group(1), text[match.start():i + 1], text.count("\n", 0, match.start()) + 1
					break


def field(block, name):
	match = re.search(r"\(" + re.escape(name) + r"\s+\"((?:\\.|[^\"])*)\"\)", block)
	return match.group(1) if match else ""


def atom(block, name):
	match = re.search(r"\(" + re.escape(name) + r"\s+([A-Z0-9_-]+)", block)
	return match.group(1) if match else ""


def atoms(block, name):
	match = re.search(r"\(" + re.escape(name) + r"\s+([^\)]*)\)", block)
	return " ".join(match.group(1).split()) if match else ""


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("root", type=Path)
	parser.add_argument("room")
	parser.add_argument("--evidence-limit", type=int, default=120, help="maximum evidence hits; 0 prints all")
	args = parser.parse_args()
	room_id = args.room.upper()
	zil_files = sorted(args.root.rglob("*.zil"))
	rooms = []
	objects = {}
	for path in zil_files:
		text = path.read_text(errors="replace")
		for name, block, line in forms(text, "ROOM"):
			if name == room_id:
				rooms.append((path, block, line))
		for name, block, line in forms(text, "OBJECT"):
			objects[name] = {"path": path, "block": block, "line": line, "parent": atom(block, "IN")}
	if not rooms:
		raise SystemExit(f"room not found: {room_id}")

	def belongs(name):
		seen = set()
		parent = objects[name]["parent"]
		while parent and parent not in seen:
			if parent == room_id:
				return True
			seen.add(parent)
			parent = objects.get(parent, {}).get("parent", "")
		return False

	path, block, line = rooms[0]
	print(f"# Extracted ZIL room: {room_id}\n")
	print(f"- Room form: `{path}:{line}`")
	print(f"- Display name: {field(block, 'DESC') or '(missing)' }")
	print(f"- Long description: {field(block, 'LDESC') or '(missing)' }\n")
	print("## Room form\n")
	print("```zil")
	print(block)
	print("```\n")
	print("## Contained objects\n")
	print("| ID | Parent | Description | Long description | Flags | Source |")
	print("|---|---|---|---|---|---|")
	contained = []
	for name in sorted(objects):
		if belongs(name):
			item = objects[name]
			contained.append(name)
			print(f"| {name} | {item['parent']} | {field(item['block'], 'DESC')} | {field(item['block'], 'LDESC')} | {atoms(item['block'], 'FLAGS')} | `{item['path']}:{item['line']}` |")
	print("\n## Evidence hits\n")
	terms = [room_id] + contained
	pattern = re.compile("|".join(re.escape(term) for term in sorted(terms, key=len, reverse=True)))
	count = 0
	evidence_files = sorted(set(zil_files + list(args.root.rglob("*.md"))))
	for evidence in evidence_files:
		for lineno, source_line in enumerate(evidence.read_text(errors="replace").splitlines(), 1):
			if pattern.search(source_line):
				if args.evidence_limit and count >= args.evidence_limit:
					print(f"\n_Evidence truncated at {args.evidence_limit} hits; rerun with `--evidence-limit 0` for the complete index._")
					return
				print(f"- `{evidence}:{lineno}` {source_line.strip()}")
				count += 1


if __name__ == "__main__":
	main()
