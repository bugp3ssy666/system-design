#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NEQ,
  TK_AND,
  TK_OR,
  TK_NUM,
  TK_HEX,
  TK_REG,
  TK_NEG,
  TK_DEREF,
  TK_NOT

  /* TODO: Add more token types */
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},             // 空格串
  {"0[xX][0-9a-fA-F]+", TK_HEX}, // 十六进制整数
  {"[0-9]+", TK_NUM},            // 十进制整数
  {"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG}, // 寄存器名
  {"==", TK_EQ},                 // 相等比较
  {"!=", TK_NEQ},                // 不等比较
  {"&&", TK_AND},                // 逻辑与
  {"\\|\\|", TK_OR},             // 逻辑或
  {"!", TK_NOT},                 // 逻辑非
  {"\\+", '+'},                  // 加号
  {"-", '-'},                    // 减号或负号
  {"\\*", '*'},                  // 乘号或解引用
  {"/", '/'},                    // 除号
  {"\\(", '('},                  // 左括号
  {"\\)", ')'}                   // 右括号
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

/* PA 1: 求值辅助函数：
 * 检查 p..q 是否被一对最外层括号完整包围。
 * 若括号本身不匹配，则通过 bad 告诉上层这是非法表达式。
 */
static bool check_parentheses(int p, int q, bool *bad) {
  int i;
  int balance = 0;

  *bad = false;

  if (p > q) {
    *bad = true;
    return false;
  }

  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
    }
    else if (tokens[i].type == ')') {
      balance --;
      if (balance < 0) {
        *bad = true;
        return false;
      }
    }
  }

  if (balance != 0) {
    *bad = true;
    return false;
  }

  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  balance = 0;
  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      balance ++;
    }
    else if (tokens[i].type == ')') {
      balance --;
    }

    /* 最外层括号若在 q 之前就闭合，说明不是单独包围整个子表达式。 */
    if (balance == 0 && i < q) {
      return false;
    }
  }

  return true;
}

/* PA 1: 辅助函数：返回运算符优先级，数值越小表示优先级越低。 */
static int get_precedence(int type) {
  switch (type) {
    case TK_OR:
      return 1;
    case TK_AND:
      return 2;
    case TK_EQ:
    case TK_NEQ:
      return 3;
    case '+':
    case '-':
      return 4;
    case '*':
    case '/':
      return 5;
    case TK_NEG:
    case TK_DEREF:
    case TK_NOT:
      return 6;
    default:
      return 0;
  }
}

/* PA 1: 辅助函数：判断一个 token 是否能作为完整子表达式的结尾。 */
static bool is_value_token(int type) {
  return type == TK_NUM || type == TK_HEX || type == TK_REG || type == ')';
}

/* PA 1: 辅助函数：判断 token 是否属于右结合的单目运算符。 */
static bool is_unary_operator(int type) {
  return type == TK_NEG || type == TK_DEREF || type == TK_NOT;
}

/* PA 1: 求值辅助函数：
 * 词法分析结束后，根据上下文区分二元和单目运算符。
 * '-' 在表达式开头，或前一个 token 不是“值结尾”时，解释成单目负号。
 * '*' 在表达式开头，或前一个 token 不是“值结尾”时，解释成解引用。
 */
static void preprocess_tokens(void) {
  int i;

  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '-') {
      if (i == 0 || !is_value_token(tokens[i - 1].type)) {
        tokens[i].type = TK_NEG;
      }
    }
    else if (tokens[i].type == '*') {
      if (i == 0 || !is_value_token(tokens[i - 1].type)) {
        tokens[i].type = TK_DEREF;
      }
    }
  }
}

/* PA 1: 求值辅助函数：
 * 解析寄存器 token，对应寄存器值按 uint32_t 返回。
 * 当前支持 32 位、16 位、8 位通用寄存器以及 eip。
 */
static uint32_t read_register(const char *reg, bool *success) {
  int i;
  const char *name = reg + 1;

  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsl[i]) == 0) {
      return reg_l(i);
    }
  }

  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsw[i]) == 0) {
      return reg_w(i);
    }
  }

  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsb[i]) == 0) {
      return reg_b(i);
    }
  }

  if (strcmp(name, "eip") == 0) {
    return cpu.eip;
  }

  printf("bad expression: unknown register '%s'\n", reg);
  *success = false;
  return 0;
}

/* PA 1: 辅助函数：读取单个字面量或寄存器 token 的值。 */
static uint32_t eval_single_token(int p, bool *success) {
  switch (tokens[p].type) {
    case TK_NUM:
      return strtoul(tokens[p].str, NULL, 10);
    case TK_HEX:
      return strtoul(tokens[p].str, NULL, 16);
    case TK_REG:
      return read_register(tokens[p].str, success);
    default:
      printf("bad expression: token '%s' is not a value\n", tokens[p].str);
      *success = false;
      return 0;
  }
}

/* PA 1: 求值辅助函数：
 * 在 p..q 中寻找 dominant operator。
 * 只考虑括号外层的运算符。
 * 二元运算符同优先级取最右边，以符合左结合。
 * 单目运算符同优先级取最左边，以符合右结合。
 */
static int find_dominant_operator(int p, int q) {
  int i;
  int op = -1;
  int min_precedence = 0x7fffffff;
  int balance = 0;

  for (i = p; i <= q; i ++) {
    int cur_precedence;

    if (tokens[i].type == '(') {
      balance ++;
      continue;
    }

    if (tokens[i].type == ')') {
      balance --;
      continue;
    }

    if (balance > 0) {
      continue;
    }

    cur_precedence = get_precedence(tokens[i].type);
    if (cur_precedence == 0) {
      continue;
    }

    if (cur_precedence < min_precedence) {
      min_precedence = cur_precedence;
      op = i;
      continue;
    }

    if (cur_precedence == min_precedence) {
      if (is_unary_operator(tokens[i].type)) {
        continue;
      }
      op = i;
    }
  }

  return op;
}

/* PA 1: 表达式求值主函数：
 * 用分治递归地求解 p..q 对应的 token 子表达式。 */
static uint32_t eval(int p, int q, bool *success) {
  bool bad = false;
  uint32_t val1, val2;
  int op;

  if (p > q) {
    printf("bad expression: empty subexpression\n");
    *success = false;
    return 0;
  }

  if (p == q) {
    return eval_single_token(p, success);
  }

  if (check_parentheses(p, q, &bad) == true) {
    /* 若整个表达式被一对最外层括号包围，直接去掉这一层。 */
    return eval(p + 1, q - 1, success);
  }

  if (bad) {
    printf("bad expression: parentheses are not matched\n");
    *success = false;
    return 0;
  }

  op = find_dominant_operator(p, q);
  if (op < 0) {
    printf("bad expression: cannot find dominant operator\n");
    *success = false;
    return 0;
  }

  /* 单目运算符只需要递归求右侧子表达式。 */
  if (tokens[op].type == TK_NEG) {
    val2 = eval(op + 1, q, success);
    if (*success == false) {
      return 0;
    }
    return 0 - val2;
  }

  if (tokens[op].type == TK_NOT) {
    val2 = eval(op + 1, q, success);
    if (*success == false) {
      return 0;
    }
    return !val2;
  }

  if (tokens[op].type == TK_DEREF) {
    val2 = eval(op + 1, q, success);
    if (*success == false) {
      return 0;
    }
    return vaddr_read(val2, 4);
  }

  val1 = eval(p, op - 1, success);
  if (*success == false) {
    return 0;
  }

  val2 = eval(op + 1, q, success);
  if (*success == false) {
    return 0;
  }

  /* 按 dominant operator 合并左右子表达式的值。 */
  switch (tokens[op].type) {
    case '+':
      return val1 + val2;
    case '-':
      return val1 - val2;
    case '*':
      return val1 * val2;
    case '/':
      if (val2 == 0) {
        printf("bad expression: divide by zero\n");
        *success = false;
        return 0;
      }
      return val1 / val2;
    case TK_EQ:
      return val1 == val2;
    case TK_NEQ:
      return val1 != val2;
    case TK_AND:
      return val1 && val2;
    case TK_OR:
      return val1 || val2;
    default:
      printf("bad expression: unsupported operator '%s'\n", tokens[op].str);
      *success = false;
      return 0;
  }
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* 识别出新 token 后，按规则把它记录到 tokens 数组中。 */
        switch (rules[i].token_type) {
          case TK_NOTYPE:
            /* 空白符不入表，直接跳过。 */
            break;
          default:
            /* 实验要求保持 KISS，token 数组或字符串溢出时直接终止。 */
            if (nr_token >= (int)(sizeof(tokens) / sizeof(tokens[0]))) {
              assert(0);
            }
            if (substr_len >= (int)sizeof(tokens[nr_token].str)) {
              assert(0);
            }

            /* 无论什么类型的 token，只要入表就保存对应子串。 */
            tokens[nr_token].type = rules[i].token_type;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* 空表达式不合法，直接返回失败。 */
  if (nr_token == 0) {
    printf("bad expression: empty expression\n");
    *success = false;
    return 0;
  }

  /* 在正式递归求值前，先统一识别单目负号和单目解引用。 */
  preprocess_tokens();

  /* 从整个 token 区间开始递归求值。 */
  *success = true;
  return eval(0, nr_token - 1, success);
}
