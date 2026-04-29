#include "cpu/exec.h"
#include "device/port-io.h"

void diff_test_skip_qemu();
void diff_test_skip_nemu();

make_EHelper(lidt) {
  TODO();

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
  TODO();

  print_asm("int %s", id_dest->str);

#ifdef DIFF_TEST
  diff_test_skip_nemu();
#endif
}

make_EHelper(iret) {
  TODO();

  print_asm("iret");
}

make_EHelper(in) {
  /* PA 2: IN 根据操作数宽度，从译码得到的 I/O 端口读取数据到 AL/AX/EAX。 */
  t2 = pio_read(id_src->val, id_dest->width);
  operand_write(id_dest, &t2);

  print_asm_template2(in);

#ifdef DIFF_TEST
  diff_test_skip_qemu();
#endif
}

make_EHelper(out) {
  /* PA 2: OUT 将 AL/AX/EAX 写入译码得到的 I/O 端口，端口号来自 imm8 或 DX。 */
  pio_write(id_dest->val, id_src->width, id_src->val);

  print_asm("out%c %s,%s", suffix_char(id_src->width), id_src->str, id_dest->str);

#ifdef DIFF_TEST
  diff_test_skip_qemu();
#endif
}
