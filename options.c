#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "kvm.h"

static int is_option(char *arg)
{
    return arg && arg[0] == '-';
}

static enum option hash_opt(char *arg)
{
    int val = 0;
    for (int i = 0; arg && arg[i]; i++)
        val += arg[i];

    return (enum option)val;
}

struct options *parse_options(int argc, char **argv)
{
    int option_arg = 0;
    enum option opt;

    if (argc == 1)
        return NULL;
    struct options *opts = malloc(sizeof (struct options));
    opts->bzImgPath = NULL;
    opts->initrdPath = NULL;
    opts->argc = 0;
    opts->ram_size = (unsigned long long)(2048) * (1 << 20);

    for (int i = 1; i < argc; i++)
    {
        if (!option_arg) {
            if (is_option(argv[i])) {
                opt = hash_opt(argv[i]);
                if (opt == HELP)
                    display_help(opts);
                option_arg = 1;
            }
            else if (opts->bzImgPath == NULL) {
                opts->bzImgPath = malloc(strlen(argv[i]) *sizeof (char));
                opts->bzImgPath = strcpy(opts->bzImgPath, argv[i]);
            }
            else {
                opts->cmdline = argv + i;
                opts->argc = argc - i;
                break;
            }
        }
        else {
            switch (opt) {
                case RAM:
                    opts->ram_size = atoll(argv[i]) * (1 << 20);
                    break;
                case INITRD:
                    if (opts->initrdPath)
                        break;
                    opts->initrdPath = malloc(strlen(argv[i]) * sizeof (char));
                    opts->initrdPath = strcpy(opts->initrdPath, argv[i]);
                    break;
                default:
                    err_printf("Unkown option %s\n", argv[i - 1]);
                    display_help(opts);
            }
            option_arg = 0;
        }
    }

    if (opts->bzImgPath == NULL)
        display_help(opts);
    return opts;
}

void display_help(struct options *opts)
{
    printf("mykvm: Help message\n");
    free(opts);
    exit(1);
}

void dump_options(struct options *opts)
{
    printf("ram: %llu\n", opts->ram_size);
    printf("argc: %d\n", opts->argc);
    printf("initrd: %s\n", opts->initrdPath);
    printf("bzImage: %s\n", opts->bzImgPath);
}
