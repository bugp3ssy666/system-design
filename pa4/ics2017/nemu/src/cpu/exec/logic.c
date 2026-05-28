#include "cpu/exec.h"

static inline void clear_logic_flags(void) {
  /* PA 2: 逻辑类指令执行后会把 CF 和 OF 清 0。 */
  rtl_li(&t0, 0);
  rtl_set_CF(&t0);
  rtl_set_OF(&t0);
}

make_EHelper(test) {
  /* PA 2: TEST 只做按位与并更新标志位，不写回结果。 */
  rtl_and(&t2, &id_dest->val, &id_src->val);
  clear_logic_flags();
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(test);
}

make_EHelper(and) {
  /* PA 2: AND 会写回按位与结果，并按 i386 语义清零 CF/OF。 */
  rtl_and(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  clear_logic_flags();
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(and);
}

make_EHelper(xor) {
  /* PA 2: XOR 会写回按位异或结果，并按 i386 语义清零 CF/OF。 */
  rtl_xor(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  clear_logic_flags();
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(xor);
}

make_EHelper(or) {
  /* PA 2: OR 会写回按位或结果，并按 i386 语义清零 CF/OF。 */
  rtl_or(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  clear_logic_flags();
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(or);
}

make_EHelper(rol) {
  uint32_t count = id_src->val & 0x1f;

  if (count != 0) {
    uint32_t width = id_dest->width * 8;
    uint32_t mask = rtl_mask(id_dest->width);
    uint32_t value = id_dest->val & mask;
    uint32_t rotate = count % width;

    /* ROL rotates bits inside the operand width and writes the wrapped bit to CF. */
    t2 = (rotate == 0 ? value : ((value << rotate) | (value >> (width - rotate)))) & mask;
    operand_write(id_dest, &t2);

    rtl_li(&t0, t2 & 0x1);
    rtl_set_CF(&t0);

    if (count == 1) {
      rtl_msb(&t1, &t2, id_dest->width);
      rtl_xor(&t0, &t1, &t0);
      rtl_set_OF(&t0);
    }
  }

  print_asm_template2(rol);
}

make_EHelper(sar) {
  /* PA 2: SAR 是算术右移，高位补符号位；当前实验只需要更新 ZF/SF。 */
  rtl_sar(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(sar);
}

make_EHelper(shl) {
  /* PA 2: SHL/SAL 是逻辑左移；当前实验只需要更新 ZF/SF。 */
  rtl_shl(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(shl);
}

make_EHelper(shr) {
  /* PA 2: SHR 是逻辑右移，高位补 0；当前实验只需要更新 ZF/SF。 */
  rtl_shr(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);
  rtl_update_ZFSF(&t2, id_dest->width);

  print_asm_template2(shr);
}

make_EHelper(setcc) {
  uint8_t subcode = decoding.opcode & 0xf;
  rtl_setcc(&t2, subcode);
  operand_write(id_dest, &t2);

  print_asm("set%s %s", get_cc_name(subcode), id_dest->str);
}

make_EHelper(not) {
  /* PA 2: NOT 只做按位取反，不影响任何标志位。 */
  rtl_mv(&t2, &id_dest->val);
  rtl_not(&t2);
  operand_write(id_dest, &t2);

  print_asm_template1(not);
}
