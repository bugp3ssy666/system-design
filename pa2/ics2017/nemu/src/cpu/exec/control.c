#include "cpu/exec.h"

make_EHelper(jmp) {
  // 目标地址已经在译码阶段计算完成
  decoding.is_jmp = 1;

  print_asm("jmp %x", decoding.jmp_eip);
}

make_EHelper(jcc) {
  // 目标地址已经在译码阶段计算完成
  uint8_t subcode = decoding.opcode & 0xf;
  rtl_setcc(&t2, subcode);
  decoding.is_jmp = t2;

  print_asm("j%s %x", get_cc_name(subcode), decoding.jmp_eip);
}

make_EHelper(jmp_rm) {
  decoding.jmp_eip = id_dest->val;
  decoding.is_jmp = 1;

  print_asm("jmp *%s", id_dest->str);
}

make_EHelper(call) {
  /* PA 2: CALL rel32 先压入下一条指令地址，再跳转到译码阶段算出的相对目标地址。
   * 走到这里时 decode_J 已经取走 32 位位移，因此 *eip 已经是顺序执行的
   * 下一条指令地址。
   */
  rtl_push(eip);
  decoding.is_jmp = 1;

  print_asm("call %x", decoding.jmp_eip);
}

make_EHelper(ret) {
  /* PA 2: 近返回 RET 从栈顶弹出返回地址作为新的 EIP。公共的 update_eip()
   * 会在本 helper 返回后根据 is_jmp 把 decoding.jmp_eip 写入 cpu.eip。
   */
  rtl_pop(&decoding.jmp_eip);
  decoding.is_jmp = 1;

  print_asm("ret");
}

make_EHelper(call_rm) {
  /* PA 2: 间接 call 先把顺序执行的下一条指令地址压栈，
   * 再跳到译码阶段算出的目标操作数。 */
  rtl_push(eip);
  decoding.jmp_eip = id_dest->val;
  decoding.is_jmp = 1;

  print_asm("call *%s", id_dest->str);
}
