#ifndef WASIGO_SYSROOT_INTRIN_H_
#define WASIGO_SYSROOT_INTRIN_H_

/* GenuineIntel. HvPartition uses this to pick VMCALL rather than VMMCALL. */
static inline void __cpuid(int regs[4], int leaf) {
  regs[0] = leaf;
  regs[1] = 0x756e6547;
  regs[2] = 0x6c65746e;
  regs[3] = 0x49656e69;
}

#endif
