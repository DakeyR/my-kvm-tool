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
#include <stdarg.h>

#include "options.h"
#include "kvm.h"
#include "serial.h"

unsigned char out_o[] = {
  0xb0, 0x61, 0x66, 0xba, 0xf8, 0x03, 0xee, 0xeb, 0xf7
};
unsigned int out_o_len = 9;

int main(int argc, char **argv)
{
    struct options *opts = parse_options(argc, argv);
    dump_options(opts);

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

    struct kvm_pit_config pit_conf;
    pit_conf.flags = 0;//KVM_PIT_SPEAKER_DUMMY;
    ioctl(kvm_data.fd_vm, KVM_CREATE_PIT2, &pit_conf);

	kvm_data.fd_vcpu = ioctl(kvm_data.fd_vm, KVM_CREATE_VCPU, 0);

    setup_memory_regions(&kvm_data, boot_params, opts);

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


    init_uart_regs(&kvm_data.uart);
    set_cpuid(&kvm_data);

	for (;;) {
		int rc = ioctl(kvm_data.fd_vcpu, KVM_RUN, 0);

		if (rc < 0)
			warn("KVM_RUN");

        struct kvm_regs regs;
        ioctl(kvm_data.fd_vcpu, KVM_GET_REGS, &regs);

	    struct kvm_sregs sregs;
	    ioctl(kvm_data.fd_vcpu, KVM_GET_SREGS, &sregs);

#if defined(__clang__)
        __builtin_dump_struct(&regs, &err_printf);
        __builtin_dump_struct(&sregs, &err_printf);
#endif
        err_printf("vm exit, reason : %d, sleeping 1s\n", kvm_data.kvm_run->exit_reason);


        switch(kvm_data.kvm_run->exit_reason)
        {
            case KVM_EXIT_IO:
                {
                __u16 port = kvm_data.kvm_run->io.port;
                if (port >= SERIAL_UART_BASE_ADDR
                       && port <= SERIAL_UART_BASE_ADDR + 7) {
                    serial_uart_handle_io(&kvm_data.uart, (void *)&(kvm_data.kvm_run->io), kvm_data.kvm_run);
                    break;
                }
                else if (port == 0x61)
                    break;
                err_printf("KVM_EXIT_IO %s port: 0x%x\n", kvm_data.kvm_run->io.direction ? "OUT" : "IN", port);
                break;
                }
            case KVM_EXIT_INTERNAL_ERROR:
                err_printf("KVM_EXIT_INTERNAL_ERROR: suberror: %d\n", kvm_data.kvm_run->internal.suberror);
            case KVM_EXIT_SHUTDOWN:
                munmap((void *)kvm_data.regions[0].userspace_addr, kvm_data.regions[0].memory_size);
                munmap((void *)kvm_data.regions[1].userspace_addr, kvm_data.regions[1].memory_size);
                free(opts);
                return 1;
            default:
                break;
        }
    }

    munmap((void *)kvm_data.regions[0].userspace_addr, kvm_data.regions[0].memory_size);
    munmap((void *)kvm_data.regions[1].userspace_addr, kvm_data.regions[1].memory_size);
    free(opts);
	return 0;
}
