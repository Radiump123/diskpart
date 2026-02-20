#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_LINE 1024

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

static void show_header(void)
{
    printf("\nDiskPart (Linux compatibility mode)\n");
    printf("Copyright (C) ReactOS contributors\n");
    printf("Type 'help' for available commands.\n\n");
}

static void show_help(void)
{
    puts("Available commands:");
    puts("  help           Show this help");
    puts("  list disk      List block devices from /sys/block");
    puts("  list volume    List mounted volumes using lsblk");
    puts("  exit           Exit diskpart");
}

static void list_disk(void)
{
    DIR *d = opendir("/sys/block");
    struct dirent *ent;

    if (d == NULL)
    {
        fprintf(stderr, "Unable to read /sys/block: %s\\n", strerror(errno));
        return;
    }

    puts("Disks:");
    while ((ent = readdir(d)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        if (!strncmp(ent->d_name, "loop", 4) ||
            !strncmp(ent->d_name, "ram", 3))
            continue;

        printf("  %s\n", ent->d_name);
    }

    closedir(d);
}

static void list_volume(void)
{
    fflush(stdout);
    int rc = system("lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS");
    if (rc != 0)
        fprintf(stderr, "Failed to run lsblk (exit code %d).\\n", rc);
}

static exit_code run_command(char *line)
{
    trim(line);

    if (line[0] == '\0')
        return EXIT_OK;

    if (!strcasecmp(line, "help") || !strcmp(line, "?"))
    {
        show_help();
        return EXIT_OK;
    }

    if (!strcasecmp(line, "exit"))
        return EXIT_EXIT;

    if (!strcasecmp(line, "list disk"))
    {
        list_disk();
        return EXIT_OK;
    }

    if (!strcasecmp(line, "list volume"))
    {
        list_volume();
        return EXIT_OK;
    }

    fprintf(stderr, "Unknown command: %s\\n", line);
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
        fprintf(stderr, "Could not open script '%s': %s\\n", filename, strerror(errno));
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
            fprintf(stderr, "Invalid argument: %s\\n", argv[i]);
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
                fputs("Missing value for -s\\n", stderr);
                return EXIT_CMD_ARG;
            }
            script = argv[++i];
        }
        else if (!strcasecmp(argv[i] + 1, "t"))
        {
            if ((i + 1) >= argc)
            {
                fputs("Missing value for -t\\n", stderr);
                return EXIT_CMD_ARG;
            }
            timeout = atoi(argv[++i]);
            if (timeout < 0)
                timeout = 0;
        }
        else
        {
            fprintf(stderr, "Unknown flag: %s\\n", argv[i]);
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
