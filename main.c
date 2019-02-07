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

unsigned char out_o[] = {
  0xb0, 0x61, 0x66, 0xba, 0xf8, 0x03, 0xee, 0xeb, 0xf7
};
unsigned int out_o_len = 9;

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


    setup_regs(&kvm_data);

    int size = ioctl(kvm_data.fd_kvm, KVM_GET_VCPU_MMAP_SIZE, 0);

    kvm_data.kvm_run = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED,
                            kvm_data.fd_vcpu, 0);

    if (kvm_data.kvm_run == MAP_FAILED)
        warn("mapping vcpu failed, requested : %d", size);

    struct kvm_guest_debug debug;
    debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP | KVM_GUESTDBG_USE_SW_BP;
    ioctl(kvm_data.fd_vcpu, KVM_SET_GUEST_DEBUG, &debug);

    //set_cpuid(&kvm_data);

	for (;;) {
		int rc = ioctl(kvm_data.fd_vcpu, KVM_RUN, 0);

		if (rc < 0)
			warn("KVM_RUN");

        printf("----------------\n");
        struct kvm_regs regs;
        ioctl(kvm_data.fd_vcpu, KVM_GET_REGS, &regs);

	    struct kvm_sregs sregs;
	    ioctl(kvm_data.fd_vcpu, KVM_GET_SREGS, &sregs);

        __builtin_dump_struct(&regs, &printf);

        if (regs.rip == 0x100047)
            __builtin_dump_struct(&sregs, &printf);

        //printf("rsi: %llx\n", regs.rsi);
        printf("rip: %llx\n", regs.rip);

		printf("vm exit, reason : %d, sleeping 1s\n", kvm_data.kvm_run->exit_reason);
        switch(kvm_data.kvm_run->exit_reason)
        {
            case KVM_EXIT_INTERNAL_ERROR:
                printf("suberror: %d\n", kvm_data.kvm_run->internal.suberror);
            case KVM_EXIT_SHUTDOWN:
                return 1;
            default:
                break;
        }
        getchar();
	}

    munmap((void *)kvm_data.regions[0].userspace_addr, kvm_data.regions[0].memory_size);
    munmap((void *)kvm_data.regions[1].userspace_addr, kvm_data.regions[1].memory_size);
    free(opts);
	return 0;
}
