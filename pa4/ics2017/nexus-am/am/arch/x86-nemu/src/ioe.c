#include <am.h>
#include <x86.h>

#define RTC_PORT 0x48   // Note that this is not standard
#define I8042_DATA_PORT 0x60
#define I8042_STATUS_PORT 0x64
#define I8042_STATUS_HASKEY_MASK 0x1

static unsigned long boot_time;

void _ioe_init() {
  boot_time = inl(RTC_PORT);
}

unsigned long _uptime() {
  /* PA 2: RTC_PORT 返回 AM 程序启动后的运行时间（ms） */
  return inl(RTC_PORT) - boot_time;
}

uint32_t* const fb = (uint32_t *)0x40000;

_Screen _screen = {
  .width  = 400,
  .height = 300,
};

extern void* memcpy(void *, const void *, int);

void _draw_rect(const uint32_t *pixels, int x, int y, int w, int h) {
  /* PA 2: 绘制矩形接口
   * fb 指向 NEMU 中映射到 0x40000 的显存。
   * pixels 是一块 w*h 的连续像素数组，按行拷贝到屏幕上
   * 以 (x, y) 为左上角的矩形区域即可。
   */
  if (w <= 0 || h <= 0 || x < 0 || y < 0 ||
      x >= _screen.width || y >= _screen.height) {
    return;
  }

  int copy_w = w;
  if (x + copy_w > _screen.width) {
    copy_w = _screen.width - x;
  }

  for (int j = 0; j < h && y + j < _screen.height; j ++) {
    memcpy(&fb[(y + j) * _screen.width + x], pixels, copy_w * sizeof(uint32_t));
    pixels += w;
  }
}

void _draw_sync() {
}

int _read_key() {
  /* PA 2: 键盘输入接口
   * 先读状态端口：NEMU 的 i8042 用 bit 0 表示“有键盘事件”。
   * 如果没有事件，按照 AM 的约定返回 _KEY_NONE。
   */
  if ((inb(I8042_STATUS_PORT) & I8042_STATUS_HASKEY_MASK) == 0) {
    return _KEY_NONE;
  }

  // 再读数据端口：返回的就是 AM 键盘码，按下事件已经带有 0x8000 标志。
  return inl(I8042_DATA_PORT);
}
