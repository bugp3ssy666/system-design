#include "cpu/exec.h"

static inline void update_CF_add(const rtlreg_t *dest, const rtlreg_t *result, int width) {
  /* PA 2: 加法的 CF 等价于“按当前操作数宽度截断后，结果是否小于原目的操作数”。 */
  rtlreg_t mask, dest_masked, result_masked;
  rtl_li(&mask, rtl_mask(width));
  rtl_and(&dest_masked, dest, &mask);
  rtl_and(&result_masked, result, &mask);
  rtl_sltu(&t0, &result_masked, &dest_masked);
  rtl_set_CF(&t0);
}

static inline void update_CF_adc(const rtlreg_t *dest, const rtlreg_t *src, const rtlreg_t *carry, int width) {
  /* PA 2: ADC 的 CF 需要同时考虑两次无符号进位：
   * 第一次是 DEST + SRC，第二次是在部分和上继续加旧 CF。 */
  rtlreg_t mask, dest_masked, src_masked, sum, sum_masked, carry_masked, result_masked, cf1, cf2;
  rtl_li(&mask, rtl_mask(width));
  rtl_and(&dest_masked, dest, &mask);
  rtl_and(&src_masked, src, &mask);
  rtl_andi(&carry_masked, carry, 0x1);

  rtl_add(&sum, &dest_masked, &src_masked);
  rtl_and(&sum_masked, &sum, &mask);
  rtl_sltu(&cf1, &sum_masked, &dest_masked);

  rtl_add(&result_masked, &sum_masked, &carry_masked);
  rtl_and(&result_masked, &result_masked, &mask);
  rtl_sltu(&cf2, &result_masked, &sum_masked);

  rtl_or(&t0, &cf1, &cf2);
  rtl_set_CF(&t0);
}

static inline void update_CF_sub(const rtlreg_t *dest, const rtlreg_t *src, int width) {
  /* PA 2: 减法的 CF 表示无符号借位，因此只需比较截断后的 DEST 是否小于 SRC。 */
  rtlreg_t mask, dest_masked, src_masked;
  rtl_li(&mask, rtl_mask(width));
  rtl_and(&dest_masked, dest, &mask);
  rtl_and(&src_masked, src, &mask);
  rtl_sltu(&t0, &dest_masked, &src_masked);
  rtl_set_CF(&t0);
}

static inline void update_CF_sbb(const rtlreg_t *dest, const rtlreg_t *src, const rtlreg_t *carry, int width) {
  /* PA 2: SBB 的 CF 同样拆成两步判断：
   * 先看 DEST - SRC 是否借位，再看减去旧 CF 时是否继续借位。 */
  rtlreg_t mask, dest_masked, src_masked, carry_masked, diff, diff_masked, result_masked, cf1, cf2;
  rtl_li(&mask, rtl_mask(width));
  rtl_and(&dest_masked, dest, &mask);
  rtl_and(&src_masked, src, &mask);
  rtl_andi(&carry_masked, carry, 0x1);

  rtl_sub(&diff, &dest_masked, &src_masked);
  rtl_and(&diff_masked, &diff, &mask);
  rtl_sltu(&cf1, &dest_masked, &src_masked);

  rtl_sub(&result_masked, &diff_masked, &carry_masked);
  rtl_and(&result_masked, &result_masked, &mask);
  rtl_sltu(&cf2, &diff_masked, &carry_masked);

  rtl_or(&t0, &cf1, &cf2);
  rtl_set_CF(&t0);
}

static inline void update_OF_add(const rtlreg_t *dest, const rtlreg_t *src, const rtlreg_t *result, int width) {
  /* PA 2: 有符号加法溢出条件：
   * sign(DEST) == sign(SRC) 且 sign(DEST) != sign(RESULT)。 */
  rtl_xor(&t0, dest, src);
  rtl_not(&t0);
  rtl_xor(&t1, dest, result);
  rtl_and(&t0, &t0, &t1);
  rtl_msb(&t0, &t0, width);
  rtl_set_OF(&t0);
}

static inline void update_OF_sub(const rtlreg_t *dest, const rtlreg_t *src, const rtlreg_t *result, int width) {
  /* PA 2: 有符号减法溢出条件：
   * sign(DEST) != sign(SRC) 且 sign(DEST) != sign(RESULT)。 */
  rtl_xor(&t0, dest, src);
  rtl_xor(&t1, dest, result);
  rtl_and(&t0, &t0, &t1);
  rtl_msb(&t0, &t0, width);
  rtl_set_OF(&t0);
}

make_EHelper(add) {
  /* PA 2: ADD 会写回结果，并统一通过辅助函数更新 ZF/SF/CF/OF。 */
  rtl_add(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  update_CF_add(&id_dest->val, &t2, id_dest->width);
  update_OF_add(&id_dest->val, &id_src->val, &t2, id_dest->width);

  print_asm_template2(add);
}

make_EHelper(sub) {
  /* PA 2: SUB 会写回结果；具体的借位与溢出判断复用上面的通用 helper。 */
  rtl_sub(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  update_CF_sub(&id_dest->val, &id_src->val, id_dest->width);
  update_OF_sub(&id_dest->val, &id_src->val, &t2, id_dest->width);

  print_asm_template2(sub);
}

make_EHelper(cmp) {
  /* PA 2: CMP 和 SUB 的标志位行为一致，只是它不写回结果。 */
  rtl_sub(&t2, &id_dest->val, &id_src->val);

  rtl_update_ZFSF(&t2, id_dest->width);
  update_CF_sub(&id_dest->val, &id_src->val, id_dest->width);
  update_OF_sub(&id_dest->val, &id_src->val, &t2, id_dest->width);

  print_asm_template2(cmp);
}

make_EHelper(inc) {
  /* PA 2 */
  rtlreg_t one;
  rtl_li(&one, 1);

  /* INC 只等价于加 1，但按 i386 语义不会修改 CF。 */
  rtl_add(&t2, &id_dest->val, &one);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  update_OF_add(&id_dest->val, &one, &t2, id_dest->width);

  print_asm_template1(inc);
}

make_EHelper(dec) {
  /* PA 2 */
  rtlreg_t one;
  rtl_li(&one, 1);

  /* DEC 只等价于减 1，同样不会修改 CF。 */
  rtl_sub(&t2, &id_dest->val, &one);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  update_OF_sub(&id_dest->val, &one, &t2, id_dest->width);

  print_asm_template1(dec);
}

make_EHelper(neg) {
  /* PA 2: NEG 等价于 0 - DEST。
   * 当原操作数非 0 时，CF 需要被置 1。 */
  rtl_sub(&t2, &tzero, &id_dest->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_neq0(&t0, &id_dest->val);
  rtl_set_CF(&t0);
  update_OF_sub(&tzero, &id_dest->val, &t2, id_dest->width);

  print_asm_template1(neg);
}

make_EHelper(adc) {
  rtlreg_t carry, src_with_carry;

  /* 先取旧 CF，再执行 DEST + SRC + CF。 */
  rtl_get_CF(&carry);

  rtl_add(&t2, &id_dest->val, &id_src->val);
  rtl_add(&t2, &t2, &carry);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  /* 新 CF/OF 不能直接复用普通 ADD，需要把旧进位一起纳入判断。 */
  update_CF_adc(&id_dest->val, &id_src->val, &carry, id_dest->width);
  rtl_add(&src_with_carry, &id_src->val, &carry);
  update_OF_add(&id_dest->val, &src_with_carry, &t2, id_dest->width);

  print_asm_template2(adc);
}

make_EHelper(sbb) {
  rtlreg_t carry, src_with_carry;

  /* 先取旧 CF，再执行 DEST - SRC - CF。 */
  rtl_get_CF(&carry);

  rtl_sub(&t2, &id_dest->val, &id_src->val);
  rtl_sub(&t2, &t2, &carry);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  /* 新 CF/OF 需要按带借位减法的两步语义重新计算。 */
  update_CF_sbb(&id_dest->val, &id_src->val, &carry, id_dest->width);
  rtl_add(&src_with_carry, &id_src->val, &carry);
  update_OF_sub(&id_dest->val, &src_with_carry, &t2, id_dest->width);

  print_asm_template2(sbb);
}

make_EHelper(mul) {
  rtl_lr(&t0, R_EAX, id_dest->width);
  rtl_mul(&t0, &t1, &id_dest->val, &t0);

  switch (id_dest->width) {
    case 1:
      rtl_sr_w(R_AX, &t1);
      break;
    case 2:
      rtl_sr_w(R_AX, &t1);
      rtl_shri(&t1, &t1, 16);
      rtl_sr_w(R_DX, &t1);
      break;
    case 4:
      rtl_sr_l(R_EDX, &t0);
      rtl_sr_l(R_EAX, &t1);
      break;
    default: assert(0);
  }

  print_asm_template1(mul);
}

make_EHelper(imul1) {
  rtl_lr(&t0, R_EAX, id_dest->width);
  /* 一操作数 IMUL 是有符号乘法，参与运算的两个操作数都要先按各自宽度符号扩展。 */
  rtl_sext(&t0, &t0, id_dest->width);
  rtl_sext(&t2, &id_dest->val, id_dest->width);
  rtl_imul(&t0, &t1, &t2, &t0);

  switch (id_dest->width) {
    case 1:
      rtl_sr_w(R_AX, &t1);
      break;
    case 2:
      rtl_sr_w(R_AX, &t1);
      rtl_shri(&t1, &t1, 16);
      rtl_sr_w(R_DX, &t1);
      break;
    case 4:
      rtl_sr_l(R_EDX, &t0);
      rtl_sr_l(R_EAX, &t1);
      break;
    default: assert(0);
  }

  print_asm_template1(imul);
}

make_EHelper(imul2) {
  rtl_sext(&id_src->val, &id_src->val, id_src->width);
  rtl_sext(&id_dest->val, &id_dest->val, id_dest->width);

  rtl_imul(&t0, &t1, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t1);

  print_asm_template2(imul);
}

make_EHelper(imul3) {
  rtl_sext(&id_src->val, &id_src->val, id_src->width);
  /* 第三个操作数 id_src2 也必须按它自己的原始宽度做符号扩展，
   * 不能错误地复用 id_src 的宽度。 */
  rtl_sext(&id_src2->val, &id_src2->val, id_src2->width);
  rtl_sext(&id_dest->val, &id_dest->val, id_dest->width);

  rtl_imul(&t0, &t1, &id_src2->val, &id_src->val);
  operand_write(id_dest, &t1);

  print_asm_template3(imul);
}

make_EHelper(div) {
  switch (id_dest->width) {
    case 1:
      rtl_li(&t1, 0);
      rtl_lr_w(&t0, R_AX);
      break;
    case 2:
      rtl_lr_w(&t0, R_AX);
      rtl_lr_w(&t1, R_DX);
      rtl_shli(&t1, &t1, 16);
      rtl_or(&t0, &t0, &t1);
      rtl_li(&t1, 0);
      break;
    case 4:
      rtl_lr_l(&t0, R_EAX);
      rtl_lr_l(&t1, R_EDX);
      break;
    default: assert(0);
  }

  rtl_div(&t2, &t3, &t1, &t0, &id_dest->val);

  rtl_sr(R_EAX, id_dest->width, &t2);
  if (id_dest->width == 1) {
    rtl_sr_b(R_AH, &t3);
  }
  else {
    rtl_sr(R_EDX, id_dest->width, &t3);
  }

  print_asm_template1(div);
}

make_EHelper(idiv) {
  rtl_sext(&id_dest->val, &id_dest->val, id_dest->width);

  switch (id_dest->width) {
    case 1:
      rtl_lr_w(&t0, R_AX);
      rtl_sext(&t0, &t0, 2);
      rtl_msb(&t1, &t0, 4);
      rtl_sub(&t1, &tzero, &t1);
      break;
    case 2:
      rtl_lr_w(&t0, R_AX);
      rtl_lr_w(&t1, R_DX);
      rtl_shli(&t1, &t1, 16);
      rtl_or(&t0, &t0, &t1);
      rtl_msb(&t1, &t0, 4);
      rtl_sub(&t1, &tzero, &t1);
      break;
    case 4:
      rtl_lr_l(&t0, R_EAX);
      rtl_lr_l(&t1, R_EDX);
      break;
    default: assert(0);
  }

  rtl_idiv(&t2, &t3, &t1, &t0, &id_dest->val);

  rtl_sr(R_EAX, id_dest->width, &t2);
  if (id_dest->width == 1) {
    rtl_sr_b(R_AH, &t3);
  }
  else {
    rtl_sr(R_EDX, id_dest->width, &t3);
  }

  print_asm_template1(idiv);
}
