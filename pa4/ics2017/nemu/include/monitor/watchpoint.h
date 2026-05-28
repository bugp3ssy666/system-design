#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

#define WP_EXPR_LEN 256

typedef struct watchpoint {
  int NO;                     // 监视点序号
  struct watchpoint *next;    // 指向下一个监视点的指针

  char expr[WP_EXPR_LEN];     // 被监视的表达式字符串
  uint32_t last_value;        // 表达式上一次求值的结果, 用来判断是否发生变化
} WP;

// 从监视点池中申请/释放一个监视点节点, 供设置和删除监视点时调用
WP* new_wp();
void free_wp(WP *wp);

/* PA 1: 对外部代码：
 * 监视点功能接口——
 * 设置、删除、打印和执行后检查
 */
bool set_watchpoint(char *expr_str);
bool delete_watchpoint(int no);
void info_watchpoints();
bool check_watchpoints();

#endif
