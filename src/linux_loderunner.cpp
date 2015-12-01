#include "loderunner_platform.h"

#include <X11/Xlib.h>

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
  char const *msg = "Hello, World!";
  int screen;

  return 0;
}
