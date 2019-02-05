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


	kvm_data.fd_vcpu = ioctl(kvm_data.fd_vm, KVM_CREATE_VCPU, 0);

    setup_memory_regions(&kvm_data, boot_params);

    setup_sregs(&kvm_data);

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

    munmap((void *)kvm_data.regions[0].userspace_addr, kvm_data.regions[0].memory_size);
    munmap((void *)kvm_data.regions[1].userspace_addr, kvm_data.regions[1].memory_size);
    free(opts);
	return 0;
}
