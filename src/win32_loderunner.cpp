#include "loderunner_platform.h"

#include "loderunner.h"
#include <windows.h>
#include <intrin.h>

struct win32_game_code {
  HMODULE GameCodeDLL;
  game_update_and_render *UpdateAndRender;
  bool32 IsValid;
};

global bool GlobalRunning;

global BITMAPINFO GlobalBitmapInfo;
global LARGE_INTEGER GlobalPerformanceFrequency;

global game_memory GameMemory;
global game_offscreen_buffer GameBackBuffer;

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  file_read_result Result = {};

  HANDLE FileHandle = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, 0,
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
  StretchDIBits(hdc, 0, 0, GameBackBuffer.Width, GameBackBuffer.Height,  // dest
                0, 0, GameBackBuffer.Width, GameBackBuffer.Height,       // src
                GameBackBuffer.Memory, &GlobalBitmapInfo, DIB_RGB_COLORS,
                SRCCOPY);
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

  GlobalBitmapInfo.bmiHeader.biWidth = Width;
  GlobalBitmapInfo.bmiHeader.biHeight = -Height;
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

#define GAME_CODE_DLL_FILENAME "loderunner.dll"

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
        if (VKCode == VK_ESCAPE) {
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
          } else if (VKCode == 'X') {
            Win32ProcessKeyboardMessage(&Player1->Turbo, IsDown);
          } else if (VKCode == VK_SPACE) {
            Win32ProcessKeyboardMessage(&Player1->Fire, IsDown);
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
          } else if (VKCode == 'I') {
#if BUILD_INTERNAL
            Win32ProcessKeyboardMessage(&Player1->Debug, IsDown);
            Win32ProcessKeyboardMessage(&Player2->Debug, IsDown);
#endif
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

  WNDCLASS WindowClass = {};
  WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
  WindowClass.lpfnWndProc = Win32WindowProc;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "loderunnerWindowClass";

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
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                               CW_USEDEFAULT, kWindowWidth, kWindowHeight, 0, 0,
                               hInstance, 0);

    // We're not going to release it as we use CS_OWNDC
    HDC hdc = GetDC(Window);

    HBRUSH BgBrush = CreateSolidBrush(RGB(0x00, 0x22, 0x22));
    SelectObject(hdc, BgBrush);
    PatBlt(hdc, 0, 0, kWindowWidth, kWindowHeight, PATCOPY);
    DeleteObject(BgBrush);

    if (Window) {
      GlobalRunning = true;

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

        GlobalBitmapInfo.bmiHeader.biSize = sizeof(GlobalBitmapInfo.bmiHeader);
        GlobalBitmapInfo.bmiHeader.biPlanes = 1;
        GlobalBitmapInfo.bmiHeader.biBitCount = 32;
        GlobalBitmapInfo.bmiHeader.biCompression = BI_RGB;

        // Set up proper values of buffers based on actual client size
        Win32ResizeClientWindow(Window);
      }

      // Get space for inputs
      game_input Input[2];
      game_input *OldInput = &Input[0];
      game_input *NewInput = &Input[1];
      *NewInput = {};

      FILETIME LastDLLWriteTime = Win32GetDLLWriteTime();

      // Main loop
      while (GlobalRunning) {
        FILETIME NewDLLWriteTime = Win32GetDLLWriteTime();
        int CMP = CompareFileTime(&LastDLLWriteTime, &NewDLLWriteTime);
        if (CMP != 0) {
          Win32UnloadGameCode(&Game);
          Game = Win32LoadGameCode();
          LastDLLWriteTime = NewDLLWriteTime;
        }

        // Collect input
        Win32ProcessPendingMessages(NewInput);
        NewInput->dtForFrame = TargetMSPF;

        Game.UpdateAndRender(NewInput, &GameBackBuffer, &GameMemory);

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

        Win32UpdateWindow(hdc);

        // Enforce FPS
        // TODO: for some reason Time to sleep drops every now and again,
        // disabling gradient solves or masks this, though I don't see
        // any reason why this might happen
        {
          r32 MillisecondsElapsed =
              Win32GetMillisecondsElapsed(LastTimestamp, Win32GetWallClock());
          u32 TimeToSleep = 0;

          if (MillisecondsElapsed < TargetMSPF) {
            TimeToSleep = (u32)(TargetMSPF - MillisecondsElapsed);
            Sleep(TimeToSleep);

            while (MillisecondsElapsed < TargetMSPF) {
              MillisecondsElapsed = Win32GetMillisecondsElapsed(
                  LastTimestamp, Win32GetWallClock());
            }
          } else {
            OutputDebugStringA("Frame missed\n");
          }

          LastTimestamp = Win32GetWallClock();

          // if (TimeToSleep)
          // {
          //     char String[300];
          //     sprintf_s(String, "Time to sleep: %d, Ms elapsed: %.2f, < 10 =
          //     %d\n", TimeToSleep, MillisecondsElapsed, TimeToSleep < 10);
          //     OutputDebugStringA(String);
          // }
        }
      }
    }
  } else {
    // TODO: logging
    OutputDebugStringA("Couldn't register window class");
  }

  return 0;
}