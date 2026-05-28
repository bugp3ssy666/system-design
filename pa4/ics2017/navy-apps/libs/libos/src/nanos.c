#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include "syscall.h"

// TODO: discuss with syscall interface
#ifndef __ISA_NATIVE__

// FIXME: this is temporary

int _syscall_(int type, uintptr_t a0, uintptr_t a1, uintptr_t a2){
  int ret = -1;
  asm volatile("int $0x80": "=a"(ret): "a"(type), "b"(a0), "c"(a1), "d"(a2));
  return ret;
}

void _exit(int status) {
  _syscall_(SYS_exit, status, 0, 0);
}

int _open(const char *path, int flags, mode_t mode) {
  /* [PA 3] open() 通过系统调用进入 Nanos-lite 的简易文件系统。 */
  return _syscall_(SYS_open, (uintptr_t)path, flags, mode);
}

int _write(int fd, const void *buf, size_t count){
  /* [PA 3] newlib 的 write()/printf() 最终会落到这里，再通过 int 0x80 进入 Nanos-lite */
  return _syscall_(SYS_write, fd, (uintptr_t)buf, count);
}

extern char _end;

void *_sbrk(intptr_t increment){
  static char *program_break = NULL;

  if (program_break == NULL) {
    /* [PA 3] _end 由链接脚本提供，表示用户程序静态数据区之后的初始 program break */
    program_break = &_end;
  }

  char *old_break = program_break;
  char *new_break = program_break + increment;

  if (_syscall_(SYS_brk, (uintptr_t)new_break, 0, 0) == 0) {
    program_break = new_break;
    return old_break;
  }

  return (void *)-1;
}

int _read(int fd, void *buf, size_t count) {
  /* [PA 3] read() 最终交给内核从文件系统中取数据。 */
  return _syscall_(SYS_read, fd, (uintptr_t)buf, count);
}

int _close(int fd) {
  /* [PA 3] close() 在当前简易文件系统里总是成功。 */
  return _syscall_(SYS_close, fd, 0, 0);
}

off_t _lseek(int fd, off_t offset, int whence) {
  /* [PA 3] lseek() 交给内核调整文件偏移。 */
  return _syscall_(SYS_lseek, fd, (uintptr_t)offset, whence);
}

// The code below is not used by Nanos-lite.
// But to pass linking, they are defined as dummy functions

// not implement but used
int _fstat(int fd, struct stat *buf) {
  return 0;
}

int execve(const char *fname, char * const argv[], char *const envp[]) {
  assert(0);
  return -1;
}

int _execve(const char *fname, char * const argv[], char *const envp[]) {
  return execve(fname, argv, envp);
}

int _kill(int pid, int sig) {
  _exit(-SYS_kill);
  return -1;
}

pid_t _getpid() {
  _exit(-SYS_getpid);
  return 1;
}

char **environ;

time_t time(time_t *tloc) {
  assert(0);
  return 0;
}

int signal(int num, void *handler) {
  assert(0);
  return -1;
}

pid_t _fork() {
  assert(0);
  return -1;
}

int _link(const char *d, const char *n) {
  assert(0);
  return -1;
}

int _unlink(const char *n) {
  assert(0);
  return -1;
}

pid_t _wait(int *status) {
  assert(0);
  return -1;
}

clock_t _times(void *buf) {
  assert(0);
  return 0;
}

int _gettimeofday(struct timeval *tv) {
  assert(0);
  tv->tv_sec = 0;
  tv->tv_usec = 0;
  return 0;
}

int _fcntl(int fd, int cmd, ... ) {
  assert(0);
  return 0;
}

int pipe(int pipefd[2]) {
  assert(0);
  return 0;
}

int dup(int oldfd) {
  assert(0);
  return 0;
}

int dup2(int oldfd, int newfd) {
  assert(0);
  return 0;
}

pid_t vfork(void) {
  assert(0);
  return 0;
}

#endif
