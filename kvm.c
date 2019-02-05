#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <asm/bootparam.h>

#include "kvm.h"
#include "options.h"

struct boot_params *setup_boot_params(struct options *opts, 
                                      struct kvm_data *kvm_data){
    kvm_data->bzImg = fopen(opts->bzImgPath, "r");
    if (kvm_data->bzImg == NULL) {
        warn("Cannot open bzImage %s", opts->bzImgPath);
        return NULL;
    }
    int rc = fseek(kvm_data->bzImg, 0x0201, SEEK_SET);
    if (rc) {
        warn("Cannot set cursor to ofset 0x0202 in bzImage");
        fclose(kvm_data->bzImg);
        kvm_data->bzImg = NULL;
        return NULL;
    }

    size_t setupHdrSize = (size_t)fgetc(kvm_data->bzImg) + 0x0202;

    rc = fseek(kvm_data->bzImg, 0x01f1, SEEK_SET);


    struct boot_params *boot_params = malloc(sizeof (struct boot_params));
    if (boot_params == NULL) {
        warn("Cannot allocate memory for boot_params structure");
        fclose(kvm_data->bzImg);
        kvm_data->bzImg = NULL;
        return NULL;
    }

    size_t read = fread(&(boot_params->hdr), sizeof (char),setupHdrSize,
                          kvm_data->bzImg);

    if (read != setupHdrSize) {
        warn("Read size different from expected: got %zu, expected %zu, eof: %d, error: %d",
                read, setupHdrSize, feof(kvm_data->bzImg), ferror(kvm_data->bzImg));
    }

    boot_params->hdr.vid_mode = 0xFFFF;//"normal"
    boot_params->hdr.type_of_loader = 0xFF;
    boot_params->hdr.loadflags = 0xE1;//0b11100001 (LOADED HIGH, USE SEGMENT, QUIET, USE HEAP
    boot_params->hdr.ramdisk_image = 0x0;
    boot_params->hdr.ramdisk_size = 0x0;
    //boot_params->hdr->heap_end_ptr = TODO;
    //boot_params->hdr->cmd_line_ptr = TODO;

    unsigned char setup_sects = boot_params->hdr.setup_sects;
    setup_sects = setup_sects ? setup_sects : 4;

    fseek(kvm_data->bzImg, 0x0, SEEK_END);

    kvm_data->img_size = ftell(kvm_data->bzImg);
    kvm_data->kernel_offset = (setup_sects + 1) * 512;
    kvm_data->kernel_size = kvm_data->img_size - kvm_data->kernel_offset;

    return boot_params;
}
