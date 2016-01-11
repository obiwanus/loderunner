#include "loderunner_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // usleep
#include <time.h>
#include <limits.h>

#include "loderunner.h"

struct linux_game_code {
  void *Library;
  game_update_and_render *UpdateAndRender;
  bool32 IsValid;
};

global bool GlobalRunning;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;
global platform_sound_output gSoundOutput;
global XImage *gXImage;

internal void LinuxGetExeDir(char *PathToExe) {
  readlink("/proc/self/exe", PathToExe, PATH_MAX);

  // Cut the file name
  char *OnePastLastSlash = PathToExe;
  for (char *Scan = PathToExe; *Scan; Scan++) {
    if (*Scan == '/') {
      OnePastLastSlash = Scan + 1;
    }
  }
  *OnePastLastSlash = 0;
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  file_read_result Result = {};

  char PathToExe[PATH_MAX];
  LinuxGetExeDir(PathToExe);

  char FilePath[PATH_MAX];
  sprintf(FilePath, "%sdata/%s", PathToExe, Filename)

  FILE *f = fopen(FilePath, "rb");
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

inline u64 LinuxGetWallClock() {
  u64 result = 0;
  struct timespec spec;

  clock_gettime(CLOCK_MONOTONIC, &spec);
  result = spec.tv_nsec;  // ns

  return result;
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
  if (!Game.IsValid) {
    printf("Could not load game code\n");
    exit(1);
  }

  Display *display;
  Window window;
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

  XSelectInput(display, window,
               ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask);
  XMapRaised(display, window);

  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

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

  // TODO: query monitor refresh rate
  int target_fps = 60;
  r32 target_nspf = 1.0e9f / (r32)target_fps;  // Target ms per frame

  GlobalRunning = true;

  u64 last_timestamp = LinuxGetWallClock();

  while (GlobalRunning) {
    // Process events
    while (XPending(display)) {
      XEvent event;
      KeySym key;
      char buf[256];
      player_input *Player1 = &NewInput->Player1;
      player_input *Player2 = &NewInput->Player2;
      char symbol = 0;
      bool32 pressed = false;
      bool32 released = false;
      bool32 retriggered = false;

      XNextEvent(display, &event);

      if (XLookupString(&event.xkey, buf, 255, &key, 0) == 1) {
        symbol = buf[0];
      }

      // Process user input
      if (event.type == KeyPress) {
        printf("Key pressed\n");
        pressed = true;
      }

      if (event.type == KeyRelease) {
        if (XEventsQueued(display, QueuedAfterReading)) {
          XEvent nev;
          XPeekEvent(display, &nev);

          if (nev.type == KeyPress && nev.xkey.time == event.xkey.time &&
              nev.xkey.keycode == event.xkey.keycode) {
            // Ignore. Key wasn't actually released
            printf("Key release ignored\n");
            XNextEvent(display, &event);
            retriggered = true;
          }
        }

        if (!retriggered) {
          printf("Key released\n");
          released = true;
        }
      }

      if (pressed || released) {
        if (key == XK_Escape) {
          GlobalRunning = false;
        } else if (key == XK_Left) {
          Player1->Left.EndedDown = pressed;
        } else if (key == XK_Right) {
          Player1->Right.EndedDown = pressed;
        } else if (key == XK_Up) {
          Player1->Up.EndedDown = pressed;
        } else if (key == XK_Down) {
          Player1->Down.EndedDown = pressed;
        } else if (key == XK_space) {
          Player1->Fire.EndedDown = pressed;
        } else if (symbol == 'x') {
          Player1->Turbo.EndedDown = pressed;
        }
      }

      // Close window message
      if (event.type == ClientMessage) {
        if (event.xclient.data.l[0] == wmDeleteMessage) {
          GlobalRunning = false;
        }
      }
    }

    bool32 RedrawLevel = false;

    Game.UpdateAndRender(NewInput, &GameBackBuffer, &GameMemory, &gSoundOutput,
                         RedrawLevel);

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

    XPutImage(display, window, gc, gXImage, 0, 0, 0, 0, kWindowWidth,
              kWindowHeight);

    // Limit FPS
    {
      u64 current_timestamp = LinuxGetWallClock();
      u64 ns_elapsed = LinuxGetWallClock() - last_timestamp;

      if (ns_elapsed < target_nspf) {
        timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = target_nspf - ns_elapsed;  // time to sleep
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);

        while (ns_elapsed < target_nspf) {
          ns_elapsed = LinuxGetWallClock() - last_timestamp;
        }
      } else {
        printf("Frame missed\n");
      }

      last_timestamp = LinuxGetWallClock();
    }
  }

  XCloseDisplay(display);

  return 0;
}
