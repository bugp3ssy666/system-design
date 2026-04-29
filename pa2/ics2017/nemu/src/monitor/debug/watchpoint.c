#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "monitor/monitor.h"

#define NR_WP 32

// 监视点池, 以及使用中的监视点链表和空闲的监视点链表
static WP wp_pool[NR_WP];
static WP *head, *free_;

// 初始化监视点池和链表
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

// PA 1: 跳过表达式前导空格, 便于 w 命令直接传入整段参数
static char *skip_expr_spaces(char *s) {
  while (s != NULL && *s == ' ') {
    s ++;
  }
  return s;
}

// PA 1: 根据监视点序号在使用中链表里查找对应节点
static WP *find_wp(int no) {
  WP *wp;
  for (wp = head; wp != NULL; wp = wp->next) {
    if (wp->NO == no) {
      return wp;
    }
  }

  return NULL;
}

// PA 1: 获取一个空闲监视点
WP* new_wp() {
  // 没有空闲监视点时直接中止, 防止后续解引用空指针
  assert(free_ != NULL);

  // 从空闲链表头部取出一个节点
  WP *wp = free_;
  free_ = free_->next;

  // 初始化业务字段, 避免复用旧节点时残留上一次监视点的信息
  wp->expr[0] = '\0';
  wp->last_value = 0;

  // 将新申请的节点挂到使用中链表头部, 便于后续统一遍历检查
  wp->next = head;
  head = wp;

  return wp;
}

// PA 1: 回收一个使用中的监视点
void free_wp(WP *wp) {
  assert(wp != NULL);

  // 在使用中链表中找到 wp, 同时记录它前一个节点以便卸下
  WP *prev = NULL;
  WP *cur = head;
  while (cur != NULL && cur != wp) {
    prev = cur;
    cur = cur->next;
  }

  // 找不到说明 wp 不是正在使用的监视点, 给出错误信息后直接返回
  if (cur == NULL) {
    uintptr_t wp_addr = (uintptr_t)wp;
    uintptr_t pool_begin = (uintptr_t)wp_pool;
    uintptr_t pool_end = (uintptr_t)(wp_pool + NR_WP);

    // 如果指针来自监视点池, 可以安全输出它的序号; 否则只报告非法指针
    if (wp_addr >= pool_begin && wp_addr < pool_end) {
      printf("\33[1;31mwatchpoint_error:\33[0m watchpoint #%d is not in use\n", wp->NO);
    }
    else {
      printf("\33[1;31mwatchpoint_error:\33[0m invalid watchpoint pointer %p\n", (void *)wp);
    }
    return;
  }

  // 从使用中链表摘下 wp
  if (prev == NULL) {
    head = wp->next;
  }
  else {
    prev->next = wp->next;
  }

  // 将 wp 归还到空闲链表头部, 之后可再次被 new_wp() 分配
  wp->next = free_;
  free_ = wp;
}

// 以下是对外功能接口
// PA 1: 设置一个新的监视点, 保存表达式 EXPR 和它当前的值
bool set_watchpoint(char *expr_str) {
  expr_str = skip_expr_spaces(expr_str);
  if (expr_str == NULL || *expr_str == '\0') {
    printf("\33[1;31mwatchpoint_error:\33[0m Missing expression\n");
    return false;
  }

  // 拷贝并去掉末尾空格, 让 info w 输出的表达式更清楚
  size_t len = strlen(expr_str);
  while (len > 0 && expr_str[len - 1] == ' ') {
    len --;
  }

  if (len == 0) {
    printf("\33[1;31mwatchpoint_error:\33[0m Missing expression\n");
    return false;
  }
  if (len >= WP_EXPR_LEN) {
    printf("\33[1;31mwatchpoint_error:\33[0m Expression is too long, max length is %d\n", WP_EXPR_LEN - 1);
    return false;
  }

  char saved_expr[WP_EXPR_LEN];
  memcpy(saved_expr, expr_str, len);
  saved_expr[len] = '\0';

  // expr() 接收可写字符串, 这里使用副本求值, 避免破坏保存下来的表达式文本
  char eval_expr[WP_EXPR_LEN];
  strcpy(eval_expr, saved_expr);

  bool success = true;
  uint32_t val = expr(eval_expr, &success);
  if (!success) {
    printf("\33[1;31mwatchpoint_error:\33[0m Bad expression\n");
    return false;
  }

  WP *wp = new_wp();
  strcpy(wp->expr, saved_expr);
  wp->last_value = val;

  printf("Watchpoint %d: %s = %u (0x%08x)\n", wp->NO, wp->expr, val, val);
  return true;
}

// PA 1: 按序号删除监视点
bool delete_watchpoint(int no) {
  WP *wp = find_wp(no);
  if (wp == NULL) {
    printf("\33[1;31mwatchpoint_error:\33[0m Watchpoint %d not found\n", no);
    return false;
  }

  free_wp(wp);
  printf("Deleted watchpoint %d\n", no);
  return true;
}

// PA 1: 打印当前使用中的监视点信息
void info_watchpoints() {
  WP *wp;
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("Num     Type           Disp Enb Expr                 Value\n");
  for (wp = head; wp != NULL; wp = wp->next) {
    printf("%-7d %-14s %-4s %-3s %-20s %u (0x%08x)\n",
        wp->NO, "watchpoint", "keep", "y", wp->expr, wp->last_value, wp->last_value);
  }
}

// PA 1: 每执行完一条指令后检查所有监视点, 任意一个变化都暂停 NEMU
bool check_watchpoints() {
  bool triggered = false;
  WP *wp;

  for (wp = head; wp != NULL; wp = wp->next) {
    bool success = true;
    // 使用表达式副本求值, 避免表达式求值过程影响监视点中保存的原字符串
    char eval_expr[WP_EXPR_LEN];
    strcpy(eval_expr, wp->expr);
    uint32_t new_value = expr(eval_expr, &success);

    // 表达式曾经合法但现在求值失败时也暂停, 方便用户立刻排查
    if (!success) {
      printf("\33[1;31mwatchpoint_error:\33[0m Failed to evaluate watchpoint %d: %s\n",
          wp->NO, wp->expr);
      triggered = true;
      continue;
    }

    if (new_value != wp->last_value) {
      printf("\33[1;31mWatchpoint %d triggered:\33[0m %s\n", wp->NO, wp->expr);
      printf("Old value = %u (0x%08x)\n", wp->last_value, wp->last_value);
      printf("New value = %u (0x%08x)\n", new_value, new_value);

      // 触发后更新保存值, 避免继续运行时因同一次变化反复停下
      wp->last_value = new_value;
      triggered = true;
    }
  }

  if (triggered) {
    // 可能同一条指令触发多个监视点, 上面会全部打印后再统一暂停
    nemu_state = NEMU_STOP;
  }

  return triggered;
}
