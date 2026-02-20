# diskpart (Linux-only)

This repository now targets **Linux only**. The executable is implemented in `diskpart/linux_main.c` and uses Linux tooling/APIs for disk operations.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/diskpart/diskpart
```

## Install

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

## Linux command support

- selection/context: `select disk X`, `select partition X`, `select volume X`
- inventory/details: `list disk|partition|volume`, `detail disk|partition|volume`, `rescan`
- partition flags: `active`, `inactive`, `set id=GUID`, `gpt attributes=...`, `uniqueid disk [id=...]`
- disk label/layout: `clean`, `clean all`, `convert gpt`, `convert mbr`, `create partition primary|efi|msr`, `delete partition [override]`, `delete volume`, `extend`, `shrink`
- filesystem/mount: `filesystems`, `format`, `assign`, `remove`, `automount`
- RAID/virtual-disk equivalents: `add`, `break`, `create vdisk`, `compact`, `merge vdisk`, `import`
- shell behavior: `help`, `help <command>`, `rem`, `exit`

## Required Linux tools

- core: `lsblk`, `blkid`, `parted`, `partprobe`, `wipefs`, `blockdev`, `mount`, `umount`
- GPT extras: `sgdisk` (package: `gdisk`)
- filesystem tools: `mkfs.ext4`, `mkfs.xfs`, `mkfs.vfat`, `mkfs.exfat`, `mkfs.ntfs`
- optional: `mdadm` (RAID), `qemu-img` (vdisk compact/merge), `systemctl`/`udisks2` (automount)

Most mutating commands require root.
