#include "loderunner_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // usleep
#include <limits.h>

#if BUILD_SLOW
#include <signal.h>
#endif

#include "loderunner.h"

struct linux_game_code {
  void *Library;
  game_update_and_render *UpdateAndRender;
  bool32 IsValid;
};

global bool GlobalRunning;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;
global XImage *gXImage;

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  file_read_result Result = {};

  FILE *f = fopen(Filename, "rb");
  if (f == NULL) {
    printf("Cannot open file: %s\n", Filename);
    exit(1);
  }

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  Result.MemorySize = fsize;
  Result.Memory = malloc(fsize);
  fread(Result.Memory, fsize, 1, f);
  fclose(f);

  return Result;
}

int main(int argc, char const *argv[]) {
  // Load game code
  linux_game_code Game = {};
  {
    char path_to_exec[PATH_MAX];
    char path_to_so[PATH_MAX];
    readlink("/proc/self/exe", path_to_exec, PATH_MAX);
    sprintf(path_to_so, "%s.so", path_to_exec);
    Game.Library = dlopen(path_to_so, RTLD_NOW);
    if (Game.Library != NULL) {
      dlerror();  // clear error code
      Game.UpdateAndRender =
          (game_update_and_render *)dlsym(Game.Library, "GameUpdateAndRender");
      char *err = dlerror();
      if (err != NULL) {
        Game.UpdateAndRender = GameUpdateAndRenderStub;
        printf("Could not find GameUpdateAndRender: %s\n", err);
      } else {
        Game.IsValid = true;
      }
    }
  }
  Assert(Game.IsValid);

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

  const int kWindowWidth = 1500;
  const int kWindowHeight = 1000;

  window = XCreateSimpleWindow(display, RootWindow(display, screen), 300, 300,
                               kWindowWidth, kWindowHeight, 0, border_color,
                               bg_color);

  XSetStandardProperties(display, window, "My Window", "Hi!", None, NULL, 0,
                         NULL);

  XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask);
  XMapRaised(display, window);

  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

  // XSync(display, True);

  usleep(5000);  // 50 ms

  // Init game memory
  {
    GameMemory.MemorySize = 1024 * 1024 * 1024;  // 1 Gigabyte
    GameMemory.Start = malloc(GameMemory.MemorySize);
    GameMemory.Free = GameMemory.Start;
    GameMemory.IsInitialized = true;

    GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
  }

  // Init backbuffer
  GameBackBuffer.MaxWidth = 2000;
  GameBackBuffer.MaxHeight = 1500;
  GameBackBuffer.BytesPerPixel = 4;

  // @tmp
  GameBackBuffer.Width = kWindowWidth;
  GameBackBuffer.Height = kWindowHeight;

  int BufferSize = GameBackBuffer.MaxWidth * GameBackBuffer.MaxHeight *
                   GameBackBuffer.BytesPerPixel;

  GameBackBuffer.Memory = malloc(BufferSize);

  Pixmap pixmap;
  GC gc;
  XGCValues gcvalues;

  // Create x image
  {
    // int depth = 32;
    // int bitmap_pad = 32;
    // int bytes_per_line = 0;
    // int offset = 0;

    // gXImage = XCreateImage(display, CopyFromParent, depth, ZPixmap, offset,
    //                        (char *)GameBackBuffer.Memory, kWindowWidth,
    //                        kWindowHeight, bitmap_pad, bytes_per_line);

    // TODO: find a way to do it with a newly created image
    usleep(500000);
    gXImage = XGetImage(display, window, 0, 0, kWindowWidth, kWindowHeight,
                        AllPlanes, ZPixmap);

    GameBackBuffer.Memory = (void *)gXImage->data;

    // u32 *Pixel = (u32 *)gXImage->data;
    // for (int i = 0; i < kWindowWidth * 700; i++) {
    //   *Pixel = 0x00FF00FF;
    //   Pixel++;
    // }

    // pixmap = XCreatePixmap(display, window, kWindowWidth,
    //                        kWindowHeight, depth);
    gc = XCreateGC(display, window, 0, &gcvalues);
  }

  // Get space for inputs
  game_input Input[2];
  game_input *OldInput = &Input[0];
  game_input *NewInput = &Input[1];
  *NewInput = {};

  GlobalRunning = true;

  while (GlobalRunning) {
    // Process events
    while (XPending(display)) {
      XNextEvent(display, &event);

      // Process user input
      if (event.type == KeyPress) {
        if (XLookupString(&event.xkey, buf, 255, &key, 0) == 1) {
          char symbol = buf[0];
        }
        if (key == XK_Escape) {
          GlobalRunning = false;
        } else if (key == XK_Left) {
          printf("left!\n");
        }
      }

      // Close window message
      if (event.type == ClientMessage) {
        if (event.xclient.data.l[0] == wmDeleteMessage) {
          GlobalRunning = false;
        }
      }
    }

    Game.UpdateAndRender(NewInput, &GameBackBuffer, &GameMemory);

    // Swap inputs
    game_input *TmpInput = OldInput;
    OldInput = NewInput;
    NewInput = TmpInput;
    *NewInput = {};  // zero everything

    // Retain the EndedDown state
    for (int p = 0; p < COUNT_OF(NewInput->Players); p++) {
      player_input *OldPlayerInput = &OldInput->Players[p];
      player_input *NewPlayerInput = &NewInput->Players[p];
      for (int b = 0; b < COUNT_OF(OldPlayerInput->Buttons); b++) {
        NewPlayerInput->Buttons[b].EndedDown =
            OldPlayerInput->Buttons[b].EndedDown;
      }
    }

    // XPutImage(display, window, gc, gXImage, 0, 0, 0, 0, kWindowWidth,
    //           kWindowHeight);

    // TODO: limit FPS
  }

  XCloseDisplay(display);

  return 0;
}
