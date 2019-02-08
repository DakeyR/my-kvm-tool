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

int err_printf(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    fprintf(stderr, str, args);
    va_end(args);
    return 0;
}

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
    boot_params->hdr.loadflags |= KEEP_SEGMENTS | LOADED_HIGH | QUIET_FLAG | CAN_USE_HEAP;// (LOADED HIGH, USE SEGMENT, QUIET, USE HEAP
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

    __builtin_dump_struct(boot_params, &err_printf);
    return boot_params;
}

void setup_memory_regions(struct kvm_data *kvm_data,
                          struct boot_params *boot_params,
                          struct options *opts)
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

    /*
    char cmd[] = "noapic noacpi pci=conf1 reboot=k panic=1 i8042.direct=1 i8042.dumbkbd=1 i8042.nopnp=1 console=ttyS0 earlyprintk=serial i8042.noaux=1 init=/bin/sh";
    memcpy(reg1_addr + 0x50000, cmd, sizeof(cmd));
    */
    boot_params->hdr.cmdline_size = setup_cmdline(kvm_data, opts);
    boot_params->hdr.cmd_line_ptr = CMDLINE_ADDR;
    setup_initrd(kvm_data, opts, boot_params);
    memcpy(reg1_addr + 0x20000, boot_params, sizeof (struct boot_params));

    __builtin_dump_struct(boot_params, &err_printf);
    free (boot_params);


}

int setup_cmdline(struct kvm_data *kvm_data, struct options *opts)
{
    char *reg = (void *)kvm_data->regions[0].userspace_addr;
    int offset = 0;
    for (int i = 0; i < opts->argc; i++)
    {
        int len = strlen(opts->cmdline[i]);
        memcpy(reg + CMDLINE_ADDR + offset, opts->cmdline[i], len);
        reg[CMDLINE_ADDR + offset + len] = ' ';
        offset += len + 1;
    }
    return offset;
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
        Seg.present = 1; \
        Seg.dpl = 0; \
        Seg.s = 1; \
        Seg.avl = 1; \
        Seg.l = 0; \
	} while (0)

	set_segment_selector(sregs.cs, 0, ~0, 1);
	set_segment_selector(sregs.ds, 0, ~0, 1);
	set_segment_selector(sregs.es, 0, ~0, 1);
	set_segment_selector(sregs.ss, 0, ~0, 1);

#undef set_segment_selector

	sregs.cs.db = 1;
	sregs.ss.db = 1;
	sregs.ds.db = 1;
	sregs.es.db = 1;

	sregs.cs.selector = 0x10;
	sregs.ds.selector = 0x18;
	sregs.es.selector = 0x18;
	sregs.ss.selector = 0x18;
    sregs.cs.type = 11;
    sregs.ds.type = 2;
    sregs.es.type = 2;
    sregs.ss.type = 2;


	sregs.cr0 |= 1;

	ioctl(kvm_data->fd_vcpu, KVM_SET_SREGS, &sregs);
}

void setup_regs(struct kvm_data *kvm_data)
{
	struct kvm_regs regs;
	ioctl(kvm_data->fd_vcpu, KVM_GET_REGS, &regs);

	regs.rflags = 2;

    /*
     * struct kvm_regs { //out (KVM_GET_REGS) / in (KVM_SET_REGS)
--__u64 rax, rbx, rcx, rdx;
--__u64 rsi, rdi, rsp, rbp;
--__u64 r8,  r9,  r10, r11;
--__u64 r12, r13, r14, r15;
--__u64 rip, rflags;
    };
    */
	regs.rip = 0x100000;
    regs.rsi = 0x20000;
    regs.rdi = 0x0;
    regs.rbx = 0x0;
    regs.rdx = 0x0;
    regs.rbp = 0x0;
    regs.rsp = 0x0;

	ioctl(kvm_data->fd_vcpu, KVM_SET_REGS, &regs);
}

void set_cpuid(struct kvm_data *kvm_data)
{
/*#define KVM_CPUID_SIGNATURE 0x40000000
#define KVM_CPUID_FEATURES 0x40000001

    struct kvm_cpuid_entry entry1 = {
        .function = KVM_CPUID_SIGNATURE,
        .eax = 0x40000001,
        .ebx = 0x4b4d564b,
        .ecx = 0x564b4d56,
        .edx = 0x4d,
    };
    struct kvm_cpuid_entry entry2 = {
        .function = KVM_CPUID_FEATURES,
        .eax = 0x0,
        .ebx = 0x0,
        .ecx = 0x0,
        .edx = 0x0,
    };

    struct kvm_cpuid *cpuid = malloc(sizeof (struct kvm_cpuid) + sizeof (struct kvm_cpuid_entry));
    cpuid->nent = 2;
    cpuid->entries[0] = entry1;
    cpuid->entries[1] = entry2;

    ioctl(kvm_data->fd_vcpu, KVM_SET_CPUID, cpuid);
*/
    /* the following code is from the simplekvm by Hideki EIRAKU */
      /* Set cpuid entry */
  struct {
    struct kvm_cpuid a;
    struct kvm_cpuid_entry b[4];
  } cpuid_info;
  cpuid_info.a.nent = 4;
  cpuid_info.a.entries[0].function = 0;
  cpuid_info.a.entries[0].eax = 1;
  cpuid_info.a.entries[0].ebx = 0;
  cpuid_info.a.entries[0].ecx = 0;
  cpuid_info.a.entries[0].edx = 0;
  cpuid_info.a.entries[1].function = 1;
  cpuid_info.a.entries[1].eax = 0x400;
  cpuid_info.a.entries[1].ebx = 0;
  cpuid_info.a.entries[1].ecx = 0;
  cpuid_info.a.entries[1].edx = 0x701b179; /* SSE2, SSE, FXSR, PAT,
					      CMOV, PGE, MTRR, CX8,
					      PAE, MSR, TSC, PSE,
					      FPU */
  cpuid_info.a.entries[2].function = 0x80000000;
  cpuid_info.a.entries[2].eax = 0x80000001;
  cpuid_info.a.entries[2].ebx = 0;
  cpuid_info.a.entries[2].ecx = 0;
  cpuid_info.a.entries[2].edx = 0;
  cpuid_info.a.entries[3].function = 0x80000001;
  cpuid_info.a.entries[3].eax = 0;
  cpuid_info.a.entries[3].ebx = 0;
  cpuid_info.a.entries[3].ecx = 0;
  cpuid_info.a.entries[3].edx = 0x20100800; /* AMD64, NX, SYSCALL */
  if (ioctl (kvm_data->fd_vcpu, KVM_SET_CPUID, &cpuid_info.a) < 0)
    err (1, "KVM_SET_CPUID failed");
}

void setup_initrd(struct kvm_data *kvm_data,
                  struct options *opts,
                  struct boot_params *boot_params)
{
    FILE *initrd = fopen(opts->initrdPath, "r");
    if (initrd == NULL) {
        warn("unable to open initrd");
        return;
    }

    int size = 0;
    fseek(initrd, 0, SEEK_END);
    size = ftell(initrd);
    fseek(initrd, 0, SEEK_SET);
    if (ftell(initrd) != 0) {
        warn("position in initrd not reset");
        fclose(initrd);
        return;
    }

    void *addr = (char *)(kvm_data->regions[1].userspace_addr)
                     + (kvm_data->regions[1].memory_size - size - 0x100000);
    int i = 0;
    while (i < size) {
        int rc = fread(addr + i, sizeof (char), size, initrd);
        i += rc;
        if (ferror(initrd)) {
            warn("error reading initrd");
            fclose(initrd);
            return;
        }
    }

    boot_params->hdr.ramdisk_image = 0x37FFFFFF - size;
    boot_params->hdr.ramdisk_size = size;
    fclose(initrd);
}
