#ifndef _OPTIONS_H_
#define _OPTIONS_H_

struct options {
    char *bzImgPath;
};

struct options *parse_options(int argc, char **argv);
#endif
