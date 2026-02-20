# diskpart

Linux-focused DiskPart-compatible CLI.

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

The Linux implementation provides working equivalents for the requested command set:

- selection/context: `select disk X`, `select partition X`, `select volume X`
- inventory/details: `list disk|partition|volume`, `detail disk|partition|volume`, `rescan`
- partition flags: `active`, `inactive`, `set id=GUID`, `gpt attributes=...`, `uniqueid disk [id=...]`
- disk label/layout: `clean`, `clean all`, `convert gpt`, `convert mbr`, `create partition primary|efi|msr`, `delete partition [override]`, `delete volume`, `extend`, `shrink`
- filesystem/mount: `format`, `assign`, `remove`, `automount`
- RAID/virtual-disk equivalents: `add`, `break`, `create vdisk`, `compact`, `merge vdisk`, `import`
- shell behavior: `help`, `help <command>`, `filesystems`, `rem`, `exit`

## Tooling used under Linux

This command maps to standard Linux utilities. Install the relevant packages for your distro:

- core: `lsblk`, `blkid`, `parted`, `partprobe`, `wipefs`, `blockdev`, `mount`, `umount`
- GPT extras: `sgdisk` (package: `gdisk`)
- filesystem tools: `mkfs.ext4`, `mkfs.xfs`, `mkfs.vfat`, `mkfs.exfat`, `mkfs.ntfs`
- optional: `mdadm` (RAID), `qemu-img` (vdisk compact/merge), `systemctl`/`udisks2` (automount)

Most mutating commands require root.
