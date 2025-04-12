# Legacy Compatible UKIs

This doc outlines the design for legacy BIOS compatible UKIs (Unified Kernel Images).
A legacy compatible UKI is one that is both a valid EFI binary and a valid bzImage; where executing it as either an EFI binary or a bzImage should result in the same kernel, initrd, and cmdline to be booted.

EFI binaries start with a DOS header, of which only the `e_lfanew` field is important for EFI binaries.
This field is a pointer to the PE/COFF header, which is the real EFI binary header.

This structure allows for arbitrary distance between the start of the file and the PE header.
We leaverage this to embedd a linux kernel compatible header here as well as some tiny stub code.
The kernel header is setup such that the PE/COFF header and some additional loader code after it are considered the real mode code, while all the rest of the EFI binary, so all its sections, is considered the kernels protected mode code.

A legacy bootloader would then load the stub code, the loader code, and the PE/COFF header into low mem and the rest of the EFI binary into high mem.
From there the loader code can parse the PE header to relocate the EFI sections accordingly and execute the kernel.

## File Layout

```
0x0000 - 0x0040 DOS Header
0x01f1 - 0x026c Stub Kernel Header
0x0280 - 0x0400 Stub Kernel
0x0400 - 0x1000 PE/COFF Header
0x1000 - 0x2000 Loader Code
0x2000 - ?      PE section contents
```

The actual sections within the EFI binary should follow the [UKI spec](https://uapi-group.org/specifications/specs/unified_kernel_image/).

> [!CAUTION]
> The `syssize` field of the stub kernel header must be kept up to date when the size of the EFI binary is increased (i.e. when adding sections) and when increasing the PE header size one must take care not to overflow into the legacy loader code.

## Version String

The version string pointed to in the stub kernel header should be set to `UKI:${version}` where the version is the the same as specified in the `.osrel` section of the UKI.
This allows legacy bootloaders to extract the correct version info without having to implement EFI parsing directly.
