#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* 跳过命令参数前面的空格, 便于后续统一解析。 */
static char *skip_spaces(char *s) {
  while (s != NULL && *s == ' ') {
    s ++;
  }
  return s;
}

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

/* PA 1: {si [N]} - 单步执行 N 条指令; 若 N 缺省, 默认执行 1 条。 */
static int cmd_si(char *args) {
  /* 缺省情况下执行 1 条指令。 */
  uint64_t n = 1;

  if (args != NULL) {
    /* 解析 si 后继第一个参数, 并检查是否还存在多余参数。 */
    char *arg = strtok(args, " ");
    char *extra = strtok(NULL, " ");

    if (arg == NULL) {
      cpu_exec(n);
      return 0;
    }

    /* si 只接受一个正整数参数。 */
    if (extra != NULL || strspn(arg, "0123456789") != strlen(arg)) {
      printf("Usage: si [N]\n");
      return 0;
    }

    n = strtoull(arg, NULL, 10);
    /* 执行条数必须大于 0 */
    if (n <= 0) {
      printf("\33[1;31mcmd_error:\33[0m [N] should be greater than 0\n");
      return 0;
    }
  }

  /* 复用已有的执行入口, 执行指定条数后返回到监视器。 */
  cpu_exec(n);
  return 0;
}

/* PA 1: {info r/info w} - 打印寄存器或监视点信息。 */
static int cmd_info(char *args) {
  args = skip_spaces(args);
  if (args == NULL) {
    printf("\33[1;31mcmd_error:\33[0m Unknown info command\n\33[1;30minfo r - print registers\ninfo w - print watchpoints\33[0m\n");
    return 0;
  }
  if (*args == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Unknown info command\n\33[1;30minfo r - print registers\ninfo w - print watchpoints\33[0m\n");
    return 0;
  }

  /* 解析 info 的子命令, 当前支持 r 和 w。 */
  char *subcmd = strtok(args, " ");
  char *extra = strtok(NULL, " ");

  if (extra != NULL) {
    printf("\33[1;31mcmd_error:\33[0m Unknown info command\n\33[1;30minfo r - print registers\ninfo w - print watchpoints\33[0m\n");
    return 0;
  }

  /* 打印寄存器分支 */
  if (strcmp(subcmd, "r") == 0) {
    int i;
    /* 依次输出 8 个通用寄存器, 再输出 eip。 */
    for (i = 0; i < 8; i ++) {
      printf("%s\t0x%08x\n", reg_name(i, 4), reg_l(i));
    }
    printf("eip\t0x%08x\n", cpu.eip);
    return 0;
  }

  /* 打印监视点分支 */
  if (strcmp(subcmd, "w") == 0) {
    /* 打印所有正在使用的监视点。 */
    info_watchpoints();
    return 0;
  }

  printf("\33[1;31mcmd_error:\33[0m Unknown info command '%s'\n\33[1;30minfo r - print registers\ninfo w - print watchpoints\33[0m\n", subcmd);
  return 0;
}

/* PA 1: {p EXPR} - 对表达式 EXPR 求值并输出结果。 */
static int cmd_p(char *args) {
  bool success = true;
  uint32_t val;

  args = skip_spaces(args);
  if (args == NULL || *args == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Wrong p command format\n\33[1;30mp EXPR - evaluate expression\33[0m\n");
    return 0;
  }

  /* 调用表达式求值入口, 支持当前 expr.c 已实现的所有表达式类型。 */
  val = expr(args, &success);
  if (!success) {
    printf("\33[1;31mcmd_error:\33[0m Bad expression\n");
    return 0;
  }

  /* 同时输出十进制和十六进制结果, 便于调试观察。 */
  printf("%u (0x%08x)\n", val, val);
  return 0;
}

/* PA 1: {x N EXPR} - 先对 EXPR 求值, 再从该地址开始扫描连续 N 个 4 字节单元。 */
static int cmd_x(char *args) {
  char *n_str;
  char *expr_str;
  if (args == NULL) {
    printf("\33[1;31mcmd_error:\33[0m Wrong x command format\n\33[1;30mx N EXPR - scan memory from evaluated address\33[0m\n");
    return 0;
  }

  args = skip_spaces(args);
  if (*args == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Wrong x command format\n\33[1;30mx N EXPR - scan memory from evaluated address\33[0m\n");
    return 0;
  }

  /* x 的第一个参数是扫描个数 N, 其余整串都作为表达式 EXPR 传给 expr(). */
  n_str = strtok(args, " ");
  uint64_t n;
  vaddr_t addr;
  int i;
  bool success = true;

  if (n_str == NULL) {
    printf("\33[1;31mcmd_error:\33[0m Wrong x command format\n\33[1;30mx N EXPR - scan memory from evaluated address\33[0m\n");
    return 0;
  }

  expr_str = n_str + strlen(n_str) + 1;
  expr_str = skip_spaces(expr_str);
  if (*expr_str == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Wrong x command format\n\33[1;30mx N EXPR - scan memory from evaluated address\33[0m\n");
    return 0;
  }

  /* 检查 N 必须是一个正整数, 表示需要输出多少个 4 字节单元。 */
  if (strspn(n_str, "0123456789") != strlen(n_str)) {
    printf("\33[1;31mcmd_error:\33[0m [N] should be a positive decimal integer\n");
    return 0;
  }
  n = strtoull(n_str, NULL, 10);
  if (n == 0) {
    printf("\33[1;31mcmd_error:\33[0m [N] should be greater than 0\n");
    return 0;
  }

  /* 由 expr() 统一求 EXPR 的值, 可兼容寄存器、十六进制、逻辑/算术表达式等写法。 */
  addr = expr(expr_str, &success);
  if (!success) {
    printf("\33[1;31mcmd_error:\33[0m Bad expression\n");
    return 0;
  }

  /* 从起始地址开始, 每次读取 4 字节并按十六进制输出。 */
  for (i = 0; i < (int)n; i ++) {
    vaddr_t cur_addr = addr + i * 4;
    uint32_t data = vaddr_read(cur_addr, 4);
    printf("0x%08x: 0x%08x\n", cur_addr, data);
  }

  return 0;
}

/* PA 1: {w EXPR} - 设置监视点, 当 EXPR 的值变化时暂停执行。 */
static int cmd_w(char *args) {
  args = skip_spaces(args);
  if (args == NULL || *args == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Wrong w command format\n\33[1;30mw EXPR - set watchpoint\33[0m\n");
    return 0;
  }

  set_watchpoint(args);
  return 0;
}

/* PA 1: {d N} - 删除序号为 N 的监视点。 */
static int cmd_d(char *args) {
  args = skip_spaces(args);
  if (args == NULL || *args == '\0') {
    printf("\33[1;31mcmd_error:\33[0m Wrong d command format\n\33[1;30md N - delete watchpoint N\33[0m\n");
    return 0;
  }

  char *no_str = strtok(args, " ");
  char *extra = strtok(NULL, " ");
  if (no_str == NULL || extra != NULL || strspn(no_str, "0123456789") != strlen(no_str)) {
    printf("\33[1;31mcmd_error:\33[0m Wrong d command format\n\33[1;30md N - delete watchpoint N\33[0m\n");
    return 0;
  }

  uint64_t no = strtoull(no_str, NULL, 10);
  if (no > 0x7fffffff) {
    printf("\33[1;31mcmd_error:\33[0m Watchpoint number is too large\n");
    return 0;
  }

  delete_watchpoint((int)no);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  /* PA 1: 新注册 si 指令 */
  { "si", "Step through [N] instructions, default by 1", cmd_si },
  /* PA 1: 新注册 info 指令, 用于打印寄存器或监视点信息。 */
  { "info", "Print program status information", cmd_info },
  /* PA 1: 新注册 p 指令, 用于对表达式求值。 */
  { "p", "Evaluate expression: p EXPR", cmd_p },
  /* PA 1: x 指令升级为对 EXPR 求值后再扫描内存。 */
  { "x", "Scan memory: x N EXPR", cmd_x },
  /* PA 1: 新注册 w 指令, 用于设置表达式监视点。 */
  { "w", "Set watchpoint: w EXPR", cmd_w },
  /* PA 1: 新注册 d 指令, 用于删除指定序号的监视点。 */
  { "d", "Delete watchpoint: d N", cmd_d },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

