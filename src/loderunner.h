#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "loderunner_platform.h"
#include "loderunner_math.h"

void *GameMemoryAlloc(int SizeInBytes);

struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
  int MaxWidth;  // We'll only allocate this much
  int MaxHeight;
};

struct bmp_file {
  int Width;
  int Height;
  void *Bitmap;

  // Zero if compression is not 3
  u32 RedMask;
  u32 GreenMask;
  u32 BlueMask;
  u32 AlphaMask;
};

#pragma pack(push, 1)

struct bmp_file_header {
  u16 bfType;
  u32 bfSize;
  u16 bfReserved1;
  u16 bfReserved2;
  u32 bfOffBits;
};

struct bmp_info_header {
  u32 biSize;
  i32 biWidth;
  i32 biHeight;
  u16 biPlanes;
  u16 biBitCount;
  u32 biCompression;
  u32 biSizeImage;
  i32 biXPelsPerMeter;
  i32 biYPelsPerMeter;
  u32 biClrUsed;
  u32 biClrImportant;
  u32 biRedMask;
  u32 biGreenMask;
  u32 biBlueMask;
  u32 biAlphaMask;
};

#pragma pack(pop)

struct sprite {
  bmp_file *Image;
  int XOffset;
  int YOffset;
  int Width;
  int Height;
};

struct sprites {
  bmp_file *Brick;
  bmp_file *BrickHard;
  bmp_file *Ladder;
  bmp_file *Rope;
  bmp_file *Treasure;
  bmp_file *Sprites;  // crazy
  sprite Falling;
  sprite Breaking;
};

struct file_read_result {
  void *Memory;
  u64 MemorySize;
};

struct game_button_state {
  int HalfTransitionCount;
  bool32 EndedDown;
};

struct player_input {
  union {
    game_button_state Buttons[7];

    struct {
      game_button_state Up;
      game_button_state Down;
      game_button_state Left;
      game_button_state Right;
      game_button_state Fire;
      game_button_state Turbo;
      game_button_state Debug;
      // NOTE: don't forget to update the number above
      // if you add a new state
    };
  };
};

struct game_input {
  union {
    player_input Players[2];
    struct {
      player_input Player1;
      player_input Player2;
    };
  };
  r32 dtForFrame;
};

struct frame {
  int XOffset;
  int YOffset;
  int Lasting;
};

struct animation {
  int Counter = 0;
  int FrameCount = 0;
  int Frame = 0;  // Current frame

  // We assume we're not going to need more
  frame Frames[5];
};

struct entity {
  union {
    v2i Position;
    struct {
      int X;
      int Y;
    };
  };
  int TileX;
  int TileY;
  int Width;
  int Height;
};

struct person : entity {
  int Facing;
  int FireCooldown;
  int IsFalling;

  // Animation
  animation *Animation;
  animation GoingLeft;
  animation GoingRight;
  animation Climbing;
  animation RopeLeft;
  animation RopeRight;
  animation Falling;

  bool32 Animate;
  bool32 IsInitialized;
  bool32 IsStuck;
  bool32 WasStuck;
  bool32 CanClimb;
  bool32 CanDescend;
};

struct player : person {
  bool32 IsActive = false;
};

typedef enum {
  NOWHERE = 0,
  LEFT,
  RIGHT,
  UP,
  DOWN,
} direction;

#define MAX_PATH_LENGTH 100

struct enemy : person {
  direction Direction;
  player *Pursuing;
  int PathCooldown;
  bool32 PathFound;
  v2i Path[MAX_PATH_LENGTH];
  int PathLength;
};

struct treasure : entity {
  int TileX;
  int TileY;
};

struct crushed_brick : entity {

  bool32 IsUsed = false;
  int Countdown;

  enum {
    CRUSHING = 0,
    WAITING,
    RESTORING,
  } State;

  animation Breaking;
  animation Restoring;
};

#define MAX_LEVEL_HEIGHT 100
#define MAX_LEVEL_WIDTH 100

typedef enum {
  LVL_BLANK,
  LVL_BLANK_TMP,
  LVL_BRICK,
  LVL_BRICK_HARD,
  LVL_LADDER,
  LVL_WIN_LADDER,
  LVL_ROPE,
  LVL_TREASURE,
  LVL_ENEMY,
  LVL_PLAYER,
  LVL_INVALID,
} tile_type;

typedef enum {
  WATERMAP_NOT_VISITED = 0,
  WATERMAP_OBSTACLE,
  WATERMAP_WATER,
} water_point;

struct level {
  int Width;
  int Height;
  int PlayerCount;
  int EnemyCount;
  int TreasureCount;
  tile_type Contents[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
  water_point WaterMap[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
  int DirectionMap[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
};

// -----------------------------------------------------------
// Platform functions

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) \
  file_read_result name(char const *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) \
  void name(char *Filename, int FileSize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

struct game_memory {
  int MemorySize;
  bool32 IsInitialized;
  void *Start;
  void *Free;

  // Debug functions
  debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

// Game functions

#define GAME_UPDATE_AND_RENDER(name)                             \
  void name(game_input *NewInput, game_offscreen_buffer *Buffer, \
            game_memory *Memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {
  // nothing
}
