#include "proc.h"

#define MAX_NR_PROC 4
#define PAL_SCHEDULE_QUOTA 1000

static PCB pcb[MAX_NR_PROC];
static int nr_proc = 0;
PCB *current = NULL;
static int pal_schedule_count = 0;

uintptr_t loader(_Protect *as, const char *filename);

void load_prog(const char *filename) {
  int i = nr_proc ++;
  _protect(&pcb[i].as);

  uintptr_t entry = loader(&pcb[i].as, filename);

  // TODO: remove the following three lines after you have implemented _umake()
  // _switch(&pcb[i].as);
  // current = &pcb[i];
  // ((void (*)(void))entry)();

  _Area stack;
  stack.start = pcb[i].stack;
  stack.end = stack.start + sizeof(pcb[i].stack);

  pcb[i].tf = _umake(&pcb[i].as, stack, stack, (void *)entry, NULL, NULL);
}

_RegSet* schedule(_RegSet *prev) {
  assert(nr_proc > 0);

  if (current != NULL) {
    /* [PA 4] 保存当前进程这次陷入形成的现场位置。 */
    current->tf = prev;
  }

  if (nr_proc == 1) {
    current = &pcb[0];
  }
  /* [PA 4] 优先调度 PAL 多次，只偶尔调度 hello 确认它仍在运行。 */
  else if (pal_schedule_count < PAL_SCHEDULE_QUOTA) {
    current = &pcb[0];
    pal_schedule_count ++;
  }
  else {
    current = &pcb[1];
    pal_schedule_count = 0;
  }

  _switch(&current->as);
  return current->tf;
}
