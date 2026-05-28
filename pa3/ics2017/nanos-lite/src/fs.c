#include "fs.h"

typedef struct {
  char *name;
  size_t size;
  off_t disk_offset;
  off_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB, FD_EVENTS, FD_DISPINFO, FD_NORMAL};

/* 文件记录表记录文件名、大小和在 ramdisk 中的偏移。 */
static Finfo file_table[] __attribute__((used)) = {
  {"stdin (note that this is not the actual stdin)", 0, 0},
  {"stdout (note that this is not the actual stdout)", 0, 0},
  {"stderr (note that this is not the actual stderr)", 0, 0},
  [FD_FB] = {"/dev/fb", 0, 0},
  [FD_EVENTS] = {"/dev/events", 0, 0},
  [FD_DISPINFO] = {"/proc/dispinfo", 128, 0},
#include "files.h"
};

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

void ramdisk_read(void *buf, off_t offset, size_t len);
void ramdisk_write(const void *buf, off_t offset, size_t len);
size_t events_read(void *buf, size_t len);
void dispinfo_read(void *buf, off_t offset, size_t len);
void fb_write(const void *buf, off_t offset, size_t len);
size_t dispinfo_size(void);

void init_fs() {
  /* [PA 3] /dev/fb 不在 ramdisk 中，大小由当前屏幕像素数和每像素 4 字节决定 */
  file_table[FD_FB].size = (size_t)_screen.width * _screen.height * sizeof(uint32_t);

  /* [PA 3] /proc/dispinfo 也是虚拟文件，只暴露实际文本长度，避免用户读到尾部 '\0' */
  file_table[FD_DISPINFO].size = dispinfo_size();
}

/* [PA 3] 从文件表中找到对应文件描述符的记录，并返回指针 */
static Finfo* get_file(int fd) {
  assert(fd >= 0 && fd < (int)NR_FILES);
  return &file_table[fd];
}

/* [PA 3] 查找文件表，找到对应文件名的记录，并返回文件描述/即文件表中的索引 */
int fs_open(const char *pathname, int flags, int mode) {
  (void)flags;
  (void)mode;

  for (int i = 0; i < (int)NR_FILES; i ++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }

  assert(0);
  return -1;
}

/* [PA 3] 从文件表中找到对应文件描述符的记录，读取数据到 buf 中，并更新 open_offset */
ssize_t fs_read(int fd, void *buf, size_t len) {
  Finfo *file = get_file(fd);

  if (fd == FD_EVENTS) {
    /* /dev/events 是输入事件流，没有固定文件大小，直接交给设备层生成一条事件 */
    return events_read(buf, len);
  }

  if (fd == FD_DISPINFO) {
    assert(file->open_offset >= 0 && (size_t)file->open_offset <= file->size);

    /* [PA 3] /proc/dispinfo 的内容来自 device.c 中预先生成的字符串 */
    size_t left = file->size - file->open_offset;
    if (len > left) {
      len = left;
    }

    dispinfo_read(buf, file->open_offset, len);
    file->open_offset += len;
    return len;
  }

  if (fd < FD_NORMAL) {
    return 0;
  }

  assert(file->open_offset >= 0 && (size_t)file->open_offset <= file->size);

  // 简易文件系统中文件大小固定，读取时不能越过文件边界
  size_t left = file->size - file->open_offset;
  if (len > left) {
    len = left;
  }

  ramdisk_read(buf, file->disk_offset + file->open_offset, len);
  file->open_offset += len;
  return len;
}

/* [PA 3] 从 buf 写入 fd 指向的文件，并像 read 一样维护 open_offset */
ssize_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *file = get_file(fd);

  if (fd == FD_STDOUT || fd == FD_STDERR) {
    for (size_t i = 0; i < len; i ++) {
      _putc(((const char *)buf)[i]);
    }
    return len;
  }

  if (fd == FD_FB) {
    assert(file->open_offset >= 0 && (size_t)file->open_offset <= file->size);

    /* [PA 3] 对 /dev/fb 的写入重定向到 VGA，仍然复用文件偏移和边界裁剪语义 */
    size_t left = file->size - file->open_offset;
    if (len > left) {
      len = left;
    }

    fb_write(buf, file->open_offset, len);
    file->open_offset += len;
    return len;
  }

  if (fd < FD_NORMAL) {
    return 0;
  }

  assert(file->open_offset >= 0 && (size_t)file->open_offset <= file->size);

  // 文件大小固定，写入时不能越过原文件边界
  size_t left = file->size - file->open_offset;
  if (len > left) {
    len = left;
  }

  ramdisk_write(buf, file->disk_offset + file->open_offset, len);
  file->open_offset += len;
  return len;
}

/* [PA 3] 调整 fd 指向文件的 open_offset，支持 SEEK_SET/SEEK_CUR/SEEK_END */
off_t fs_lseek(int fd, off_t offset, int whence) {
  Finfo *file = get_file(fd);

  if (fd < FD_NORMAL && fd != FD_FB && fd != FD_DISPINFO) {
    return 0;
  }

  off_t new_offset = 0;
  switch (whence) {
    case SEEK_SET:
      new_offset = offset;
      break;
    case SEEK_CUR:
      new_offset = file->open_offset + offset;
      break;
    case SEEK_END:
      new_offset = file->size + offset;
      break;
    default:
      assert(0);
  }

  if (new_offset < 0) {
    new_offset = 0;
  }
  if ((size_t)new_offset > file->size) {
    new_offset = file->size;
  }

  file->open_offset = new_offset;
  return new_offset;
}

/* [PA 3] 从文件表中找到对应文件描述符的记录，并返回成功关闭的状态 */
int fs_close(int fd) {
  (void)get_file(fd);
  return 0;
}

/* [PA 3] 从文件表中找到对应文件描述符的记录，并返回文件大小 */
size_t fs_filesz(int fd) {
  return get_file(fd)->size;
}
