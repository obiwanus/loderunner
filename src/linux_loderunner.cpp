#include "loderunner_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
// #include <X11/Xos.h>
#include <stdio.h>

#include "loderunner.h"

global bool GlobalRunning;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  file_read_result Result = {};

  // HANDLE FileHandle = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, 0,
  //                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  // if (FileHandle != INVALID_HANDLE_VALUE) {
  //   LARGE_INTEGER FileSize;
  //   if (GetFileSizeEx(FileHandle, &FileSize)) {
  //     Result.MemorySize = FileSize.QuadPart;
  //     Result.Memory =
  //         VirtualAlloc(0, Result.MemorySize, MEM_COMMIT, PAGE_READWRITE);
  //     DWORD BytesRead = 0;

  //     ReadFile(FileHandle, Result.Memory, (u32)Result.MemorySize, &BytesRead,
  //              0);

  //     CloseHandle(FileHandle);

  //     return Result;
  //   } else {
  //     OutputDebugStringA("Cannot get file size\n");
  //     // GetLastError() should help
  //   }
  // } else {
  //   OutputDebugStringA("Cannot read from file\n");
  //   // GetLastError() should help
  // }

  return Result;
}

int main(int argc, char const *argv[]) {
  Display *display;
  Window window;
  XEvent event;
  int screen;

  display = XOpenDisplay(0);
  if (display == 0) {
    fprintf(stderr, "Cannot open display\n");
    return 1;
  }

  screen = DefaultScreen(display);
  window = XCreateSimpleWindow(display, RootWindow(display, screen), 300, 300,
                               500, 500, 1, BlackPixel(display, screen),
                               WhitePixel(display, screen));

  XSetStandardProperties(display,window, "My Window", "Hi!", None, NULL, 0, NULL);

  XSelectInput(display, window, ExposureMask | KeyPressMask);
  XMapWindow(display, window);

  while (1) {
    XNextEvent(display, &event);
    if (event.type == Expose) {
      XFillRectangle(display, window, DefaultGC(display, screen), 20, 20, 10,
                     10);
    }
    if (event.type == KeyPress) {
      break;
    }
  }

  XCloseDisplay(display);

  return 0;
}
