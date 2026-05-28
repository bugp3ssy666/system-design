#include "cpu/exec.h"
#include "device/port-io.h"

void diff_test_skip_qemu();
void diff_test_skip_nemu();
void raise_intr(uint8_t NO, vaddr_t ret_addr);

make_EHelper(lidt) {
  /* [PA 3] LIDT 操作数格式为 m16&32：2 字节 limit 后接 4 字节 base */
  cpu.idtr.limit = vaddr_read(id_dest->addr, 2);
  cpu.idtr.base = vaddr_read(id_dest->addr + 2, 4);

  print_asm_template1(lidt);
}

make_EHelper(mov_r2cr) {
  TODO();

  print_asm("movl %%%s,%%cr%d", reg_name(id_src->reg, 4), id_dest->reg);
}

make_EHelper(mov_cr2r) {
  TODO();

  print_asm("movl %%cr%d,%%%s", id_src->reg, reg_name(id_dest->reg, 4));

#ifdef DIFF_TEST
  diff_test_skip_qemu();
#endif
}

make_EHelper(int) {
  /* [PA 3] INT 保存的 EIP 是 opcode 和 imm8 之后的顺序执行地址 */
  raise_intr(id_dest->val, *eip);

  print_asm("int %s", id_dest->str);
}

make_EHelper(iret) {
  /* [PA 3] PA 不做特权级切换，iret 只需弹出 EIP、CS 和 EFLAGS */
  rtl_pop(&decoding.jmp_eip);
  rtl_pop(&t0);
  cpu.cs = t0;
  rtl_pop(&cpu.eflags);
  decoding.is_jmp = 1;

  print_asm("iret");
}

make_EHelper(in) {
  /* PA 2: IN 根据操作数宽度，从译码得到的 I/O 端口读取数据到 AL/AX/EAX */
  t2 = pio_read(id_src->val, id_dest->width);
  operand_write(id_dest, &t2);

  print_asm_template2(in);

#ifdef DIFF_TEST
  diff_test_skip_qemu();
#endif
}

make_EHelper(out) {
  /* PA 2: OUT 将 AL/AX/EAX 写入译码得到的 I/O 端口，端口号来自 imm8 或 DX */
  pio_write(id_dest->val, id_src->width, id_src->val);

  print_asm("out%c %s,%s", suffix_char(id_src->width), id_src->str, id_dest->str);

#ifdef DIFF_TEST
  diff_test_skip_qemu();
#endif
}
