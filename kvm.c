#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <asm/bootparam.h>

#include "kvm.h"

struct boot_params *setup_boot_params(struct options *opts){
    FILE *bzImg = fopen(opts->bzImgPath, "r");
    if (bzImg == NULL) {
        warn("Cannot open bzImage %s", opts->bzImgPath);
        return NULL;
    }
    int rc = fseek(bzImg, 0x0201, SEEK_SET);
    if (rc) {
        warn("Cannot set cursor to ofset 0x0202 in bzImage");
        fclose(bzImg);
        return NULL;
    }

    size_t setupHdrSize = (size_t)fgetc(bzImg) + 0x0202;

    rc = fseek(bzImg, 0x01f1, SEEK_SET);

    struct boot_params *boot_params = malloc(sizeof (struct boot_params));
    if (boot_params == NULL) {
        warn("Cannot allocate memory for boot_params structure");
        fclose(bzImg);
        return NULL;
    }

    size_t read = fread(&boot_params->hdr, setupHdrSize,
                        sizeof (char), bzImg);
    if (read != setupHdrSize)
        warn("Read size different from expected: got %zu, expected: %zu ",
                read, setupHdrSize);

    return boot_params;
}
