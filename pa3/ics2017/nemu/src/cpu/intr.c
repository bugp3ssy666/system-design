#include "cpu/exec.h"
#include "memory/mmu.h"

// [PA 3] 实现 IDT 中断门描述符的解析和 INT 指令的中断处理。
void raise_intr(uint8_t NO, vaddr_t ret_addr) {
  assert((uint32_t)NO * 8 + 7 <= cpu.idtr.limit);

  /* i386 从 IDTR.base 开始，按 8 字节门描述符索引 IDT。 */
  vaddr_t gate_addr = cpu.idtr.base + NO * 8;
  uint32_t gate_low = vaddr_read(gate_addr, 4);
  uint32_t gate_high = vaddr_read(gate_addr + 4, 4);

  rtlreg_t val;

  /* 构造 INT 的入栈现场：依次压入 EFLAGS、CS 和返回 EIP。 */
  rtl_push(&cpu.eflags);
  val = cpu.cs;
  rtl_push(&val);
  val = ret_addr;
  rtl_push(&val);

  cpu.cs = (gate_low >> 16) & 0xffff;
  decoding.jmp_eip = (gate_low & 0xffff) | (gate_high & 0xffff0000);
  decoding.is_jmp = 1;

}

void dev_raise_intr() {
}
