# Bootloader

Implementation of the [UAPI Boot Loader Specification](https://uapi-group.org/specifications/specs/boot_loader_specification/) for legacy BIOS boot.

## Goals

- [x] [Basic bootloader able to load and exec Linux](https://github.com/nkraetzschmar/bootloader/issues/1)
- [ ] [Support Type 1 Boot Loader Specification Entries](https://github.com/nkraetzschmar/bootloader/issues/10)
- [ ] [Support Type 2 EFI Unified Kernel Images](https://github.com/nkraetzschmar/bootloader/issues/11)
- [ ] [Support Boot Counting](https://github.com/nkraetzschmar/bootloader/issues/12)
- [ ] [Make UKI EFI shim usable for kexec](https://github.com/nkraetzschmar/bootloader/issues/13)

## Architecture

The bootloader is designed with a 2 stage architecture.
Stage1 is only responsible for loading stage 2, which in turn implements the core functionality.

### Stage 1 (MBR)

Stage 1 is the code directly in the MBR.
It does some basic CPU register initialization, loads stage 2 from a fixed location on disk into memory, and jumps to the stage 2 entry point.

### Stage 2 (bootloader)

Stage 2 is the core of the bootloader.
It handles loading and executing the Linux kernel.
It is stored in sectors 34-65 on disk (16KiB).

Conventionally these sectors on disk are unused, as the GPT partition table occupies the first 33 sectors, but the first data partition only starts at the 1MiB offset (sector 2048).
Therefore in most common disk setups it should be perfectly fine to use this space.

### Memory Layout

```
=== low mem ===

         ? - 0x00007c00 stack (grows down dynamically)
0x00007c00 - 0x00007dbe MBR (stage 1)

0x00008000 - 0x0000c000 bootloader (stage 2)

0x00010000 - 0x00018000 real mode kernel code
0x00018000 - 0x0001f000 real mode kernel heap
0x0001f000 - 0x0001f800 kernel cmdline

0x00040000 - 0x00048000 I/O buffer

=== high mem ===

0x00100000 - ?          protected mode kernel code
0x04000000 - ?          initrd
```

## Bootloader Emulator

The build target `bootloader_emu` compiles the bootloader sources into a regular userspace executable.
This allows for testing/debugging application logic of the bootloader without running it in a full VM.
