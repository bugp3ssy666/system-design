#include "common.h"
#include "fs.h"

#define DEFAULT_ENTRY ((void *)0x4000000)

// [PA 3] loader 通过文件系统按文件名加载用户程序。
uintptr_t loader(_Protect *as, const char *filename) {
  (void)as;
  int fd = fs_open(filename, 0, 0);
  // [PA 3] 只读取被选中的文件，不再复制整个 ramdisk。
  fs_read(fd, DEFAULT_ENTRY, fs_filesz(fd));
  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
