#ifndef _MY_KVM_
#define _MY_KVM_

#include "options.h"

struct kvm_data {
  FILE *bzImg;
  size_t img_size;
  size_t kernel_offset;
  size_t kernel_size;
  int fd_kvm;
  int fd_vm;
  int fd_vcpu;
};

struct boot_params *setup_boot_params(struct options *opts,
                                      struct kvm_data *kvm_data);

#endif
