#include <stdlib.h>
#include "options.h"

struct options *parse_options(int argc, char **argv)
{
    if (argc == 1)
        return NULL;
    struct options *opts = malloc(sizeof (struct options));
    if (opts == NULL)
        return NULL;
    opts->bzImgPath = argv[1];
    return opts;
}
