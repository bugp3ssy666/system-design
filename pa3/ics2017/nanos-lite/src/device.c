#include "common.h"

#define NAME(key) \
  [_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [_KEY_NONE] = "NONE",
  _KEYS(NAME)
};

/* [PA 3] 读取键盘输入事件，写入 buf 中并返回事件文本长度 */
size_t events_read(void *buf, size_t len) {
  char event[64];
  int key = _read_key();

  if (key == _KEY_NONE) {
    /* 没有按键输入时，/dev/events 暴露当前系统运行时间作为 timer 事件。 */
    snprintf(event, sizeof(event), "t %d\n", (int)_uptime());
  } else {
    bool down = (key & 0x8000) != 0;
    key &= ~0x8000;
    assert(key > _KEY_NONE && key < (int)(sizeof(keyname) / sizeof(keyname[0])));
    assert(keyname[key] != NULL);

    /* AM 用 0x8000 标记按下事件，Navy-apps 约定输出 kd/ku 加按键名 */
    snprintf(event, sizeof(event), "%s %s\n", down ? "kd" : "ku", keyname[key]);
  }

  size_t event_len = strlen(event);
  if (event_len > len) {
    event_len = len;
  }
  memcpy(buf, event, event_len);
  return event_len;
}

static char dispinfo[128] __attribute__((used));

void dispinfo_read(void *buf, off_t offset, size_t len) {
  assert(offset >= 0);
  assert((size_t)offset + len <= strlen(dispinfo));

  /* [PA 3] /proc/dispinfo 是内核提前生成的文本信息，读操作只需按文件偏移拷贝 */
  memcpy(buf, dispinfo + offset, len);
}

void fb_write(const void *buf, off_t offset, size_t len) {
  assert(offset >= 0);
  assert(offset % sizeof(uint32_t) == 0);
  assert(len % sizeof(uint32_t) == 0);

  /* [PA 3] /dev/fb 按 32-bit 像素线性排列，offset 先换算成像素序号再换算成屏幕坐标 */
  const uint32_t *pixels = (const uint32_t *)buf;
  size_t nr_pixels = len / sizeof(uint32_t);
  size_t pos = (size_t)offset / sizeof(uint32_t);

  while (nr_pixels > 0) {
    int x = pos % _screen.width;
    int y = pos / _screen.width;
    assert(y < _screen.height);

    size_t row_pixels = _screen.width - x;
    if (row_pixels > nr_pixels) {
      row_pixels = nr_pixels;
    }

    _draw_rect(pixels, x, y, (int)row_pixels, 1);
    pixels += row_pixels;
    pos += row_pixels;
    nr_pixels -= row_pixels;
  }
}

size_t dispinfo_size(void) {
  return strlen(dispinfo);
}

void init_device() {
  _ioe_init();

  /* [PA 3] Navy-apps 的 libndl 会从 /proc/dispinfo 读取 WIDTH/HEIGHT 两行文本 */
  snprintf(dispinfo, sizeof(dispinfo), "WIDTH:%d\nHEIGHT:%d\n",
      _screen.width, _screen.height);
}
