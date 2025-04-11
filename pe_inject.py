#!/usr/bin/env python3

import pefile
import sys
import subprocess

def align(size, alignment):
	return (size + alignment - 1) & ~(alignment - 1)

def get_version(pe):
	os_rel = ""

	for section in pe.sections:
		if section.Name.decode("utf-8").split("\x00")[0] == ".osrel":
			os_rel = section.get_data().decode("utf-8").split("\x00")[0]

	for line in os_rel.split("\n"):
		if "=" not in line:
			continue
		key, value = line.split("=")
		if key == "VERSION":
			return value

	return ""


target_header = 0x0400
target_loader = 0x1000
target_data = 0x2000

input_pe = sys.argv[1]
input_stub = sys.argv[2]
input_loader = sys.argv[3]
output_pe = sys.argv[4]

with open(input_stub, "rb") as f:
	stub_data = f.read()

if len(stub_data) != 0x0400:
	print("Error: stub must be 512 bytes.")
	sys.exit(1)

with open(input_loader, "rb") as f:
	loader_data = f.read()

if len(loader_data) != 0x1000:
	print("Error: loader must be 4K.")
	sys.exit(1)

pe = pefile.PE(input_pe)

if hasattr(pe, "DIRECTORY_ENTRY_DEBUG"):
	print("Error: debug info present, will not patch.")
	sys.exit(1)

security = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_SECURITY']]
if security.VirtualAddress != 0:
	print("Error: security directory present, will not patch.")
	sys.exit(1)

if target_data % pe.OPTIONAL_HEADER.FileAlignment != 0:
	print("Error: input FileAlignment unsupported.")
	sys.exit(1)

input_header = pe.DOS_HEADER.e_lfanew
input_data = pe.OPTIONAL_HEADER.SizeOfHeaders

version = get_version(pe)

pe_header_size = 0x0018 + pe.FILE_HEADER.SizeOfOptionalHeader + pe.FILE_HEADER.NumberOfSections * 0x0028
target_header_size = align(target_header + pe_header_size, pe.OPTIONAL_HEADER.FileAlignment)

if target_header_size > target_loader:
	print("Error: PE header too large.")
	sys.exit(1)

if target_header_size > pe.OPTIONAL_HEADER.BaseOfCode:
	print("Error: not enough space in virtual memory layout to fit header.")
	sys.exit(1)

data_offset = target_data - input_data

pe.DOS_HEADER.e_lfanew = 0x0400
pe.FILE_HEADER.PointerToSymbolTable += data_offset
pe.OPTIONAL_HEADER.SizeOfHeaders = target_header_size

for section in pe.sections:
	section.PointerToRawData += data_offset
	assert section.PointerToRawData % pe.OPTIONAL_HEADER.FileAlignment == 0

patched_pe = pe.write()

output_data = bytearray()

output_data += patched_pe[:0x0040]
output_data += stub_data[0x0040:0x02c0]
output_data += ("UKI:" + version).encode("utf-8")
output_data += b'\x00' * (0x0300 - len(output_data))
output_data += stub_data[0x0300:]
output_data += patched_pe[input_header:input_header+pe_header_size]
output_data += b'\x00' * (target_loader - len(output_data))
output_data += loader_data
output_data += patched_pe[input_data:]

data_size = len(output_data) - target_data
sys_size = (data_size + 0x0f) // 0x10
output_data[0x01f4:0x01f8] = sys_size.to_bytes(4, byteorder='little')

with open(output_pe, "wb") as f:
	f.write(output_data)

subprocess.run(["x86_64-linux-gnu-objdump", "-fph", output_pe])
