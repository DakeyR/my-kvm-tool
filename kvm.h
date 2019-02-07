#ifndef _MY_KVM_
#define _MY_KVM_

#include "options.h"
#include "serial.h"

#define CMDLINE_ADDR 0x50000

struct kvm_data {
  FILE *bzImg;
  size_t img_size;
  size_t kernel_offset;
  size_t kernel_size;
  int fd_kvm;
  int fd_vm;
  int fd_vcpu;
  int kvm_run_size;
  struct kvm_run *kvm_run;
  struct uart_regs uart;
  struct kvm_userspace_memory_region regions[2];
};

int err_printf(const char *str, ...);

struct boot_params *setup_boot_params(struct options *opts,
                                      struct kvm_data *kvm_data);

void setup_memory_regions(struct kvm_data *kvm_data,
                          struct boot_params *boot_params,
                          struct options *opts);

void setup_sregs(struct kvm_data *kvm_data);
void setup_regs(struct kvm_data *kvm_data);
void set_cpuid(struct kvm_data *kvm_data);
int setup_cmdline(struct kvm_data *kvm_data, struct options *opts);
#endif
