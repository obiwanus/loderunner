#include "loderunner_platform.h"

#include "loderunner.h"
#include <time.h>
#include <windows.h>
#include <intrin.h>
#include <dsound.h>
#include <gl/gl.h>

struct win32_game_code {
  HMODULE GameCodeDLL;
  game_update_and_render *UpdateAndRender;
  bool32 IsValid;
};

global bool GlobalRunning;

// global BITMAPINFO GlobalBitmapInfo;
global GLuint gTextureHandle;
global LARGE_INTEGER GlobalPerformanceFrequency;
global WINDOWPLACEMENT gWindowPlacement = {sizeof(gWindowPlacement)};
global bool32 gFullscreen;
global HCURSOR gCursor;
global LPDIRECTSOUNDBUFFER gSoundBuffer;
global platform_sound_output gSoundOutput;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;
global bool32 gRedrawLevel;

typedef HRESULT WINAPI directsound_create(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);

#define GAME_CODE_DLL_FILENAME "gamelib.dll"

internal void Win32GetExeDir(char *PathToExe) {
  GetModuleFileName(0, PathToExe, MAX_PATH);

  // Cut the file name
  char *OnePastLastSlash = PathToExe;
  for (char *Scan = PathToExe; *Scan; Scan++) {
    if (*Scan == '\\') {
      OnePastLastSlash = Scan + 1;
    }
  }
  *OnePastLastSlash = 0;
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  file_read_result Result = {};

  char FilePath[MAX_PATH];

  char ExeDir[MAX_PATH];
  Win32GetExeDir(ExeDir);

#if BUILD_INTERNAL
  sprintf_s(FilePath, "%s", Filename);
#else
  sprintf_s(FilePath, "%sdata/%s", ExeDir, Filename);
#endif

  // for (int i = 0; i < MAX_PATH; i++) {
  //   char c = FilePath[i];
  //   if (c == '\0') break;
  //   if (c == '/') FilePath[i] = '\\';
  // }

  HANDLE FileHandle = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, 0,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  if (FileHandle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize)) {
      Result.MemorySize = FileSize.QuadPart;
      Result.Memory =
          VirtualAlloc(0, Result.MemorySize, MEM_COMMIT, PAGE_READWRITE);
      DWORD BytesRead = 0;

      ReadFile(FileHandle, Result.Memory, (u32)Result.MemorySize, &BytesRead,
               0);

      CloseHandle(FileHandle);

      return Result;
    } else {
      OutputDebugStringA("Cannot get file size\n");
      // GetLastError() should help
    }
  } else {
    OutputDebugStringA("Cannot read from file\n");
    // GetLastError() should help
  }

  return Result;
}

internal void Win32UpdateWindow(HDC hdc) {
  // StretchDIBits(hdc, 0, 0, GameBackBuffer.Width, GameBackBuffer.Height,
  //               0, 0, GameBackBuffer.Width, GameBackBuffer.Height,
  //               GameBackBuffer.Memory, &GlobalBitmapInfo, DIB_RGB_COLORS,
  //               SRCCOPY);


  glViewport(0, 0, GameBackBuffer.Width, GameBackBuffer.Height);

  glEnable(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, gTextureHandle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GameBackBuffer.Width,
               GameBackBuffer.Height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
               GameBackBuffer.Memory);  // GL_RGBA

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glBegin(GL_TRIANGLES);

  glTexCoord2f(0.0f, 1.0f);
  glVertex2i(-1, -1);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2i(1, -1);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2i(1, 1);

  glTexCoord2f(0.0f, 1.0f);
  glVertex2i(-1, -1);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2i(-1, 1);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2i(1, 1);

  glEnd();

  SwapBuffers(hdc);
}

void Win32ToggleFullscreen(HWND hwnd) {
  // Copied from the Raymond Chen's article:
  // https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353/

  DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
  if (dwStyle & WS_OVERLAPPEDWINDOW) {
    MONITORINFO mi = {sizeof(mi)};
    if (GetWindowPlacement(hwnd, &gWindowPlacement) &&
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY),
                       &mi)) {
      SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    gFullscreen = true;
  } else {
    SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(hwnd, &gWindowPlacement);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
                                             SWP_NOZORDER | SWP_NOOWNERZORDER |
                                             SWP_FRAMECHANGED);
    gFullscreen = false;
  }
}

internal void Win32ResizeClientWindow(HWND Window) {
  if (!GameMemory.IsInitialized) return;  // no buffer yet

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  int Width = ClientRect.right - ClientRect.left;
  int Height = ClientRect.bottom - ClientRect.top;

  if (Width > GameBackBuffer.MaxWidth) {
    Width = GameBackBuffer.MaxWidth;
  }
  if (Height > GameBackBuffer.MaxHeight) {
    Height = GameBackBuffer.MaxHeight;
  }

  GameBackBuffer.Width = Width;
  GameBackBuffer.Height = Height;

  gRedrawLevel = true;

  // GlobalBitmapInfo.bmiHeader.biWidth = Width;
  // GlobalBitmapInfo.bmiHeader.biHeight = -Height;
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);

  return Result;
}

inline r32 Win32GetMillisecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  r32 Result = 1000.0f * (r32)(End.QuadPart - Start.QuadPart) /
               (r32)GlobalPerformanceFrequency.QuadPart;

  return Result;
}

LRESULT CALLBACK
Win32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LRESULT Result = 0;

  switch (uMsg) {
    case WM_SIZE: {
      Win32ResizeClientWindow(hwnd);
    } break;

    case WM_CLOSE: {
      GlobalRunning = false;
    } break;

    case WM_PAINT: {
      PAINTSTRUCT Paint = {};
      HDC hdc = BeginPaint(hwnd, &Paint);
      Win32UpdateWindow(hdc  // we update the whole window, always
                             // Paint.rcPaint.left,
                             // Paint.rcPaint.top,
                             // Paint.rcPaint.right - Paint.rcPaint.left,
                             // Paint.rcPaint.bottom - Paint.rcPaint.top
                        );

      OutputDebugStringA("WM_PAINT\n");

      EndPaint(hwnd, &Paint);
    } break;

    case WM_SETCURSOR: {
      if (gFullscreen) {
        SetCursor(0);
      } else {
        SetCursor(gCursor);
      }
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      Assert(!"Keyboard input came in through a non-dispatch message!");
    } break;

    default: { Result = DefWindowProc(hwnd, uMsg, wParam, lParam); } break;
  }

  return Result;
}

internal FILETIME Win32GetDLLWriteTime() {
  FILETIME Result = {};
  WIN32_FIND_DATA FileData = {};

  char ExeDir[MAX_PATH];
  Win32GetExeDir(ExeDir);
  char PathToDLL[MAX_PATH];
  sprintf_s(PathToDLL, "%s%s", ExeDir, GAME_CODE_DLL_FILENAME);

  HANDLE FileFindHandle = FindFirstFile(PathToDLL, &FileData);
  if (FileFindHandle != INVALID_HANDLE_VALUE) {
    Result = FileData.ftLastWriteTime;
  }
  return Result;
}

internal void Win32UnloadGameCode(win32_game_code *GameCode) {
  GameCode->UpdateAndRender = GameUpdateAndRenderStub;
  FreeLibrary(GameCode->GameCodeDLL);
  GameCode->IsValid = false;
}

internal win32_game_code Win32LoadGameCode() {
  win32_game_code Result = {};
  Result.UpdateAndRender = GameUpdateAndRenderStub;

  char ExeDir[MAX_PATH];
  char PathToDLL[MAX_PATH];
  char TmpDLLFile[MAX_PATH];
  Win32GetExeDir(ExeDir);
  sprintf_s(PathToDLL, "%s%s", ExeDir, GAME_CODE_DLL_FILENAME);
  sprintf_s(TmpDLLFile, "%s%s", ExeDir, "loderunner_tmp.dll");
  CopyFile(PathToDLL, TmpDLLFile, FALSE);

  Result.GameCodeDLL = LoadLibraryA(TmpDLLFile);
  if (Result.GameCodeDLL) {
    Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(
        Result.GameCodeDLL, "GameUpdateAndRender");
    if (Result.UpdateAndRender) {
      Result.IsValid = true;
    }
  }

  return Result;
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState,
                                          bool32 IsDown) {
  if (NewState->EndedDown != IsDown) {
    NewState->HalfTransitionCount++;
    NewState->EndedDown = IsDown;
  } else {
    // This may happen if user pressed two buttons
    // that trigger the same action simultaneously
  }
}

internal void Win32ProcessPendingMessages(game_input *NewInput) {
  MSG Message;
  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
    // Get keyboard messages
    switch (Message.message) {
      case WM_QUIT: {
        GlobalRunning = false;
      } break;

      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP: {
        u32 VKCode = (u32)Message.wParam;
        bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
        bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);

        player_input *Player1 = &NewInput->Player1;
        player_input *Player2 = &NewInput->Player2;

        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
        if ((VKCode == VK_F4) && AltKeyWasDown) {
          GlobalRunning = false;
        }

        // Get input
        if (IsDown != WasDown) {
          if (VKCode == VK_UP) {
            Win32ProcessKeyboardMessage(&Player1->Up, IsDown);
          } else if (VKCode == VK_DOWN) {
            Win32ProcessKeyboardMessage(&Player1->Down, IsDown);
          } else if (VKCode == VK_LEFT) {
            Win32ProcessKeyboardMessage(&Player1->Left, IsDown);
          } else if (VKCode == VK_RIGHT) {
            Win32ProcessKeyboardMessage(&Player1->Right, IsDown);
          } else if (VKCode == VK_SPACE) {
            Win32ProcessKeyboardMessage(&Player1->Fire, IsDown);
          } else if (VKCode == VK_RETURN) {
            Win32ProcessKeyboardMessage(&Player1->Fire, IsDown);
          } else if (VKCode == VK_ESCAPE) {
            Win32ProcessKeyboardMessage(&Player1->Menu, IsDown);
          } else if (VKCode == 'W') {
            Win32ProcessKeyboardMessage(&Player2->Up, IsDown);
          } else if (VKCode == 'S') {
            Win32ProcessKeyboardMessage(&Player2->Down, IsDown);
          } else if (VKCode == 'A') {
            Win32ProcessKeyboardMessage(&Player2->Left, IsDown);
          } else if (VKCode == 'D') {
            Win32ProcessKeyboardMessage(&Player2->Right, IsDown);
          } else if (VKCode == 'E') {
            Win32ProcessKeyboardMessage(&Player2->Fire, IsDown);
          } else if (VKCode == 'X') {
#if BUILD_INTERNAL
            Win32ProcessKeyboardMessage(&Player1->Turbo, IsDown);
            Win32ProcessKeyboardMessage(&Player2->Turbo, IsDown);
#endif
          } else if (VKCode == 'I') {
#if BUILD_INTERNAL
            Win32ProcessKeyboardMessage(&Player1->Debug, IsDown);
#endif
          } else if (VKCode == VK_RETURN && AltKeyWasDown && WasDown) {
            Win32ToggleFullscreen(Message.hwnd);
          }
        }
      } break;

      default: {
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
      } break;
    }
  }
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
  win32_game_code Game = Win32LoadGameCode();
  Assert(Game.IsValid);

  // Random seed
  srand((u32)time(NULL));

  WNDCLASS WindowClass = {};
  WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
  WindowClass.lpfnWndProc = Win32WindowProc;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "loderunnerWindowClass";
  gCursor = LoadCursor(0, IDC_ARROW);
  WindowClass.hCursor = gCursor;

  // TODO: query monitor refresh rate
  int TargetFPS = 60;
  r32 TargetMSPF = 1000.0f / (r32)TargetFPS;  // Target ms per frame

  // Set target sleep resolution
  {
#define TARGET_SLEEP_RESOLUTION 1  // 1-millisecond target resolution

    TIMECAPS tc;
    UINT wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
      OutputDebugStringA("Cannot set the sleep resolution\n");
      exit(1);
    }

    wTimerRes = min(max(tc.wPeriodMin, TARGET_SLEEP_RESOLUTION), tc.wPeriodMax);
    timeBeginPeriod(wTimerRes);
  }

  QueryPerformanceFrequency(&GlobalPerformanceFrequency);

  if (RegisterClass(&WindowClass)) {
    const int kWindowWidth = 1500;
    const int kWindowHeight = 1000;

    HWND Window = CreateWindow(WindowClass.lpszClassName, 0,
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 50,
                               kWindowWidth, kWindowHeight, 0, 0, hInstance, 0);

    // We're not going to release it as we use CS_OWNDC
    HDC hdc = GetDC(Window);

    // Fullscreen by default
    Win32ToggleFullscreen(Window);

    if (Window) {
      GlobalRunning = true;

      // Init sound
      {
        HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
        if (DSoundLibrary) {
          directsound_create *DirectSoundCreate =
              (directsound_create *)GetProcAddress(DSoundLibrary,
                                                   "DirectSoundCreate");
          LPDIRECTSOUND DirectSound;
          if (DirectSoundCreate &&
              SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = 44100;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign =
                (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec =
                WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            // Create primary buffer
            if (SUCCEEDED(
                    DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
              LPDIRECTSOUNDBUFFER PrimaryBuffer;
              DSBUFFERDESC BufferDescription = {};
              BufferDescription.dwSize = sizeof(BufferDescription);
              BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

              if (SUCCEEDED(DirectSound->CreateSoundBuffer(
                      &BufferDescription, &PrimaryBuffer, 0))) {
                if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                  OutputDebugStringA("Primary buffer format was set\n");
                }
              }
            }

            // Create secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes =
                44100 * sizeof(i16) * 2;  // 1 second
            BufferDescription.lpwfxFormat = &WaveFormat;

            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                         &gSoundBuffer, 0))) {
              OutputDebugStringA("Secondary buffer created\n");
            }
          }
        }
        gSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
      }

      // Init OpenGL
      {
        PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
        DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
        DesiredPixelFormat.nVersion = 1;
        DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
        DesiredPixelFormat.dwFlags =
            PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
        DesiredPixelFormat.cColorBits = 32;
        DesiredPixelFormat.cAlphaBits = 8;
        DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

        int SuggestedPixelFormatIndex =
            ChoosePixelFormat(hdc, &DesiredPixelFormat);
        PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
        DescribePixelFormat(hdc, SuggestedPixelFormatIndex,
                            sizeof(SuggestedPixelFormat),
                            &SuggestedPixelFormat);

        SetPixelFormat(hdc, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

        HGLRC OpenGLRC = wglCreateContext(hdc);
        if (wglMakeCurrent(hdc, OpenGLRC)) {
          // Success
          glGenTextures(1, &gTextureHandle);

          typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
          wgl_swap_interval_ext *wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
          if (wglSwapInterval) {
            wglSwapInterval(1);
          }
        } else {
          // Something's wrong
          Assert(false);
        }
      }

      LARGE_INTEGER LastTimestamp = Win32GetWallClock();

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

        // GlobalBitmapInfo.bmiHeader.biSize = sizeof(GlobalBitmapInfo.bmiHeader);
        // GlobalBitmapInfo.bmiHeader.biPlanes = 1;
        // GlobalBitmapInfo.bmiHeader.biBitCount = 32;
        // GlobalBitmapInfo.bmiHeader.biCompression = BI_RGB;

        // Set up proper values of buffers based on actual client size
        Win32ResizeClientWindow(Window);
        gRedrawLevel = false;
      }

      // Get space for inputs
      game_input Input[2];
      game_input *OldInput = &Input[0];
      game_input *NewInput = &Input[1];
      *NewInput = {};

      FILETIME LastDLLWriteTime = Win32GetDLLWriteTime();

      // Main loop
      while (GlobalRunning) {
#if BUILD_INTERNAL
        FILETIME NewDLLWriteTime = Win32GetDLLWriteTime();
        int CMP = CompareFileTime(&LastDLLWriteTime, &NewDLLWriteTime);
        if (CMP != 0) {
          Win32UnloadGameCode(&Game);
          Game = Win32LoadGameCode();
          LastDLLWriteTime = NewDLLWriteTime;
        }
#endif

        // Collect input
        Win32ProcessPendingMessages(NewInput);
        NewInput->dtForFrame = TargetMSPF;

        int Result =
            Game.UpdateAndRender(NewInput, &GameBackBuffer, &GameMemory,
                                 &gSoundOutput, gRedrawLevel);
        if (Result > 0) {
          GlobalRunning = false;
        }

        if (gRedrawLevel) {
          gRedrawLevel = false;
        }

        // Swap inputs
        game_input *TmpInput = OldInput;
        OldInput = NewInput;
        NewInput = TmpInput;
        *NewInput = {};  // zero everything

        // Retain the EndedDown state
        for (int PlayerNum = 0; PlayerNum < COUNT_OF(NewInput->Players);
             PlayerNum++) {
          player_input *OldPlayerInput = &OldInput->Players[PlayerNum];
          player_input *NewPlayerInput = &NewInput->Players[PlayerNum];
          for (int ButtonNum = 0; ButtonNum < COUNT_OF(OldPlayerInput->Buttons);
               ButtonNum++) {
            NewPlayerInput->Buttons[ButtonNum].EndedDown =
                OldPlayerInput->Buttons[ButtonNum].EndedDown;
          }
        }

        // Output sound
        DWORD PlayCursor;
        DWORD WriteCursor;
        if (SUCCEEDED(
                gSoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
          int BytesPerSample = sizeof(i16) * 2;
          int SamplesPerSecond = 44100;
          int BufferSize = SamplesPerSecond * BytesPerSample;
          int SamplesToWrite =
              (SamplesPerSecond / 60) * 10;  // 10 frames' worth of sound
          DWORD BytesToWrite = SamplesToWrite * BytesPerSample;

          if (!gSoundOutput.IsInitialized) {
            gSoundOutput.IsInitialized = true;
            gSoundOutput.LastWritePosition = WriteCursor;
          }

          int BytesPassed = 0;
          DWORD LastCursor = gSoundOutput.LastWritePosition;
          if (WriteCursor < LastCursor) {
            BytesPassed = BufferSize - LastCursor + WriteCursor;
            Assert(BytesPassed > 0);
          } else {
            BytesPassed = WriteCursor - LastCursor;
          }
          int SamplesPassed = BytesPassed / BytesPerSample;
          gSoundOutput.LastWritePosition = WriteCursor;

          if (gSoundOutput.Playing != NULL) {
            if (gSoundOutput.SamplesWritten != -1) {
              gSoundOutput.SamplesWritten += SamplesPassed;
            }
            if (gSoundOutput.SamplesWritten >=
                gSoundOutput.Playing->SampleCount) {
              // Done playing a sound
              gSoundOutput.Playing = NULL;
              gSoundOutput.SamplesWritten = 0;
            }
          }

          VOID *Region1;
          DWORD Region1Size;
          VOID *Region2;
          DWORD Region2Size;

          if (SUCCEEDED(gSoundBuffer->Lock(WriteCursor, BytesToWrite, &Region1,
                                           &Region1Size, &Region2, &Region2Size,
                                           DSBLOCK_FROMWRITECURSOR))) {
            int Region1SampleCount = Region1Size / BytesPerSample;
            int Region2SampleCount = Region2Size / BytesPerSample;

            u32 *CopyFrom = NULL;
            int SamplesToCopy = 0;
            if (gSoundOutput.Playing) {
              int Offset = gSoundOutput.SamplesWritten >= 0
                               ? gSoundOutput.SamplesWritten
                               : 0;
              CopyFrom = (u32 *)gSoundOutput.Playing->Samples + Offset;
              SamplesToCopy = gSoundOutput.Playing->SampleCount - Offset;
            }

            u32 *SampleOut = (u32 *)Region1;
            for (int SampleIndex = 0; SampleIndex < Region1SampleCount;
                 SampleIndex++) {
              if (SamplesToCopy > 0) {
                *SampleOut = *CopyFrom;
                SamplesToCopy--;
                CopyFrom++;
              } else {
                *SampleOut = 0;  // Fill with zeroes
              }

              SampleOut++;
            }

            // copypaste
            SampleOut = (u32 *)Region2;
            for (int SampleIndex = 0; SampleIndex < Region2SampleCount;
                 SampleIndex++) {
              if (SamplesToCopy > 0) {
                *SampleOut = *CopyFrom;
                SamplesToCopy--;
                CopyFrom++;
              } else {
                *SampleOut = 0;  // Fill with zeroes
              }

              SampleOut++;
            }

            gSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);

            if (gSoundOutput.Playing && gSoundOutput.SamplesWritten == -1) {
              gSoundOutput.SamplesWritten = 0;
            }
          }
        }

        Win32UpdateWindow(hdc);

        // Enforce FPS
        // TODO: for some reason Time to sleep drops every now and again,
        // disabling gradient solves or masks this, though I don't see
        // any reason why this might happen
        // {
        //   r32 MillisecondsElapsed =
        //       Win32GetMillisecondsElapsed(LastTimestamp, Win32GetWallClock());
        //   u32 TimeToSleep = 0;

        //   if (MillisecondsElapsed < TargetMSPF) {
        //     TimeToSleep = (u32)(TargetMSPF - MillisecondsElapsed);
        //     Sleep(TimeToSleep);

        //     while (MillisecondsElapsed < TargetMSPF) {
        //       MillisecondsElapsed = Win32GetMillisecondsElapsed(
        //           LastTimestamp, Win32GetWallClock());
        //     }
        //   } else {
        //     OutputDebugStringA("Frame missed\n");
        //   }

        //   LastTimestamp = Win32GetWallClock();

        //   // if (TimeToSleep)
        //   // {
        //   //     char String[300];
        //   //     sprintf_s(String, "Time to sleep: %d, Ms elapsed: %.2f, < 10 =
        //   //     %d\n", TimeToSleep, MillisecondsElapsed, TimeToSleep < 10);
        //   //     OutputDebugStringA(String);
        //   // }
        // }
      }
    }
  } else {
    // TODO: logging
    OutputDebugStringA("Couldn't register window class");
  }

  return 0;
}