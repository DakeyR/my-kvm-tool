#include <stdio.h>
#include <linux/kvm.h>
#include "serial.h"
#include "kvm.h"

void init_uart_regs(struct uart_regs *regs)
{
    __u8 *uart = (void *)regs;
    for (int i = 0; i < 12; i++)
        uart[i] = 0;
    regs->lsr = (1 << 6) | (1 << 5);
}

void serial_uart_handle_io(struct uart_regs *regs,
                           struct io *io,
                           struct kvm_run *run_data)
{
    char *data = (char *)(run_data) + io->data_offset;

    int index = io->port - SERIAL_UART_BASE_ADDR;

    if (io->direction == KVM_EXIT_IO_IN)
        err_printf("KVM_EXIT_IO_IN %x\n", io->port);

    switch(index) {
        case 0:
            if (io->direction == KVM_EXIT_IO_IN) {
                *data = regs->rbr;
            }
            else {
                regs->thr = *data;
                putchar(*data);
            }
            break;
        case 1:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->ier;
            else
                regs->ier = *data;
            break;
        case 2:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->iir;
            else
                regs->fcr = *data;
            break;
        case 3:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->lcr;
            else
                regs->lcr = *data;
            break;
        case 4:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->mcr;
            else
                regs->mcr = *data;
            break;
        case 5:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->lsr;
            break;
        case 6:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->msr;
            break;
        case 7:
            if (io->direction == KVM_EXIT_IO_IN)
                *data = regs->sr;
            else
                regs->sr = *data;
            break;
    }

    //__builtin_dump_struct(regs, &printf);
}
