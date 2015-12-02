#include "loderunner_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
// #include <X11/Xos.h>
#include <stdio.h>

#if BUILD_SLOW
#include <signal.h>
#endif

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
  KeySym key;
  char buf[256];
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

  XSetStandardProperties(display, window, "My Window", "Hi!", None, NULL, 0,
                         NULL);

  XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask);
  XMapWindow(display, window);

  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

  GlobalRunning = true;

  while (GlobalRunning) {
    XNextEvent(display, &event);
    if (event.type == Expose) {
      XFillRectangle(display, window, DefaultGC(display, screen), 20, 20, 10,
                     10);
    }
    if (event.type == KeyPress &&
        XLookupString(&event.xkey, buf, 255, &key, 0) == 1) {
      char symbol = buf[0];
      if (symbol == 'q') {
        GlobalRunning = false;
      }
    }
    if (event.type == ButtonPress) {
      printf("Click at (%i, %i)\n", event.xbutton.x, event.xbutton.y);
    }
    if (event.type == ClientMessage) {
      if (event.xclient.data.l[0] == wmDeleteMessage) {
        GlobalRunning = false;
      }
    }
  }

  XCloseDisplay(display);

  return 0;
}
