#!/usr/bin/env python3

import errno
import os
import pty
import selectors
import sys
import termios
import tty

class EscSeqFilter:
	def __init__(self):
		self.state = "TXT"
		self.csi = bytearray()

	def _allow_csi(self, seq: bytes) -> bool:
		# Allow only: ESC [ <digits?> [D|C|P] ; ESC [ <0?> K ; ESC [ <0?> J
		if not seq.startswith(b"\x1b["):
			return False
		final = seq[-1:]
		body = seq[2:-1]  # bytes between '[' and final

		# reject anything with multiple params or private markers
		if any(ch in b";?:" for ch in body):
			return False

		if final in b"DCP":
			# digits-only (or empty) count is fine
			return body.isdigit() or body == b""

		if final == b"K":
			# EL: allow empty or 0/1/2
			return body in (b"", b"0", b"1", b"2") or body.isdigit()

		if final == b"J":
			# ED: allow only J0 or bare J (default 0). Block J1/J2/J3.
			return body in (b"", b"0")

		return False

	def feed(self, data):
		out = bytearray()
		for ch in data:
			if self.state == "TXT":
				if ch == 0x1B:
					self.state = "ESC"
				elif 0x20 <= ch <= 0x7F or ch in (0x0A, 0x0D, 0x08):
					out.append(ch)
			elif self.state == "ESC":
				if ch == ord("["):
					self.state = "CSI"
					self.csi = bytearray(b"\x1b[")
				elif ch == ord("]"):
					self.state = "OSC"
				elif ch == ord("P"):
					self.state = "DCS"
				else:
					self.state = "TXT"
			elif self.state == "CSI":
				self.csi.append(ch)
				if 0x40 <= ch <= 0x7E:
					if self._allow_csi(bytes(self.csi)):
						out.extend(self.csi)
					self.state = "TXT"
			elif self.state == "OSC":
				if ch == 0x07:
					self.state = "TXT"
				elif ch == 0x1B:
					self.state = "OSC_ESC"
			elif self.state == "OSC_ESC":
				if ch == ord("\\"):
					self.state = "TXT"
				else:
					self.state = "OSC"
			elif self.state == "DCS":
				if ch == 0x1B:
					self.state = "DCS_ESC"
			elif self.state == "DCS_ESC":
				if ch == ord("\\"):
					self.state = "TXT"
				else:
					self.state = "DCS"
		return bytes(out)

stdin_fd  = sys.stdin.fileno()
stdout_fd = sys.stdout.fileno()

try:
	old_tios = termios.tcgetattr(stdin_fd)
	tty.setraw(stdin_fd)
except Exception:
	pass

pid, master_fd = pty.fork()
if pid == 0:
	os.execvp(sys.argv[1], sys.argv[1:])
	os._exit(127)

sel = selectors.DefaultSelector()
try:
	sel.register(stdin_fd, selectors.EVENT_READ)
except Exception:
	pass
sel.register(master_fd, selectors.EVENT_READ)

esc_filter = EscSeqFilter()

try:
	while True:
		for key, _ in sel.select():
			if key.fd == stdin_fd:
				data = os.read(stdin_fd, 65536)
				if not data:
					os.close(master_fd); raise SystemExit
				os.write(master_fd, data)
			else:
				try:
					data = os.read(master_fd, 65536)
				except OSError as e:
					if e.errno == errno.EIO:
						os.waitpid(pid, 0); raise SystemExit
					else:
						raise
				if not data:
					os.waitpid(pid, 0); raise SystemExit
				data = esc_filter.feed(data)
				os.write(stdout_fd, data)
finally:
	try:
		termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_tios)
	except Exception:
		pass
