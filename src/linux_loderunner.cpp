#include "loderunner_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
// #include <X11/Xos.h>
#include <dlfcn.h>
#include <stdio.h>

#if BUILD_SLOW
#include <signal.h>
#endif

#include "loderunner.h"

global bool GlobalRunning;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;

struct linux_game_code {
  void *Library;
  game_update_and_render *UpdateAndRender;
  bool32 IsValid;
};

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
  // Load game code
  linux_game_code Game;
  {
    Game.Library = dlopen("libloderunner.so", RTLD_NOW);
    if (Game.Library != NULL) {
      dlerror();  // clear error code
      Game.UpdateAndRender =
          (game_update_and_render *)dlsym(Game.Library, "GameUpdateAndRender");
      char *err = dlerror();
      if (err != NULL) {
        Game.UpdateAndRender = GameUpdateAndRenderStub;
        printf("Could not find GameUpdateAndRender: %s\n", err);
      }
    }
  }

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

  u32 border_color = WhitePixel(display, screen);
  u32 bg_color = BlackPixel(display, screen);

  printf("white: %u\n", border_color);
  printf("black: %u\n", bg_color);

  window = XCreateSimpleWindow(display, RootWindow(display, screen), 300, 300,
                               500, 500, 1, border_color, bg_color);

  XSetStandardProperties(display, window, "My Window", "Hi!", None, NULL, 0,
                         NULL);

  XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask);
  XMapWindow(display, window);

  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);




  // TODO:
  // - init memory and backbuffer
  // - init input
  // - call update and render
  // - get something to display




  // Init game memory
  {
    GameMemory.MemorySize = 1024 * 1024 * 1024;  // 1 Gigabyte
    GameMemory.Start =
        VirtualAlloc(0, GameMemory.MemorySize, MEM_COMMIT, PAGE_READWRITE);
    // SecureZeroMemory(GameMemory.Start, GameMemory.MemorySize);
    GameMemory.Free = GameMemory.Start;
    GameMemory.IsInitialized = true;

    GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
    // GameMemory.DEBUGPlatformWriteEntireFile =
    // DEBUGPlatformWriteEntireFile;
  }

  // Init backbuffer
  {
    GameBackBuffer.MaxWidth = 2000;
    GameBackBuffer.MaxHeight = 1500;
    GameBackBuffer.BytesPerPixel = 4;

    int BufferSize = GameBackBuffer.MaxWidth * GameBackBuffer.MaxHeight *
                     GameBackBuffer.BytesPerPixel;
    // TODO: put it into the game memory?
    GameBackBuffer.Memory =
        VirtualAlloc(0, BufferSize, MEM_COMMIT, PAGE_READWRITE);

    GlobalBitmapInfo.bmiHeader.biSize = sizeof(GlobalBitmapInfo.bmiHeader);
    GlobalBitmapInfo.bmiHeader.biPlanes = 1;
    GlobalBitmapInfo.bmiHeader.biBitCount = 32;
    GlobalBitmapInfo.bmiHeader.biCompression = BI_RGB;

    // Set up proper values of buffers based on actual client size
    Win32ResizeClientWindow(Window);
  }

  GlobalRunning = true;

  while (GlobalRunning) {

    // Process events
    {
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
        XMoveWindow(display, window, event.xbutton.x, event.xbutton.y);
      }
      if (event.type == ClientMessage) {
        if (event.xclient.data.l[0] == wmDeleteMessage) {
          GlobalRunning = false;
        }
      }
      // TODO: collect input
    }



  }

  XCloseDisplay(display);

  return 0;
}

