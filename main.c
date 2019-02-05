#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <asm/bootparam.h>

#include "options.h"
#include "kvm.h"

int main(int argc, char **argv)
{
    struct options *opts = parse_options(argc, argv);

    struct kvm_data kvm_data;

    struct boot_params *boot_params = setup_boot_params(opts, &kvm_data);



	kvm_data.fd_kvm = open("/dev/kvm", O_RDWR);
	if (kvm_data.fd_kvm < 0) {
		err(1, "unable to open /dev/kvm");
	}

	kvm_data.fd_vm = ioctl(kvm_data.fd_kvm, KVM_CREATE_VM, 0);
	if (kvm_data.fd_vm < 0) {
		err(1, "unable to create vm");
	}

	ioctl(kvm_data.fd_vm, KVM_SET_TSS_ADDR, 0xffffd000);
	__u64 map_addr = 0xffffc000;
	ioctl(kvm_data.fd_vm, KVM_SET_IDENTITY_MAP_ADDR, &map_addr);

	ioctl(kvm_data.fd_vm, KVM_CREATE_IRQCHIP, 0);

	void *reg1_addr = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	void *reg2_addr = mmap(NULL, (size_t)(1 << 31) - (1 << 20), PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    memcpy(reg1_addr + 0x20000, boot_params, sizeof (struct boot_params));
    int read = fread(reg2_addr, sizeof (char), kvm_data.kernel_size, kvm_data.bzImg);
    if ((size_t)read != kvm_data.kernel_size)
        warn("Read size different from expected: got %u, expected %zu, eof: %d, error: %d",
                read, kvm_data.kernel_size, feof(kvm_data.bzImg), ferror(kvm_data.bzImg));
    free (boot_params);

	struct kvm_userspace_memory_region region1 = {
		.slot = 0,
		.flags = 0,
		.guest_phys_addr = 0x0,
		.memory_size = 1 << 20,
		.userspace_addr = (__u64)reg1_addr,
	};

	struct kvm_userspace_memory_region region2 = {
		.slot = 0,
		.flags = 0,
		.guest_phys_addr = 0x100000,
		.memory_size = 1 << 20,
		.userspace_addr = (__u64)reg2_addr,
	};

	ioctl(kvm_data.fd_vm, KVM_SET_USER_MEMORY_REGION, &region1);
	ioctl(kvm_data.fd_vm, KVM_SET_USER_MEMORY_REGION, &region2);

	kvm_data.fd_vcpu = ioctl(kvm_data.fd_vm, KVM_CREATE_VCPU, 0);

	struct kvm_sregs sregs;
	ioctl(kvm_data.fd_vcpu, KVM_GET_SREGS, &sregs);

#define set_segment_selector(Seg, Base, Limit, G) \
	do { \
		Seg.base = Base; \
		Seg.limit = Limit; \
		Seg.g = G; \
	} while (0)

	set_segment_selector(sregs.cs, 0, ~0, 1);
	set_segment_selector(sregs.ds, 0, ~0, 1);
	set_segment_selector(sregs.ss, 0, ~0, 1);

	sregs.cs.db = 1;
	sregs.ss.db = 1;

#undef set_segment_selector

	sregs.cr0 |= 1;

	ioctl(kvm_data.fd_vcpu, KVM_SET_SREGS, &sregs);

	struct kvm_regs regs;
	ioctl(kvm_data.fd_vcpu, KVM_GET_REGS, &regs);

	regs.rflags = 2;

	regs.rip = 0x100000;

	ioctl(kvm_data.fd_vcpu, KVM_SET_REGS, &regs);

	for (;;) {
		int rc = ioctl(kvm_data.fd_vcpu, KVM_RUN, 0);

		if (rc < 0)
			warn("KVM_RUN");

		printf("vm exit, sleeping 1s\n");
		sleep(1);
	}

    munmap(reg1_addr, 1 << 20);
    munmap(reg2_addr, (size_t)(1 << 31) - (1 << 20));
    free(opts);
	return 0;
}
