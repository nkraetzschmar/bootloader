package main

import (
	"fmt"
	"os"
	"reflect"
	"strconv"

	"github.com/diskfs/go-diskfs"
	"github.com/diskfs/go-diskfs/filesystem/fat32"
	"github.com/diskfs/go-diskfs/partition/gpt"
)

const stage2_lba = 0x22
const first_part_min = 0x40

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <disk> <partition>\n", os.Args[0])
		os.Exit(1)
	}

	disk_path := os.Args[1]
	partition_idx, err := strconv.ParseUint(os.Args[2], 10, 0)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Invalid partition number \"%s\": %v\n", os.Args[2], err)
		os.Exit(1)
	}

	disk, err := diskfs.Open(disk_path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to open disk \"%s\": %v\n", disk_path, err)
		os.Exit(1)
	}

	part_table, err := disk.GetPartitionTable()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to read partition table: %v\n", err)
		os.Exit(1)
	}

	if part_table.Type() != "gpt" {
		fmt.Fprintf(os.Stderr, "Not a GPT partition table\n")
		os.Exit(1)
	}

	gpt_table := part_table.(*gpt.Table)

	if gpt_table.LogicalSectorSize != 512 {
		fmt.Fprintf(os.Stderr, "Non standard sector size of %d detected. Must be 512\n", gpt_table.LogicalSectorSize)
		os.Exit(1)
	}

	gpt_table_ref := reflect.ValueOf(gpt_table).Elem()
	partition_array_size := uint64(gpt_table_ref.FieldByName("partitionArraySize").Int())
	partition_entry_size := gpt_table_ref.FieldByName("partitionEntrySize").Uint()
	gpt_lba := gpt_table_ref.FieldByName("partitionFirstLBA").Uint()

	data_lba := gpt_lba + (partition_array_size*partition_entry_size)/512
	if data_lba > stage2_lba {
		fmt.Fprintf(os.Stderr, "GPT table extends beyond sector %d, this is unsupported\n", stage2_lba)
		os.Exit(1)
	}

	for index, partition := range gpt_table.Partitions {
		if partition.Start <= first_part_min {
			fmt.Fprintf(os.Stderr, "Partition %d @%d starts before minimum sector %d, this is unsupported\n", index+1, partition.Start, first_part_min)
		}
	}

	if len(gpt_table.Partitions) < int(partition_idx)+1 {
		fmt.Fprintf(os.Stderr, "Partiton %d not found on disk\n", partition_idx)
		os.Exit(1)
	}

	partition := gpt_table.Partitions[partition_idx]

	file_system, err := fat32.Read(disk.File, partition.GetSize(), partition.GetStart(), 512)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to load FAT32 file system on partition %d: %v\n", partition_idx, err)
		os.Exit(1)
	}

	file, err := file_system.OpenFile("/EFI/Linux/test-boot-entry.efi", os.O_RDONLY)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to open file: %v\n", err)
		os.Exit(1)
	}

	fat32_file := file.(*fat32.File)

	disk_ranges, err := fat32_file.GetDiskRanges()
	if err != nil {
		panic(err)
	}

	type SectorRange struct {
		Start  uint32
		Length uint32
	}

	sector_ranges := make([]SectorRange, len(disk_ranges))
	for i, disk_range := range disk_ranges {
		sector_ranges[i] = SectorRange{
			Start:  uint32(disk_range.Offset/512) + uint32(partition.Start),
			Length: uint32(disk_range.Length / 512),
		}
	}

	fmt.Printf("%#v\n", sector_ranges)
}
