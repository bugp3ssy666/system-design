#include "common.h"
#include "fs.h"
#include "syscall.h"

int mm_brk(uint32_t new_brk);

_RegSet* do_syscall(_RegSet *r) {
  uintptr_t a[4];
  a[0] = SYSCALL_ARG1(r);
  a[1] = SYSCALL_ARG2(r);
  a[2] = SYSCALL_ARG3(r);
  a[3] = SYSCALL_ARG4(r);

  uintptr_t ret = 0;

  switch (a[0]) {
    case SYS_none:
      /* [PA 3] SYS_none 不执行实际动作，按约定直接返回 1 */
      ret = 1;
      break;
    case SYS_open:
      ret = fs_open((const char *)a[1], a[2], a[3]);
      break;
    case SYS_read:
      ret = fs_read(a[1], (void *)a[2], a[3]);
      break;
    case SYS_write:
      /* [PA 3] write(fd, buf, len): 统一交给文件系统处理普通文件和 stdout/stderr */
      ret = fs_write(a[1], (const void *)a[2], a[3]);
      break;
    case SYS_brk:
      /* [PA 3] brk(addr): addr 是新的 program break；当前单任务内核直接接受请求 */
      ret = mm_brk(a[1]);
      break;
    case SYS_exit:
      /* [PA 3] SYS_exit 使用第一个参数作为退出状态并结束当前程序 */
      _halt(a[1]);
      break;
    case SYS_close:
      ret = fs_close(a[1]);
      break;
    case SYS_lseek:
      /* [PA 3] lseek(fd, offset, whence): 调整文件 open_offset 并返回新位置。 */
      ret = fs_lseek(a[1], (off_t)a[2], a[3]);
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

  /* [PA 3] 返回值写回保存系统调用号的 eax，popa 后用户程序即可读到 */
  SYSCALL_ARG1(r) = ret;

  return NULL;
}
