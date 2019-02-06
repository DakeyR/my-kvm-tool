#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <asm/bootparam.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#include "kvm.h"
#include "options.h"

#define E820_TYPE_RAM 1

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


    struct boot_params *boot_params = calloc(1, sizeof (struct boot_params));
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

void setup_memory_regions(struct kvm_data *kvm_data,
                          struct boot_params *boot_params)
{

	void *reg1_addr = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	void *reg2_addr = mmap(NULL, (1 << 31) - (1 << 20), PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (reg2_addr == MAP_FAILED) {
        warn("mmap failed, requested %u", (1 << 31) - (1 << 20));
    }


    fseek(kvm_data->bzImg, kvm_data->kernel_offset, SEEK_SET);

    int read = fread(reg2_addr, sizeof (char), kvm_data->kernel_size, kvm_data->bzImg);
    if ((size_t)read != kvm_data->kernel_size) {
        warn("Read size different from expected: got %u, expected %zu, eof: %d, error: %d",
                read, kvm_data->kernel_size, feof(kvm_data->bzImg), ferror(kvm_data->bzImg));
    }

	struct kvm_userspace_memory_region region1 = {
		.slot = 0,
		.flags = 0,
		.guest_phys_addr = 0x0,
		.memory_size = 1 << 20,
		.userspace_addr = (__u64)reg1_addr,
	};

	struct kvm_userspace_memory_region region2 = {
		.slot = 1,
		.flags = 0,
		.guest_phys_addr = 0x100000,
		.memory_size = (1 << 31) - (1 << 20),
		.userspace_addr = (__u64)reg2_addr,
	};

	ioctl(kvm_data->fd_vm, KVM_SET_USER_MEMORY_REGION, &region1);
	ioctl(kvm_data->fd_vm, KVM_SET_USER_MEMORY_REGION, &region2);

    kvm_data->regions[0] = region1;
    kvm_data->regions[1] = region2;

    boot_params->e820_entries = 2;
    boot_params->e820_table[0].addr = kvm_data->regions[0].guest_phys_addr;
    boot_params->e820_table[0].size = kvm_data->regions[0].memory_size;
    boot_params->e820_table[0].type = E820_TYPE_RAM;
    boot_params->e820_table[1].addr = kvm_data->regions[1].guest_phys_addr;
    boot_params->e820_table[1].size = kvm_data->regions[1].memory_size;
    boot_params->e820_table[1].type = E820_TYPE_RAM;

    memcpy(reg1_addr + 0x20000, boot_params, sizeof (struct boot_params));
    free (boot_params);
}

void setup_sregs(struct kvm_data *kvm_data)
{
	struct kvm_sregs sregs;
	ioctl(kvm_data->fd_vcpu, KVM_GET_SREGS, &sregs);


#define set_segment_selector(Seg, Base, Limit, G) \
	do { \
		Seg.base = Base; \
		Seg.limit = Limit; \
		Seg.g = G; \
	} while (0)
/*      Seg.present = 1; \
        Seg.dpl = 0; \
        Seg.s = 1; \
        Seg.avl = 1; \
        Seg.l = 0; \ */

	set_segment_selector(sregs.cs, 0, ~0, 1);
	set_segment_selector(sregs.ds, 0, ~0, 1);
	//set_segment_selector(sregs.es, 0, ~0, 1);
	set_segment_selector(sregs.ss, 0, ~0, 1);

	sregs.cs.db = 1;
	sregs.ss.db = 1;
	/*sregs.cs.selector = 0x10;
	sregs.ds.selector = 0x18;
	sregs.es.selector = 0x18;
	sregs.ss.selector = 0x18;
    sregs.cs.type = 11;
    sregs.ds.type = 3;
    sregs.es.type = 3;
    sregs.ss.type = 3;*/

#undef set_segment_selector

	sregs.cr0 |= 1;

	ioctl(kvm_data->fd_vcpu, KVM_SET_SREGS, &sregs);
}

void setup_regs(struct kvm_data *kvm_data)
{
	struct kvm_regs regs;
	ioctl(kvm_data->fd_vcpu, KVM_GET_REGS, &regs);

	regs.rflags = 2;

	regs.rip = 0x100000;
    //regs.rsi = 0x20000;

	ioctl(kvm_data->fd_vcpu, KVM_SET_REGS, &regs);
}
