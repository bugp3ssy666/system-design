#include "common.h"

_RegSet* do_syscall(_RegSet *r);
_RegSet* schedule(_RegSet *prev);

static bool timer_logged = false;

static _RegSet* do_event(_Event e, _RegSet* r) {
  switch (e.event) {
    case _EVENT_SYSCALL:
      /* [PA 4] 调度改由时钟中断触发，系统调用只处理自身语义。 */
      return do_syscall(r);
    case _EVENT_IRQ_TIME:
      /* [PA 4] 时钟中断抢回控制权，在中断返回前进行进程调度。 */
      if (!timer_logged) {
        Log("收到时钟中断");
        timer_logged = true;
      }
      return schedule(r);
    case _EVENT_TRAP:
      /* [PA 4] 内核自陷用于触发第一次上下文切换。 */
      return schedule(r);
    default: panic("Unhandled event ID = %d", e.event);
  }

  return NULL;
}

void init_irq(void) {
  _asye_init(do_event);
}
