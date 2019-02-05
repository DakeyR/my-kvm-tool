#ifndef _MY_KVM_
#define _MY_KVM_

struct options {
    char *bzImgPath;
};

struct boot_params *setup_boot_params(struct options *opts);

#endif
