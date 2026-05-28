#include <am.h>
#include <x86.h>

static _RegSet* (*H)(_Event, _RegSet*) = NULL;

void vecsys();
void vectrap();
void vectimer();
void vecnull();

_RegSet* irq_handle(_RegSet *tf) {
  _RegSet *next = tf;
  if (H) {
    _Event ev;
    switch (tf->irq) {
      case 32: ev.event = _EVENT_IRQ_TIME; break;
      case 0x80: ev.event = _EVENT_SYSCALL; break;
      /* [PA 4] x86-nemu 约定 int $0x81 表示内核自陷 */
      case 0x81: ev.event = _EVENT_TRAP; break;
      default: ev.event = _EVENT_ERROR; break;
    }

    next = H(ev, tf);
    if (next == NULL) {
      next = tf;
    }
  }

  return next;
}

static GateDesc idt[NR_IRQ];

void _asye_init(_RegSet*(*h)(_Event, _RegSet*)) {
  // initialize IDT
  for (unsigned int i = 0; i < NR_IRQ; i ++) {
    idt[i] = GATE(STS_TG32, KSEL(SEG_KCODE), vecnull, DPL_KERN);
  }

  // -------------------- system call --------------------------
  idt[0x80] = GATE(STS_TG32, KSEL(SEG_KCODE), vecsys, DPL_USER);
  /* [PA 4] 32 号中断由 NEMU 的时钟设备触发。 */
  idt[32] = GATE(STS_TG32, KSEL(SEG_KCODE), vectimer, DPL_KERN);
  /* [PA 4] 为内核自陷保留 0x81 号中断向量 */
  idt[0x81] = GATE(STS_TG32, KSEL(SEG_KCODE), vectrap, DPL_KERN);

  set_idt(idt, sizeof(idt));

  // register event handler
  H = h;
}

_RegSet *_make(_Area stack, void *entry, void *arg) {
  return NULL;
}

void _trap() {
  /* [PA 4] 通过保留向量进入统一的陷入处理流程 */
  asm volatile("int $0x81");
}

int _istatus(int enable) {
  return 0;
}
