#ifndef _OPTIONS_H_
#define _OPTIONS_H_

struct options {
    unsigned long long ram_size;
    int argc;
    char *initrdPath;
    char *bzImgPath;
    char **cmdline;
};

enum option {
  HELP = 149,
  RAM = 154,
  INITRD = 740,
};

void display_help(struct options *opts);
struct options *parse_options(int argc, char **argv);
void dump_options(struct options *opts);
#endif
