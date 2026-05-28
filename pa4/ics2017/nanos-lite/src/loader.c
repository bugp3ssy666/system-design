#include "common.h"
#include "fs.h"
#include "memory.h"

#define DEFAULT_ENTRY ((void *)0x8048000)

uintptr_t loader(_Protect *as, const char *filename) {
  /* [PA 4] 强制通过地址空间参数建立映射。 */
  assert(as != NULL);

  int fd = fs_open(filename, 0, 0);
  size_t size = fs_filesz(fd);

  uintptr_t va = (uintptr_t)DEFAULT_ENTRY;
  size_t offset = 0;
  while (offset < size) {
    void *pa = new_page();
    size_t len = size - offset;
    if (len > PGSIZE) {
      len = PGSIZE;
    }

    /* [PA 4] loader 运行在内核地址空间中，不能直接写用户虚拟地址，而是应该先填物理页再映射 */
    memset(pa, 0, PGSIZE);
    _map(as, (void *)(va + offset), pa);
    fs_read(fd, pa, len);
    offset += len;
  }

  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
