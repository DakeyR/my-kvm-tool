#ifndef _SERIAL_H_
#define _SERIAL_H_
#include <linux/kvm.h>

#define SERIAL_UART_BASE_ADDR 0x3f8

struct io {
  __u8 direction;
  __u8 size;
  __u16 port;
  __u32 count;
  __u64 data_offset;
};

struct uart_regs {
  __u8 thr, rbr, dll;
  __u8 ier, dhl;
  __u8 iir, fcr;
  __u8 lcr;
  __u8 mcr;
  __u8 lsr;
  __u8 msr;
  __u8 sr;
};

void init_uart_regs(struct uart_regs *regs);
void serial_uart_handle_io(struct uart_regs *regs,
                           struct io *io,
                           struct kvm_run *run_data);

#endif
