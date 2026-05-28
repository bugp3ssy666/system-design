#ifndef __ARCH_H__
#define __ARCH_H__

#include <am.h>

#define PMEM_SIZE (128 * 1024 * 1024)
#define PGSIZE    4096    // Bytes mapped by a page

struct _RegSet {
  /* [PA 3] 成员顺序必须和 trap.S 从当前 %esp 开始的 trap frame 顺序一致 */
  uintptr_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  int irq;
  uintptr_t error_code, eip, cs, eflags;
};

/* [PA 3] libos 通过 eax 传系统调用号，ebx/ecx/edx 传三个参数 */
#define SYSCALL_ARG1(r) ((r)->eax)
#define SYSCALL_ARG2(r) ((r)->ebx)
#define SYSCALL_ARG3(r) ((r)->ecx)
#define SYSCALL_ARG4(r) ((r)->edx)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
#endif
