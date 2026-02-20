#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 32

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

typedef struct command_desc
{
    const char *name;
    const char *status;
} command_desc;

static const command_desc g_command_catalog[] = {
    {"active", "stub"},
    {"add", "stub"},
    {"assign", "stub"},
    {"attach", "stub"},
    {"attributes", "stub"},
    {"automount", "stub"},
    {"break", "stub"},
    {"clean", "stub"},
    {"compact", "stub"},
    {"convert", "stub"},
    {"create", "stub"},
    {"delete", "stub"},
    {"detach", "stub"},
    {"detail", "stub"},
    {"dump", "stub"},
    {"exit", "implemented"},
    {"expand", "stub"},
    {"extend", "stub"},
    {"filesystems", "stub"},
    {"format", "stub"},
    {"gpt", "stub"},
    {"help", "implemented"},
    {"import", "stub"},
    {"inactive", "stub"},
    {"list", "implemented (disk/volume/partition)"},
    {"merge", "stub"},
    {"offline", "stub"},
    {"online", "stub"},
    {"recover", "stub"},
    {"rem", "implemented (comment)"},
    {"remove", "stub"},
    {"repair", "stub"},
    {"rescan", "stub"},
    {"retain", "stub"},
    {"san", "stub"},
    {"select", "stub"},
    {"set", "stub"},
    {"setid", "stub"},
    {"shrink", "stub"},
    {"uniqueid", "stub"},
    {NULL, NULL}};

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

        argv[argc++] = p;

        while (*p != '\0' && !isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        *p = '\0';
        p++;
    }

    return argc;
}

static void show_header(void)
{
    printf("\nDiskPart (Linux compatibility mode)\n");
    printf("Copyright (C) ReactOS contributors\n");
    printf("Type 'help' for available commands.\n\n");
}

static void show_help(void)
{
    const command_desc *cmd;

    puts("Available commands (Linux compatibility mode):");
    for (cmd = g_command_catalog; cmd->name != NULL; cmd++)
        printf("  %-12s %s\n", cmd->name, cmd->status);
}

static void list_disk(void)
{
    DIR *d = opendir("/sys/block");
    struct dirent *ent;

    if (d == NULL)
    {
        fprintf(stderr, "Unable to read /sys/block: %s\n", strerror(errno));
        return;
    }

    puts("Disks:");
    while ((ent = readdir(d)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        if (!strncmp(ent->d_name, "loop", 4) || !strncmp(ent->d_name, "ram", 3))
            continue;

        printf("  %s\n", ent->d_name);
    }

    closedir(d);
}

static void list_volume(void)
{
    int rc;

    fflush(stdout);
    rc = system("lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS");
    if (rc != 0)
        fprintf(stderr, "Failed to run lsblk (exit code %d).\n", rc);
}

static void list_partition(void)
{
    int rc;

    fflush(stdout);
    rc = system("lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS | awk 'NR==1 || / part /'");
    if (rc != 0)
        fprintf(stderr, "Failed to enumerate partitions (exit code %d).\n", rc);
}

static int is_known_command(const char *cmd)
{
    const command_desc *it;

    for (it = g_command_catalog; it->name != NULL; it++)
    {
        if (!strcasecmp(it->name, cmd))
            return 1;
    }

    return 0;
}

static void show_stub_notice(const char *cmd)
{
    printf("Command '%s' is recognized but not yet implemented on Linux compatibility mode.\n", cmd);
}

static exit_code run_command(char *line)
{
    char *args[MAX_ARGS];
    int argc;

    trim(line);

    if (line[0] == '\0')
        return EXIT_OK;

    if (line[0] == '#')
        return EXIT_OK;

    argc = split_args(line, args, MAX_ARGS);
    if (argc <= 0)
        return EXIT_OK;

    if (!strcasecmp(args[0], "rem"))
        return EXIT_OK;

    if (!strcasecmp(args[0], "?") || !strcasecmp(args[0], "help"))
    {
        if (argc == 1)
        {
            show_help();
            return EXIT_OK;
        }

        if (is_known_command(args[1]))
        {
            if (!strcasecmp(args[1], "list"))
                puts("Usage: list disk | list volume | list partition");
            else
                show_stub_notice(args[1]);
            return EXIT_OK;
        }

        fprintf(stderr, "Unknown command for help: %s\n", args[1]);
        return EXIT_SYNTAX;
    }

    if (!strcasecmp(args[0], "exit"))
        return EXIT_EXIT;

    if (!strcasecmp(args[0], "list"))
    {
        if (argc == 1)
        {
            fprintf(stderr, "Usage: list disk | list volume | list partition\n");
            return EXIT_SYNTAX;
        }

        if (!strcasecmp(args[1], "disk"))
        {
            list_disk();
            return EXIT_OK;
        }

        if (!strcasecmp(args[1], "volume"))
        {
            list_volume();
            return EXIT_OK;
        }

        if (!strcasecmp(args[1], "partition"))
        {
            list_partition();
            return EXIT_OK;
        }

        fprintf(stderr, "Unsupported list target: %s\n", args[1]);
        return EXIT_SYNTAX;
    }

    if (is_known_command(args[0]))
    {
        show_stub_notice(args[0]);
        return EXIT_OK;
    }

    fprintf(stderr, "Unknown command: %s\n", args[0]);
    return EXIT_SYNTAX;
}

static int run_script(const char *filename)
{
    FILE *script;
    char line[MAX_LINE];
    exit_code result;

    script = fopen(filename, "r");
    if (script == NULL)
    {
        fprintf(stderr, "Could not open script '%s': %s\n", filename, strerror(errno));
        return EXIT_FILE;
    }

    while (fgets(line, sizeof(line), script) != NULL)
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

        if (fgets(line, sizeof(line), stdin) == NULL)
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
            if ((i + 1) >= argc)
            {
                fputs("Missing value for -s\n", stderr);
                return EXIT_CMD_ARG;
            }
            script = argv[++i];
        }
        else if (!strcasecmp(argv[i] + 1, "t"))
        {
            if ((i + 1) >= argc)
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

    if (script != NULL)
        return run_script(script);

    run_interactive();
    return EXIT_OK;
}
