#ifndef __REG_H__
#define __REG_H__

#include "common.h"
#include "memory/mmu.h"

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
enum { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum { R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH };

/* The layout below matches the register encoding scheme in i386
 * instruction format. For example, cpu.gpr[3]._16 is `bx' and
 * cpu.gpr[1]._8[1] is `ch'. The anonymous union makes the indexed
 * register view and the named register view share the same storage.
 */

typedef union {
  rtlreg_t _32;
  uint16_t _16;
  uint8_t _8[2];
} GPR;

typedef struct {
  union {
    GPR gpr[8];

    /* Do NOT change the order of the GPRs' definitions. */

    /* In NEMU, rtlreg_t is exactly uint32_t. This makes RTL instructions
     * in PA2 able to directly access these registers.
     */
    struct {
      rtlreg_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    };
  };

  vaddr_t eip;

  /* [PA 4] 分页机制需要建模 CR0/CR3：CR0.PG 控制是否开启分页，CR3 指向页目录物理基址。 */
  CR0 cr0;
  CR3 cr3;

  /* [PA 3] 中断机制的最小状态：LIDT 写入 IDTR，INT 入栈时会用到 CS。 */
  struct {
    uint16_t limit;
    uint32_t base;
  } idtr;
  uint16_t cs;

  /* PA 2: 当前已实现指令会用到的 EFLAGS 布局。
   * 各标志位的位置遵循 i386 对 EFLAGS 的定义：
    31                  23                  15               7             0
    +-------------------+-------------------+-------+-+-+-+-+-+-+-------+-+-+
    |                                               |O| |I| |S|Z|       | |C|
    |                        X                      | |X| |X| | |   X   |1| |
    |                                               |F| |F| |F|F|       | |F|
    +-------------------+-------------------+-------+-+-+-+-+-+-+-------+-+-+
   * eflags 原始视图用于 restart()/difftest 初始化或复制整个寄存器；
   * 位域视图用于让 RTL 直接读写单个标志位，避免每条指令手写移位。
   */
  union {
    rtlreg_t eflags;
    struct {
      uint32_t CF      : 1;   // 0
      uint32_t bit1    : 1;   // 1
      uint32_t bit2_5  : 4;   // 2..5
      uint32_t ZF      : 1;   // 6
      uint32_t SF      : 1;   // 7
      uint32_t bit8    : 1;   // 8
      uint32_t IF      : 1;   // 9
      uint32_t bit10   : 1;   // 10
      uint32_t OF      : 1;   // 11
      uint32_t bit12_31: 20;  // 12..31
    };
  };

  /* [PA 4] 模拟 CPU 的 INTR 引脚，高电平表示有硬件中断等待响应。 */
  bool INTR;

} CPU_state;

extern CPU_state cpu;

static inline int check_reg_index(int index) {
  assert(index >= 0 && index < 8);
  return index;
}

#define reg_l(index) (cpu.gpr[check_reg_index(index)]._32)
#define reg_w(index) (cpu.gpr[check_reg_index(index)]._16)
#define reg_b(index) (cpu.gpr[check_reg_index(index) & 0x3]._8[index >> 2])

extern const char* regsl[];
extern const char* regsw[];
extern const char* regsb[];

static inline const char* reg_name(int index, int width) {
  assert(index >= 0 && index < 8);
  switch (width) {
    case 4: return regsl[index];
    case 1: return regsb[index];
    case 2: return regsw[index];
    default: assert(0);
  }
}

#endif
