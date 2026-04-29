#include "cpu/exec.h"

make_EHelper(mov) {
  operand_write(id_dest, &id_src->val);
  print_asm_template2(mov);
}

make_EHelper(push) {
  /* PA 2: dummy 中的 PUSH r32 使用 opcode 50+r。r 译码器已经把源寄存器的值
   * 读入 id_dest->val，因此执行阶段只需要完成 i386 的栈操作：
   * ESP -= 4，然后把源操作数写入 [ESP]。
   */
  rtl_push(&id_dest->val);

  print_asm_template1(push);
}

make_EHelper(pop) {
  /* PA 2: dummy 中的 POP r32 使用 opcode 58+r。先通过 RTL 弹出栈顶值，
   * 再用通用 operand_write() 写回已经译码出的目标寄存器。
   */
  rtl_pop(&t2);
  operand_write(id_dest, &t2);

  print_asm_template1(pop);
}

make_EHelper(pusha) {
  TODO();

  print_asm("pusha");
}

make_EHelper(popa) {
  TODO();

  print_asm("popa");
}

make_EHelper(leave) {
  /* PA 2: 
   * leave 等价于:
   *   mov esp, ebp
   *   pop ebp
   * 这样函数尾声可以一次恢复当前栈帧。 */
  rtl_mv(&cpu.esp, &cpu.ebp);
  rtl_pop(&cpu.ebp);

  print_asm("leave");
}

make_EHelper(cltd) {
  /* PA 2: cltd instr*/
  if (decoding.is_operand_size_16) {
    /* cwd: 用 AX 的符号位填满 DX。 */
    rtl_lr_w(&t0, R_AX);
    rtl_sext(&t1, &t0, 2);
    rtl_sari(&t1, &t1, 16);
    rtl_sr_w(R_DX, &t1);
  }
  else {
    /* cdq: 用 EAX 的符号位填满 EDX。 */
    rtl_lr_l(&t0, R_EAX);
    rtl_sext(&t1, &t0, 4);
    rtl_sari(&t1, &t1, 31);
    rtl_sr_l(R_EDX, &t1);
  }

  print_asm(decoding.is_operand_size_16 ? "cwd" : "cltd");
}

make_EHelper(cwtl) {
  /* PA 2: cwtl instr*/
  if (decoding.is_operand_size_16) {
    /* cbtw: 把 AL 符号扩展到 AX。 */
    rtl_lr_b(&t0, R_AL);
    rtl_sext(&t1, &t0, 1);
    rtl_sr_w(R_AX, &t1);
  }
  else {
    /* cwtl/cwde: 把 AX 符号扩展到 EAX。 */
    rtl_lr_w(&t0, R_AX);
    rtl_sext(&t1, &t0, 2);
    rtl_sr_l(R_EAX, &t1);
  }

  print_asm(decoding.is_operand_size_16 ? "cbtw" : "cwtl");
}

make_EHelper(movsx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  rtl_sext(&t2, &id_src->val, id_src->width);
  operand_write(id_dest, &t2);
  print_asm_template2(movsx);
}

make_EHelper(movzx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  operand_write(id_dest, &id_src->val);
  print_asm_template2(movzx);
}

make_EHelper(lea) {
  rtl_li(&t2, id_src->addr);
  operand_write(id_dest, &t2);
  print_asm_template2(lea);
}
