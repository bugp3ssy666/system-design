#include "nemu.h"
#include "device/mmio.h"
#include "memory/mmu.h"

#define PMEM_SIZE (128 * 1024 * 1024)

#define pmem_rw(addr, type) *(type *)({\
    Assert(addr < PMEM_SIZE, "physical address(0x%08x) is out of bound", addr); \
    guest_to_host(addr); \
    })

uint8_t pmem[PMEM_SIZE];

/* Memory accessing interfaces */

uint32_t paddr_read(paddr_t addr, int len) {
  /* PA 2: 物理地址访问接口
   * 物理地址可能落在设备通过 MMIO 映射出来的空间中。
   * 如果 is_mmio() 找到了映射，就交给设备的 MMIO 读接口处理；
   * 否则才把它当作普通物理内存 pmem 来访问。
   */
  int map_NO = is_mmio(addr);
  if (map_NO != -1) {
    return mmio_read(addr, len, map_NO);
  }

  return pmem_rw(addr, uint32_t) & (~0u >> ((4 - len) << 3));
}

void paddr_write(paddr_t addr, int len, uint32_t data) {
  /* PA 2: 物理地址访问接口
   * MMIO 写入不能直接写 pmem，因为目标可能是显存等设备寄存器/缓冲区。
   * 命中映射时调用 mmio_write()，由对应设备的回调完成必要处理。
   */
  int map_NO = is_mmio(addr);
  if (map_NO != -1) {
    mmio_write(addr, len, data, map_NO);
    return;
  }

  memcpy(guest_to_host(addr), &data, len);
}

uint32_t vaddr_read(vaddr_t addr, int len) {
  assert(len >= 1 && len <= 4);

  if (!cpu.cr0.paging) {
    return paddr_read(addr, len);
  }

  /* [PA 4] 非跨页访问仍走单次翻译；跨页访问在下面拆成两段 */
  if ((addr & PAGE_MASK) + len <= PAGE_SIZE) {
    return paddr_read(page_translate(addr, false), len);
  }

  /* [PA 4] i386 允许非对齐访问；跨页时分别翻译两页，再按小端序拼回结果 */
  int left = PAGE_SIZE - (addr & PAGE_MASK);
  int right = len - left;
  uint32_t low = paddr_read(page_translate(addr, false), left);
  uint32_t high = paddr_read(page_translate(addr + left, false), right);
  return low | (high << (left << 3));
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  assert(len >= 1 && len <= 4);

  if (!cpu.cr0.paging) {
    paddr_write(addr, len, data);
    return;
  }

  /* [PA 4] 非跨页写入仍走单次翻译；跨页写入在下面拆成两段 */
  if ((addr & PAGE_MASK) + len <= PAGE_SIZE) {
    paddr_write(page_translate(addr, true), len, data);
    return;
  }

  /* [PA 4] 跨页写入同样拆成低地址页和高地址页两段，保持小端字节顺序 */
  int left = PAGE_SIZE - (addr & PAGE_MASK);
  int right = len - left;
  uint32_t low_mask = (1u << (left << 3)) - 1;
  paddr_write(page_translate(addr, true), left, data & low_mask);
  paddr_write(page_translate(addr + left, true), right, data >> (left << 3));
}

/* [PA 4] 将虚拟地址 addr 转换为物理地址，is_write 指示访问类型以更新访问权限位 */
paddr_t page_translate(vaddr_t addr, bool is_write) {
  uint32_t dir_idx = (addr >> 22) & 0x3ff;
  uint32_t page_idx = (addr >> 12) & 0x3ff;
  uint32_t offset = addr & PAGE_MASK;

  paddr_t pdir_base = cpu.cr3.val & ~PAGE_MASK;
  paddr_t pde_addr = pdir_base + dir_idx * sizeof(PDE);
  PDE pde;
  pde.val = paddr_read(pde_addr, 4);

  /* 本阶段不实现 page fault 处理；遇到无效映射说明页表准备或翻译逻辑有误 */
  assert(pde.present);

  if (!pde.accessed) {
    pde.accessed = 1;
    paddr_write(pde_addr, 4, pde.val);
  }

  paddr_t ptab_base = pde.page_frame << 12;
  paddr_t pte_addr = ptab_base + page_idx * sizeof(PTE);
  PTE pte;
  pte.val = paddr_read(pte_addr, 4);
  assert(pte.present);

  if (!pte.accessed || (is_write && !pte.dirty)) {
    pte.accessed = 1;
    if (is_write) {
      pte.dirty = 1;
    }
    paddr_write(pte_addr, 4, pte.val);
  }

  return (pte.page_frame << 12) | offset;
}
