#include <x86.h>
#include <klib.h>

#define PG_ALIGN __attribute((aligned(PGSIZE)))

static PDE kpdirs[NR_PDE] PG_ALIGN;
static PTE kptabs[PMEM_SIZE / PGSIZE] PG_ALIGN;
static void* (*palloc_f)();
static void (*pfree_f)(void*);

_Area segments[] = {      // Kernel memory mappings
  {.start = (void*)0,          .end = (void*)PMEM_SIZE}
};

#define NR_KSEG_MAP (sizeof(segments) / sizeof(segments[0]))

void _pte_init(void* (*palloc)(), void (*pfree)(void*)) {
  palloc_f = palloc;
  pfree_f = pfree;

  int i;

  // make all PDEs invalid
  for (i = 0; i < NR_PDE; i ++) {
    kpdirs[i] = 0;
  }

  PTE *ptab = kptabs;
  for (i = 0; i < NR_KSEG_MAP; i ++) {
    uint32_t pdir_idx = (uintptr_t)segments[i].start / (PGSIZE * NR_PTE);
    uint32_t pdir_idx_end = (uintptr_t)segments[i].end / (PGSIZE * NR_PTE);
    for (; pdir_idx < pdir_idx_end; pdir_idx ++) {
      // fill PDE
      kpdirs[pdir_idx] = (uintptr_t)ptab | PTE_P;

      // fill PTE
      PTE pte = PGADDR(pdir_idx, 0, 0) | PTE_P;
      PTE pte_end = PGADDR(pdir_idx + 1, 0, 0) | PTE_P;
      for (; pte < pte_end; pte += PGSIZE) {
        *ptab = pte;
        ptab ++;
      }
    }
  }

  set_cr3(kpdirs);
  set_cr0(get_cr0() | CR0_PG);
}

void _protect(_Protect *p) {
  PDE *updir = (PDE*)(palloc_f());
  p->ptr = updir;
  // map kernel space
  for (int i = 0; i < NR_PDE; i ++) {
    updir[i] = kpdirs[i];
  }

  p->area.start = (void*)0x8000000;
  p->area.end = (void*)0xc0000000;
}

void _release(_Protect *p) {
}

void _switch(_Protect *p) {
  set_cr3(p->ptr);
}

/*[PA 4] 实现虚拟地址到物理地址的映射，建立用户程序的页表 */
void _map(_Protect *p, void *va, void *pa) {
  assert(p != NULL);
  uintptr_t vaddr = (uintptr_t)va;
  uintptr_t paddr = (uintptr_t)pa;
  assert((vaddr & (PGSIZE - 1)) == 0);
  assert((paddr & (PGSIZE - 1)) == 0);
  assert(vaddr >= (uintptr_t)p->area.start && vaddr < (uintptr_t)p->area.end);

  PDE *pdir = (PDE *)p->ptr;
  uint32_t pdir_idx = PDX(vaddr);
  uint32_t ptab_idx = PTX(vaddr);

  if ((pdir[pdir_idx] & PTE_P) == 0) {
    PTE *ptab = (PTE *)palloc_f();
    memset(ptab, 0, PGSIZE);

    /* 页目录/页表在内核地址空间中恒等映射，因此这里写入的指针值同时也是物理页号 */
    pdir[pdir_idx] = (uintptr_t)ptab | PTE_P;
  }

  PTE *ptab = (PTE *)PTE_ADDR(pdir[pdir_idx]);
  assert((ptab[ptab_idx] & PTE_P) == 0);

  /* 本阶段只需要建立映射，不实现权限保护；NEMU 侧只检查 present 位 */
  ptab[ptab_idx] = paddr | PTE_P;
}

void _unmap(_Protect *p, void *va) {
}

/* [PA 4] 人工构造陷阱帧，准备进入用户程序的初始上下文 */
_RegSet *_umake(_Protect *p, _Area ustack, _Area kstack, void *entry, char *const argv[], char *const envp[]) {
  /* 当前简化实现暂不区分用户栈和内核栈，也不传递 argv/envp。 */
  (void)p;
  (void)kstack;
  (void)argv;
  (void)envp;

  uintptr_t *sp = (uintptr_t *)ustack.end;

  /* 为 _start(argc, argv, envp) 准备初始栈帧；返回地址不会被使用。 */
  *--sp = 0;  // envp，环境变量指针
  *--sp = 0;  // argv，参数数组指针
  *--sp = 0;  // argc，参数个数
  *--sp = 0;  // 返回地址，不会被实际使用

  _RegSet *tf = (_RegSet *)((uintptr_t)sp - sizeof(_RegSet));
  assert((uintptr_t)tf >= (uintptr_t)ustack.start);
  memset(tf, 0, sizeof(*tf));

  /* iret 将从这个人工陷阱帧恢复到用户程序入口。 */
  tf->eip = (uintptr_t)entry;
  /* [PA 4] 初始现场需要打开 IF，用户进程运行时才能响应时钟中断。 */
  tf->cs = KSEL(SEG_KCODE);
  tf->eflags = 0x2 | FL_IF;
  tf->esp = (uintptr_t)sp;

  return tf;
}
