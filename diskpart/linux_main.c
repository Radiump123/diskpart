#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_PATH 256

typedef enum exit_code
{
    EXIT_OK = 0,
    EXIT_FATAL,
    EXIT_CMD_ARG,
    EXIT_FILE,
    EXIT_SERVICE,
    EXIT_SYNTAX,
    EXIT_EXIT
} exit_code;

typedef struct app_state
{
    char selected_disk[MAX_PATH];
    char selected_partition[MAX_PATH];
    char selected_volume[MAX_PATH];
} app_state;

static app_state g_state;

static void trim(char *s)
{
    char *start = s;
    char *end;

    while (isspace((unsigned char)*start))
        start++;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    if (*s == '\0')
        return;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;

    *(end + 1) = '\0';
}

static int split_args(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max_args)
    {
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        if (*p == '"')
        {
            p++;
            argv[argc++] = p;
            while (*p != '\0' && *p != '"')
                p++;
            if (*p == '"')
                *p++ = '\0';
            continue;
        }

        argv[argc++] = p;
        while (*p != '\0' && !isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;
        *p++ = '\0';
    }

    return argc;
}

static const char *arg_value(int argc, char *argv[], const char *key)
{
    size_t n = strlen(key);
    int i;

    for (i = 0; i < argc; i++)
    {
        if (!strncasecmp(argv[i], key, n) && argv[i][n] == '=')
            return argv[i] + n + 1;
    }

    return NULL;
}

static int run_cmd(const char *fmt, ...)
{
    char cmd[2048];
    va_list ap;
    int rc;

    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    fflush(stdout);
    rc = system(cmd);
    if (rc != 0)
        fprintf(stderr, "Command failed (%d): %s\n", rc, cmd);
    return rc;
}

static int require_root(void)
{
    if (geteuid() == 0)
        return 1;

    fputs("This command requires root privileges (run with sudo).\n", stderr);
    return 0;
}

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void resolve_dev(const char *value, char out[MAX_PATH])
{
    if (!strncmp(value, "/dev/", 5))
        snprintf(out, MAX_PATH, "%s", value);
    else
        snprintf(out, MAX_PATH, "/dev/%s", value);
}

static int resolve_indexed_device(const char *kind, int index, char out[MAX_PATH])
{
    char cmd[256];
    FILE *fp;
    char name[128];
    int current = 0;

    if (index <= 0)
        return 0;

    if (!strcasecmp(kind, "disk"))
        snprintf(cmd, sizeof(cmd), "lsblk -dn -o NAME,TYPE | awk '$2==\"disk\"{print $1}'");
    else
        snprintf(cmd, sizeof(cmd), "lsblk -ln -o NAME,TYPE | awk '$2==\"part\"{print $1}'");

    fp = popen(cmd, "r");
    if (!fp)
        return 0;

    while (fscanf(fp, "%127s", name) == 1)
    {
        current++;
        if (current == index)
        {
            pclose(fp);
            snprintf(out, MAX_PATH, "/dev/%s", name);
            return 1;
        }
    }

    pclose(fp);
    return 0;
}

static int get_disk_and_partnum(const char *part_dev, char *disk_out, size_t dsz, int *part_num)
{
    char cmd[512];
    FILE *fp;
    char pkname[128];
    int pnum;

    snprintf(cmd, sizeof(cmd), "lsblk -no PKNAME,PARTN %s 2>/dev/null", part_dev);
    fp = popen(cmd, "r");
    if (!fp)
        return 0;

    if (fscanf(fp, "%127s %d", pkname, &pnum) != 2)
    {
        pclose(fp);
        return 0;
    }

    pclose(fp);
    snprintf(disk_out, dsz, "/dev/%s", pkname);
    *part_num = pnum;
    return pnum > 0;
}

static void show_header(void)
{
    printf("\nDiskPart (Linux mode)\n");
    printf("Type 'help' for available commands.\n\n");
}

static void show_help(void)
{
    puts("Linux diskpart commands:");
    puts("  active");
    puts("  add md=<md_device> device=<member_device>");
    puts("  assign [mount=<path>]");
    puts("  attributes disk [set readonly|clear readonly]");
    puts("  attributes volume [set readonly|clear readonly]");
    puts("  automount [enable|disable]");
    puts("  break md=<md_device> device=<member_device>");
    puts("  attach vdisk file=<path>");
    puts("  clean | clean all");
    puts("  compact file=<path>");
    puts("  detach vdisk device=<loopdev>");
    puts("  dump");
    puts("  convert gpt | convert mbr");
    puts("  create partition primary [start=<MiB>] [size=<MiB>]");
    puts("  create partition efi [start=<MiB>] [size=<MiB>] (default size 100MiB)");
    puts("  create partition msr [start=<MiB>] [size=<MiB>] (default size 16MiB)");
    puts("  create vdisk file=<path> maximum=<MiB> [type=fixed|expandable]");
    puts("  delete partition [override]");
    puts("  delete volume");
    puts("  detail disk | detail partition | detail volume");
    puts("  expand [size=<MiB>] (alias of extend)");
    puts("  extend [size=<MiB>]");
    puts("  exit");
    puts("  filesystems");
    puts("  format [fs=ext4|xfs|vfat|exfat|ntfs] [label=<name>]");
    puts("  gpt attributes=<hex_mask>");
    puts("  help");
    puts("  import");
    puts("  inactive");
    puts("  list disk | list partition | list volume | list vdisk");
    puts("  merge vdisk file=<path>");
    puts("  offline disk | online disk");
    puts("  recover | repair");
    puts("  remove [mount=<path>]");
    puts("  rem <comment>");
    puts("  rescan");
    puts("  select disk <N|/dev/...>");
    puts("  select partition <N|/dev/...>");
    puts("  select volume <N|/dev/...>");
    puts("  retain | san");
    puts("  set id=<GUID>   (alias: setid id=<GUID>)");
    puts("  shrink size=<MiB>");
    puts("  uniqueid disk [id=<GUID>]");
}

static void show_help_for(const char *cmd)
{
    if (!strcasecmp(cmd, "format") || !strcasecmp(cmd, "filesystems"))
    {
        puts("format/filesystems: supported fs are ext4, xfs, vfat(fat32), exfat, ntfs");
        return;
    }

    if (!strcasecmp(cmd, "select"))
    {
        puts("select usage: select disk <N|/dev/...> | select partition <N|/dev/...> | select volume <N|/dev/...>");
        return;
    }

    if (!strcasecmp(cmd, "create"))
    {
        puts("create usage: create partition primary|efi|msr [start=<MiB>] [size=<MiB>] OR create vdisk file=<path> maximum=<MiB> [type=fixed|expandable]");
        return;
    }

    if (!strcasecmp(cmd, "clean"))
    {
        puts("clean usage: clean | clean all");
        return;
    }

    if (!strcasecmp(cmd, "delete"))
    {
        puts("delete usage: delete partition [override] | delete volume");
        return;
    }

    show_help();
}

static exit_code cmd_select(int argc, char *argv[])
{
    char dev[MAX_PATH];
    int idx;

    if (argc < 3)
        return EXIT_SYNTAX;

    if (!strcasecmp(argv[1], "disk") || !strcasecmp(argv[1], "partition") || !strcasecmp(argv[1], "volume"))
    {
        idx = atoi(argv[2]);
        if (idx > 0)
        {
            const char *kind = !strcasecmp(argv[1], "disk") ? "disk" : "partition";
            if (!resolve_indexed_device(kind, idx, dev))
            {
                fprintf(stderr, "No %s at index %d\n", argv[1], idx);
                return EXIT_FILE;
            }
        }
        else
        {
            resolve_dev(argv[2], dev);
        }

        if (!file_exists(dev))
        {
            fprintf(stderr, "Device not found: %s\n", dev);
            return EXIT_FILE;
        }

        if (!strcasecmp(argv[1], "disk"))
        {
            snprintf(g_state.selected_disk, sizeof(g_state.selected_disk), "%s", dev);
            printf("Selected disk: %s\n", g_state.selected_disk);
        }
        else if (!strcasecmp(argv[1], "partition"))
        {
            snprintf(g_state.selected_partition, sizeof(g_state.selected_partition), "%s", dev);
            printf("Selected partition: %s\n", g_state.selected_partition);
        }
        else
        {
            snprintf(g_state.selected_volume, sizeof(g_state.selected_volume), "%s", dev);
            printf("Selected volume: %s\n", g_state.selected_volume);
        }
        return EXIT_OK;
    }

    return EXIT_SYNTAX;
}

static exit_code cmd_list(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_SYNTAX;

    if (!strcasecmp(argv[1], "disk"))
        return run_cmd("lsblk -d -o NAME,SIZE,RO,TYPE,MODEL") == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(argv[1], "partition"))
    {
        if (g_state.selected_disk[0])
            return run_cmd("lsblk -ln -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS %s | awk '$3==\"part\"'", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;
        return run_cmd("lsblk -ln -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS | awk '$3==\"part\"'") == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (!strcasecmp(argv[1], "volume"))
        return run_cmd("lsblk -f") == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(argv[1], "vdisk"))
        return run_cmd("losetup -a") == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_detail(int argc, char *argv[])
{
    const char *target = NULL;

    if (argc < 2)
        return EXIT_SYNTAX;

    if (!strcasecmp(argv[1], "disk"))
        target = g_state.selected_disk;
    else if (!strcasecmp(argv[1], "partition"))
        target = g_state.selected_partition;
    else if (!strcasecmp(argv[1], "volume"))
        target = g_state.selected_volume;
    else
        return EXIT_SYNTAX;

    if (!target[0])
    {
        fprintf(stderr, "No selection for detail %s. Use select first.\n", argv[1]);
        return EXIT_SYNTAX;
    }

    return run_cmd("lsblk -O %s && blkid %s", target, target) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_active(int on)
{
    char disk[MAX_PATH];
    int partnum;

    if (!g_state.selected_partition[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
        return EXIT_SERVICE;

    return run_cmd("parted -s %s set %d boot %s", disk, partnum, on ? "on" : "off") == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_add_break(const char *verb, int argc, char *argv[])
{
    const char *md = arg_value(argc, argv, "md");
    const char *dev = arg_value(argc, argv, "device");

    if (!md || !dev)
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (!strcasecmp(verb, "add"))
        return run_cmd("mdadm --manage %s --add %s", md, dev) == 0 ? EXIT_OK : EXIT_SERVICE;

    return run_cmd("mdadm --manage %s --fail %s --remove %s", md, dev, dev) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_assign(int argc, char *argv[])
{
    const char *mount_point = arg_value(argc, argv, "mount");

    if (!g_state.selected_volume[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (!mount_point)
        mount_point = "/mnt/diskpart";

    run_cmd("mkdir -p '%s'", mount_point);
    return run_cmd("mount %s '%s'", g_state.selected_volume, mount_point) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_remove(int argc, char *argv[])
{
    const char *mount_point = arg_value(argc, argv, "mount");

    if (!require_root())
        return EXIT_SERVICE;

    if (mount_point)
        return run_cmd("umount '%s'", mount_point) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (g_state.selected_volume[0])
        return run_cmd("umount %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_attributes(int argc, char *argv[])
{
    const char *target;

    if (argc < 2)
        return EXIT_SYNTAX;

    if (!strcasecmp(argv[1], "disk"))
        target = g_state.selected_disk;
    else if (!strcasecmp(argv[1], "volume"))
        target = g_state.selected_volume;
    else
        return EXIT_SYNTAX;

    if (!target[0])
        return EXIT_SYNTAX;

    if (argc == 2)
        return run_cmd("lsblk -o NAME,RO %s", target) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!require_root())
        return EXIT_SERVICE;

    if (argc >= 4 && !strcasecmp(argv[2], "set") && !strcasecmp(argv[3], "readonly"))
        return run_cmd("blockdev --setro %s", target) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (argc >= 4 && !strcasecmp(argv[2], "clear") && !strcasecmp(argv[3], "readonly"))
        return run_cmd("blockdev --setrw %s", target) == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_automount(int argc, char *argv[])
{
    if (argc == 1)
        return run_cmd("systemctl is-enabled udisks2.service || true") == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!require_root())
        return EXIT_SERVICE;

    if (!strcasecmp(argv[1], "enable"))
        return run_cmd("systemctl enable --now udisks2.service") == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(argv[1], "disable"))
        return run_cmd("systemctl disable --now udisks2.service") == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_clean(int argc, char *argv[])
{
    if (!g_state.selected_disk[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (argc >= 2 && !strcasecmp(argv[1], "all"))
    {
        if (run_cmd("blkdiscard -f %s", g_state.selected_disk) == 0)
            return EXIT_OK;
        return run_cmd("dd if=/dev/zero of=%s bs=16M status=progress conv=fsync", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (run_cmd("wipefs -a %s", g_state.selected_disk) != 0)
        return EXIT_SERVICE;

    run_cmd("sgdisk -Z %s", g_state.selected_disk);
    run_cmd("partprobe %s", g_state.selected_disk);
    return EXIT_OK;
}

static exit_code cmd_convert(int argc, char *argv[])
{
    if (argc < 2 || !g_state.selected_disk[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (!strcasecmp(argv[1], "gpt"))
        return run_cmd("parted -s %s mklabel gpt", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(argv[1], "mbr"))
        return run_cmd("parted -s %s mklabel msdos", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_create(int argc, char *argv[])
{
    const char *size;
    const char *start;

    if (argc < 2)
        return EXIT_SYNTAX;

    if (!strcasecmp(argv[1], "vdisk"))
    {
        const char *file = arg_value(argc, argv, "file");
        const char *maximum = arg_value(argc, argv, "maximum");
        const char *type = arg_value(argc, argv, "type");

        if (!file || !maximum)
            return EXIT_SYNTAX;

        if (type && !strcasecmp(type, "fixed"))
            return run_cmd("dd if=/dev/zero of='%s' bs=1M count=%s status=none", file, maximum) == 0 ? EXIT_OK : EXIT_SERVICE;

        return run_cmd("truncate -s %sM '%s'", maximum, file) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (argc < 3)
        return EXIT_SYNTAX;

    if (strcasecmp(argv[1], "partition") != 0)
        return EXIT_SYNTAX;

    if (!g_state.selected_disk[0] || !require_root())
        return EXIT_SERVICE;

    size = arg_value(argc, argv, "size");
    start = arg_value(argc, argv, "start");

    if (!strcasecmp(argv[2], "primary"))
    {
        const char *s = start ? start : "1";
        char end[32];
        if (size)
            snprintf(end, sizeof(end), "%dMiB", atoi(s) + atoi(size));
        else
            snprintf(end, sizeof(end), "100%%");
        return run_cmd("parted -s %s mkpart primary %sMiB %s", g_state.selected_disk, s, end) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (!strcasecmp(argv[2], "efi"))
    {
        const char *s = start ? start : "1";
        int sz = size ? atoi(size) : 100;
        int end = atoi(s) + sz;
        if (run_cmd("parted -s %s mkpart ESP fat32 %sMiB %dMiB", g_state.selected_disk, s, end) != 0)
            return EXIT_SERVICE;
        return run_cmd("parted -s %s set 1 esp on", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (!strcasecmp(argv[2], "msr"))
    {
        const char *s = start ? start : "101";
        int sz = size ? atoi(size) : 16;
        int end = atoi(s) + sz;
        return run_cmd("parted -s %s mkpart msr %sMiB %dMiB", g_state.selected_disk, s, end) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    return EXIT_SYNTAX;
}

static exit_code cmd_delete(int argc, char *argv[])
{
    char disk[MAX_PATH];
    int partnum;

    if (argc < 2)
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (!strcasecmp(argv[1], "partition"))
    {
        if (!g_state.selected_partition[0])
            return EXIT_SYNTAX;
        if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
            return EXIT_SERVICE;
        return run_cmd("parted -s %s rm %d", disk, partnum) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (!strcasecmp(argv[1], "volume"))
    {
        if (!g_state.selected_volume[0])
            return EXIT_SYNTAX;
        if (!get_disk_and_partnum(g_state.selected_volume, disk, sizeof(disk), &partnum))
            return EXIT_SERVICE;
        return run_cmd("parted -s %s rm %d", disk, partnum) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    return EXIT_SYNTAX;
}

static exit_code cmd_extend(int argc, char *argv[])
{
    char disk[MAX_PATH];
    int partnum;
    const char *size;

    if (!g_state.selected_partition[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
        return EXIT_SERVICE;

    size = arg_value(argc, argv, "size");
    if (size)
        return run_cmd("parted -s %s resizepart %d %sMiB", disk, partnum, size) == 0 ? EXIT_OK : EXIT_SERVICE;

    return run_cmd("parted -s %s resizepart %d 100%%", disk, partnum) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_shrink(int argc, char *argv[])
{
    char disk[MAX_PATH];
    int partnum;
    const char *size = arg_value(argc, argv, "size");

    if (!size)
        return EXIT_SYNTAX;
    if (!g_state.selected_partition[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
        return EXIT_SERVICE;

    return run_cmd("parted -s %s resizepart %d %sMiB", disk, partnum, size) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_filesystems(void)
{
    puts("Supported filesystems:");
    puts("  ext4");
    puts("  xfs");
    puts("  vfat (fat32)");
    puts("  exfat");
    puts("  ntfs");
    return EXIT_OK;
}

static exit_code cmd_format(int argc, char *argv[])
{
    const char *fs = arg_value(argc, argv, "fs");
    const char *label = arg_value(argc, argv, "label");

    if (!g_state.selected_volume[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    if (!fs)
        fs = "ext4";

    if (!strcasecmp(fs, "ext4"))
    {
        if (label)
            return run_cmd("mkfs.ext4 -F -L '%s' %s", label, g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;
        return run_cmd("mkfs.ext4 -F %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (!strcasecmp(fs, "xfs"))
        return run_cmd("mkfs.xfs -f %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(fs, "vfat") || !strcasecmp(fs, "fat32"))
        return run_cmd("mkfs.vfat %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(fs, "exfat"))
        return run_cmd("mkfs.exfat %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!strcasecmp(fs, "ntfs"))
        return run_cmd("mkfs.ntfs -F %s", g_state.selected_volume) == 0 ? EXIT_OK : EXIT_SERVICE;

    return EXIT_SYNTAX;
}

static exit_code cmd_gpt(int argc, char *argv[])
{
    const char *attributes = arg_value(argc, argv, "attributes");
    char disk[MAX_PATH];
    int partnum;

    if (!attributes)
        return EXIT_SYNTAX;
    if (!g_state.selected_partition[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
        return EXIT_SERVICE;

    return run_cmd("sgdisk --attributes=%d:set:%s %s", partnum, attributes, disk) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_setid(int argc, char *argv[])
{
    const char *id = arg_value(argc, argv, "id");
    char disk[MAX_PATH];
    int partnum;

    if (!id)
        return EXIT_SYNTAX;
    if (!g_state.selected_partition[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    if (!get_disk_and_partnum(g_state.selected_partition, disk, sizeof(disk), &partnum))
        return EXIT_SERVICE;

    return run_cmd("sgdisk --typecode=%d:%s %s", partnum, id, disk) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_uniqueid(int argc, char *argv[])
{
    const char *id = arg_value(argc, argv, "id");

    if (argc < 2 || strcasecmp(argv[1], "disk") != 0)
        return EXIT_SYNTAX;
    if (!g_state.selected_disk[0])
        return EXIT_SYNTAX;

    if (!id)
        return run_cmd("lsblk -no NAME,PTUUID %s", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;

    if (!require_root())
        return EXIT_SERVICE;

    return run_cmd("sgdisk --disk-guid=%s %s", id, g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_offline_online(int online)
{
    if (!g_state.selected_disk[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;

    return run_cmd("blockdev %s %s", online ? "--setrw" : "--setro", g_state.selected_disk) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_rescan(void)
{
    if (!require_root())
        return EXIT_SERVICE;

    run_cmd("partprobe || true");
    run_cmd("udevadm trigger --subsystem-match=block || true");
    run_cmd("udevadm settle || true");
    return EXIT_OK;
}

static exit_code cmd_compact_merge(int argc, char *argv[])
{
    if (!strcasecmp(argv[0], "compact"))
    {
        const char *file = arg_value(argc, argv, "file");
        if (!file)
            return EXIT_SYNTAX;
        if (run_cmd("command -v qemu-img >/dev/null 2>&1") != 0)
            return EXIT_SERVICE;
        return run_cmd("qemu-img convert -O qcow2 '%s' '%s.compact.qcow2'", file, file) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (argc >= 2 && !strcasecmp(argv[0], "merge") && !strcasecmp(argv[1], "vdisk"))
    {
        const char *file = arg_value(argc, argv, "file");
        if (!file)
            return EXIT_SYNTAX;
        if (run_cmd("command -v qemu-img >/dev/null 2>&1") != 0)
            return EXIT_SERVICE;
        return run_cmd("qemu-img commit '%s'", file) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    return EXIT_SYNTAX;
}


static exit_code cmd_attach_detach(int argc, char *argv[], int attach)
{
    if (attach)
    {
        const char *file = arg_value(argc, argv, "file");
        if (!file)
            return EXIT_SYNTAX;
        if (!require_root())
            return EXIT_SERVICE;
        return run_cmd("losetup --find --show '%s'", file) == 0 ? EXIT_OK : EXIT_SERVICE;
    }

    if (argc < 2 || strcasecmp(argv[1], "vdisk") != 0)
        return EXIT_SYNTAX;

    {
        const char *dev = arg_value(argc, argv, "device");
        if (!dev)
            return EXIT_SYNTAX;
        if (!require_root())
            return EXIT_SERVICE;
        return run_cmd("losetup -d %s", dev) == 0 ? EXIT_OK : EXIT_SERVICE;
    }
}

static exit_code cmd_repair_recover(void)
{
    const char *target = g_state.selected_volume[0] ? g_state.selected_volume : g_state.selected_partition;
    if (!target[0])
        return EXIT_SYNTAX;
    if (!require_root())
        return EXIT_SERVICE;
    return run_cmd("fsck -fy %s", target) == 0 ? EXIT_OK : EXIT_SERVICE;
}

static exit_code cmd_not_supported(const char *name)
{
    printf("Command '%s' is recognized but not implemented yet on Linux.\n", name);
    return EXIT_OK;
}

static exit_code cmd_import(void)
{
    run_cmd("mdadm --assemble --scan || true");
    run_cmd("pvscan || true");
    run_cmd("vgscan || true");
    return EXIT_OK;
}


static int is_known_windows_command(const char *cmd)
{
    const char *known[] = {
        "active","add","assign","attach","attributes","automount","break","clean","compact",
        "convert","create","delete","detach","detail","dump","expand","extend","filesystems",
        "format","gpt","help","import","inactive","list","merge","offline","online","recover",
        "remove","repair","rescan","retain","san","select","set","setid","shrink","uniqueid",
        "exit","rem",NULL};
    int i;
    for (i = 0; known[i] != NULL; i++)
    {
        if (!strcasecmp(cmd, known[i]))
            return 1;
    }
    return 0;
}

static exit_code run_command(char *line)
{
    char *argv[MAX_ARGS];
    int argc;

    trim(line);
    if (line[0] == '\0' || line[0] == '#')
        return EXIT_OK;

    argc = split_args(line, argv, MAX_ARGS);
    if (argc <= 0)
        return EXIT_OK;

    if (!strcasecmp(argv[0], "rem"))
        return EXIT_OK;
    if (!strcasecmp(argv[0], "exit"))
        return EXIT_EXIT;
    if (!strcasecmp(argv[0], "help") || !strcasecmp(argv[0], "?"))
    {
        if (argc >= 2)
            show_help_for(argv[1]);
        else
            show_help();
        return EXIT_OK;
    }

    if (!strcasecmp(argv[0], "select")) return cmd_select(argc, argv);
    if (!strcasecmp(argv[0], "list")) return cmd_list(argc, argv);
    if (!strcasecmp(argv[0], "detail")) return cmd_detail(argc, argv);
    if (!strcasecmp(argv[0], "active")) return cmd_active(1);
    if (!strcasecmp(argv[0], "inactive")) return cmd_active(0);
    if (!strcasecmp(argv[0], "add")) return cmd_add_break("add", argc, argv);
    if (!strcasecmp(argv[0], "break")) return cmd_add_break("break", argc, argv);
    if (!strcasecmp(argv[0], "assign")) return cmd_assign(argc, argv);
    if (!strcasecmp(argv[0], "attributes")) return cmd_attributes(argc, argv);
    if (!strcasecmp(argv[0], "automount")) return cmd_automount(argc, argv);
    if (!strcasecmp(argv[0], "clean")) return cmd_clean(argc, argv);
    if (!strcasecmp(argv[0], "compact") || !strcasecmp(argv[0], "merge")) return cmd_compact_merge(argc, argv);
    if (!strcasecmp(argv[0], "convert")) return cmd_convert(argc, argv);
    if (!strcasecmp(argv[0], "create")) return cmd_create(argc, argv);
    if (!strcasecmp(argv[0], "delete")) return cmd_delete(argc, argv);
    if (!strcasecmp(argv[0], "dump")) return cmd_not_supported("dump");
    if (!strcasecmp(argv[0], "expand")) return cmd_extend(argc, argv);
    if (!strcasecmp(argv[0], "extend")) return cmd_extend(argc, argv);
    if (!strcasecmp(argv[0], "filesystems")) return cmd_filesystems();
    if (!strcasecmp(argv[0], "format")) return cmd_format(argc, argv);
    if (!strcasecmp(argv[0], "gpt")) return cmd_gpt(argc, argv);
    if (!strcasecmp(argv[0], "import")) return cmd_import();
    if (!strcasecmp(argv[0], "attach")) return cmd_attach_detach(argc, argv, 1);
    if (!strcasecmp(argv[0], "detach")) return cmd_attach_detach(argc, argv, 0);
    if (!strcasecmp(argv[0], "offline") && argc >= 2 && !strcasecmp(argv[1], "disk")) return cmd_offline_online(0);
    if (!strcasecmp(argv[0], "online") && argc >= 2 && !strcasecmp(argv[1], "disk")) return cmd_offline_online(1);
    if (!strcasecmp(argv[0], "recover") || !strcasecmp(argv[0], "repair")) return cmd_repair_recover();
    if (!strcasecmp(argv[0], "remove")) return cmd_remove(argc, argv);
    if (!strcasecmp(argv[0], "rescan")) return cmd_rescan();
    if (!strcasecmp(argv[0], "retain") || !strcasecmp(argv[0], "san")) return cmd_not_supported(argv[0]);
    if (!strcasecmp(argv[0], "set") || !strcasecmp(argv[0], "setid")) return cmd_setid(argc, argv);
    if (!strcasecmp(argv[0], "shrink")) return cmd_shrink(argc, argv);
    if (!strcasecmp(argv[0], "uniqueid")) return cmd_uniqueid(argc, argv);

    if (is_known_windows_command(argv[0]))
        return cmd_not_supported(argv[0]);

    fprintf(stderr, "Unknown command: %s\n", argv[0]);
    return EXIT_SYNTAX;
}

static int run_script(const char *filename)
{
    FILE *script;
    char line[MAX_LINE];
    exit_code result;

    script = fopen(filename, "r");
    if (!script)
    {
        fprintf(stderr, "Could not open script '%s': %s\n", filename, strerror(errno));
        return EXIT_FILE;
    }

    while (fgets(line, sizeof(line), script))
    {
        result = run_command(line);
        if (result != EXIT_OK)
        {
            fclose(script);
            return (result == EXIT_EXIT) ? EXIT_OK : result;
        }
    }

    fclose(script);
    return EXIT_OK;
}

static void run_interactive(void)
{
    char line[MAX_LINE];

    for (;;)
    {
        fputs("DISKPART> ", stdout);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        if (run_command(line) == EXIT_EXIT)
            break;
    }
}

int main(int argc, char **argv)
{
    const char *script = NULL;
    int timeout = 0;
    int i;

    memset(&g_state, 0, sizeof(g_state));

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-' && argv[i][0] != '/')
        {
            fprintf(stderr, "Invalid argument: %s\n", argv[i]);
            return EXIT_SYNTAX;
        }

        if (!strcasecmp(argv[i] + 1, "?") || !strcasecmp(argv[i] + 1, "h"))
        {
            puts("Usage: diskpart [-s <script>] [-t <seconds>]\n");
            show_help();
            return EXIT_OK;
        }
        else if (!strcasecmp(argv[i] + 1, "s"))
        {
            if (i + 1 >= argc)
            {
                fputs("Missing value for -s\n", stderr);
                return EXIT_CMD_ARG;
            }
            script = argv[++i];
        }
        else if (!strcasecmp(argv[i] + 1, "t"))
        {
            if (i + 1 >= argc)
            {
                fputs("Missing value for -t\n", stderr);
                return EXIT_CMD_ARG;
            }
            timeout = atoi(argv[++i]);
            if (timeout < 0)
                timeout = 0;
        }
        else
        {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            return EXIT_SYNTAX;
        }
    }

    show_header();

    if (timeout > 0)
        sleep((unsigned int)timeout);

    if (script)
        return run_script(script);

    run_interactive();
    return EXIT_OK;
}
