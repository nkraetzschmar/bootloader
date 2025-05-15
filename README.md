# Bootloader

Implementation of the [UAPI Boot Loader Specification](https://uapi-group.org/specifications/specs/boot_loader_specification/) for legacy BIOS boot.

## Goals

- [x] [Basic bootloader able to load and exec Linux](https://github.com/nkraetzschmar/bootloader/issues/1)
- [x] [Support Type 1 Boot Loader Specification Entries](https://github.com/nkraetzschmar/bootloader/issues/10)
- [x] [Support Type 2 EFI Unified Kernel Images](https://github.com/nkraetzschmar/bootloader/issues/11)
- [ ] [Support Boot Counting](https://github.com/nkraetzschmar/bootloader/issues/12)
- [x] [Make UKI stub usable for kexec](https://github.com/nkraetzschmar/bootloader/issues/13)

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
0x0000c000 - 0x00010000 bootloader BSS area
0x00010000 - 0x00018000 real mode kernel code
0x00018000 - 0x0001f000 real mode kernel heap
0x0001f000 - 0x0001f800 kernel cmdline

0x00040000 - 0x00050000 I/O buffer

=== high mem ===

0x00100000 - ?          protected mode kernel code
0x04000000 - ?          initrd
```

## Type 2 Bootloader Entries

Type 2 bootloader entries, aka. UKIs (Unified Kernel Images) are EFI binaries that contain all that's needed to boot a given entry, so the kernel, the initrd, and the kernel cmdline, along with an EFI stub loader.
Since they require an EFI firmware environment to execute the EFI stub this obviously can not directly be one-to-one ported to legacy BIOS.

Theoretically one could setup a minimal fake EFI environment, just spec compliant enough to make the stub code happy, but that would require significant effort, be quite likely to break with newer stub versions, and in my opinion misses the point of UKIs.
I'd argue having an EFI stub isn't the crucial part of a UKI, instead the point is to have a self contained binary that when executed loads a kernel image plus some additional components into memory and executes the kernel.

Now this is in fact doable in legacy BIOS: to achieve this we create UKIs that, similar to the Linux kernel (at least when `CONFIG_EFI_STUB=y`), are both valid EFI binaries and valid bzImages.
The entry point of this bzImage, however, is not directly the kernel and instead a small legacy stub that handles loading of the real kernel, initrd, etc - mirroring what the EFI stub does.

The exact details of this modified, legacy compatible, UKI format are described in [legacy_uki.md](./legacy_uki.md).
This format also provides basic [kexec support](./legacy_uki.md#kexec-support) for UKIs.

## Bootloader Emulator

The build target `bootloader_emu` compiles the bootloader sources into a regular userspace executable.
This allows for testing/debugging application logic of the bootloader without running it in a full VM.
