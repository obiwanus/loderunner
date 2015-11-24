#include "loderunner.h"

// TODO: delete
#include <windows.h>

// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global entity *Player;

inline int TruncateReal32(r32 Value) {
  int Result = (int)Value;
  return Result;
}

inline int RoundReal32(r32 Value) {
  // TODO: think about overflow
  return TruncateReal32(Value + 0.5f);
}

void *GameMemoryAlloc(int SizeInBytes) {
  void *Result = GameMemory->Free;

  GameMemory->Free = (void *)((u8 *)GameMemory->Free + SizeInBytes);
  i64 CurrentSize = ((u8 *)GameMemory->Free - (u8 *)GameMemory->Start);
  Assert(CurrentSize < GameMemory->MemorySize);

  return Result;
}

inline u8 UnmaskColor(u32 Pixel, u32 ColorMask) {
  int BitOffset = 0;
  switch (ColorMask) {
    case 0x000000FF:
      BitOffset = 0;
      break;
    case 0x0000FF00:
      BitOffset = 8;
      break;
    case 0x00FF0000:
      BitOffset = 16;
      break;
    case 0xFF000000:
      BitOffset = 24;
      break;
  }

  return (u8)((Pixel & ColorMask) >> BitOffset);
}

internal void DEBUGDrawRectangle(v2 Position, int Width, int Height,
                                 u32 Color) {
  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;

  for (int pY = Y; pY < Y + Height; pY++) {
    u32 *Pixel = (u32 *)Row;
    for (int pX = X; pX < X + Width; pX++) {
      *Pixel++ = Color;
    }
    Row += Pitch;
  }
}

inline void SetPixel(int X, int Y, u32 Color) {
  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;
  u32 *Pixel = (u32 *)Row;
  *Pixel = Color;
}

internal void DrawLine(r32 StartX, r32 StartY, r32 EndX, r32 EndY, u32 Color) {
  // Bresenham's algorithm
  r32 DeltaX = EndX - StartX;
  r32 DeltaY = EndY - StartY;
  int SignX = (DeltaX < 0) ? -1 : 1;
  int SignY = (DeltaY < 0) ? -1 : 1;
  int X = (int)StartX;
  int Y = (int)StartY;
  r32 Error = 0;

  if (DeltaX) {
    r32 DeltaErr = Abs(DeltaY / DeltaX);
    while (X != (int)EndX) {
      SetPixel(X, Y, Color);
      X += SignX;
      Error += DeltaErr;
      while (Error >= 0.5f) {
        SetPixel(X, Y, Color);
        Y += SignY;
        Error -= 1.0f;
      }
    }
  } else if (DeltaY) {
    while (Y != (int)EndY) {
      SetPixel(X, Y, Color);
      Y += SignY;
    }
  }
}

internal void DEBUGDrawCircle(v2 Center, int Radius, u32 Color) {
  int X = (int)Center.x - Radius;
  int Y = (int)Center.y - Radius;
  int Height = Radius * 2;
  int Width = Radius * 2;
  int CenterX = (int)Center.x;
  int CenterY = (int)Center.y;
  int RadiusSq = Radius * Radius;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    for (int pX = X; pX < X + Width; pX++) {
      int nX = CenterX - pX;
      int nY = CenterY - pY;
      if ((nX * nX + nY * nY) <= RadiusSq) {
        *Pixel = Color;
      }
      Pixel++;
    }
    Row += Pitch;
  }
}

internal void DEBUGDrawEllipse(v2 Center, int Width, int Height, u32 Color) {
  int X = (int)Center.x - Width / 2;
  int Y = (int)Center.y - Height / 2;
  int CenterX = (int)Center.x;
  int CenterY = (int)Center.y;
  int HeightSq = Height * Height / 4;
  int WidthSq = Width * Width / 4;
  int RadiusSq = HeightSq * WidthSq;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    for (int pX = X; pX < X + Width; pX++) {
      int nX = CenterX - pX;
      int nY = CenterY - pY;
      if ((nX * nX * HeightSq + nY * nY * WidthSq) <= RadiusSq) {
        *Pixel = Color;
      }
      Pixel++;
    }
    Row += Pitch;
  }
}

internal void DEBUGDrawImage(v2 Position, bmp_file Image) {
  int Width = Image.Width;
  int Height = Image.Width;
  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;
  u32 *SrcRow =
      (u32 *)Image.Bitmap + Height * Width - Width;  // bottom row first

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    int *SrcPixel = (int *)SrcRow;

    for (int pX = X; pX < X + Width; pX++) {
      // TODO: bmp may not be masked
      u8 Red = UnmaskColor(*SrcPixel, Image.RedMask);
      u8 Green = UnmaskColor(*SrcPixel, Image.GreenMask);
      u8 Blue = UnmaskColor(*SrcPixel, Image.BlueMask);
      u8 Alpha = UnmaskColor(*SrcPixel, Image.AlphaMask);

      u32 ResultingColor = Red << 16 | Green << 8 | Blue;

      if (Alpha > 0 && Alpha < 0xFF) {
        r32 ExistingRed = (r32)((*Pixel >> 16) & 0xFF);
        r32 ExistingGreen = (r32)((*Pixel >> 8) & 0xFF);
        r32 ExistingBlue = (r32)((*Pixel >> 0) & 0xFF);

        r32 NewRed = (r32)((ResultingColor >> 16) & 0xFF);
        r32 NewGreen = (r32)((ResultingColor >> 8) & 0xFF);
        r32 NewBlue = (r32)((ResultingColor >> 0) & 0xFF);

        // Blending
        r32 t = (r32)Alpha / 255.0f;

        NewRed = NewRed * t + ExistingRed * (1 - t);
        NewGreen = NewGreen * t + ExistingGreen * (1 - t);
        NewBlue = NewBlue * t + ExistingBlue * (1 - t);

        *Pixel =
            (((u8)NewRed << 16) | ((u8)NewGreen << 8) | ((u8)NewBlue << 0));
      } else if (Alpha == 0xFF) {
        *Pixel = ResultingColor;
      } else {
        // do nothing
      }

      Pixel++;
      SrcPixel++;
    }
    Row += Pitch;
    SrcRow -= Width;
  }
}

internal bmp_file DEBUGReadBMPFile(char *Filename) {
  bmp_file Result = {};
  file_read_result FileReadResult =
      GameMemory->DEBUGPlatformReadEntireFile(Filename);

  bmp_file_header *BMPFileHeader = (bmp_file_header *)FileReadResult.Memory;
  bmp_info_header *BMPInfoHeader =
      (bmp_info_header *)((u8 *)FileReadResult.Memory +
                          sizeof(bmp_file_header));

  Result.Bitmap =
      (void *)((u8 *)FileReadResult.Memory + BMPFileHeader->bfOffBits);
  Result.Width = BMPInfoHeader->biWidth;
  Result.Height = BMPInfoHeader->biHeight;

  if (BMPInfoHeader->biCompression == 3)  // BI_BITFIELDS
  {
    Result.RedMask = BMPInfoHeader->biRedMask;
    Result.GreenMask = BMPInfoHeader->biGreenMask;
    Result.BlueMask = BMPInfoHeader->biBlueMask;
    Result.AlphaMask = BMPInfoHeader->biAlphaMask;
  }

  return Result;
}

internal void InitEntity(entity *Entity, char *ImgPath, v2 Position,
                         v2 Velocity, v2 Center, int Radius, r32 Mass) {
  bmp_file BMPFile = DEBUGReadBMPFile(ImgPath);
  Entity->Image = BMPFile;
  Entity->Position = Position;
  Entity->Velocity = Velocity;

  Entity->Center = Center;
  Entity->Radius = Radius;
  Entity->Mass = Mass;
  Entity->NotJumped = true;
}

internal void CollideWithWalls(entity *Entity) {
  r32 MinX = (r32)GameBackBuffer->Width * 0.05f;
  r32 MinY = 0.0f;
  r32 MaxX = (r32)GameBackBuffer->Width * 0.95f - (r32)Entity->Image.Width;
  r32 MaxY = (r32)GameBackBuffer->Height * 0.95f - (r32)Entity->Image.Height;

  if (Entity->Position.x < MinX) {
    Entity->Position.x = MinX;
    Entity->Velocity.x = -Entity->Velocity.x;
  }
  if (Entity->Position.y < MinY) {
    Entity->Position.y = MinY;
    Entity->Velocity.y = -Entity->Velocity.y;
  }
  if (Entity->Position.x > MaxX) {
    Entity->Position.x = MaxX;
    Entity->Velocity.x = -Entity->Velocity.x;
  }
  if (Entity->Position.y > MaxY) {
    Entity->Position.y = MaxY;
    Entity->Velocity.y = -Entity->Velocity.y;
    Entity->NotJumped = true;
  }
}

internal void LimitVelocity(entity *Entity, r32 Max) {
  if (Entity->Velocity.x > Max) Entity->Velocity.x = Max;
  if (Entity->Velocity.x < -Max) Entity->Velocity.x = -Max;
  if (Entity->Velocity.y > Max) Entity->Velocity.y = Max;
  if (Entity->Velocity.y < -Max) Entity->Velocity.y = -Max;
}

internal void DEBUGDrawEntity(entity *Entity) {
  // Draw the shadow
  v2 ShadowPosition = Entity->Position + Entity->Center;
  ShadowPosition.y = (r32)GameBackBuffer->Height * 0.95f - 5.0f;  // floor
  int Width = Entity->Image.Width -
              (int)((r32)Entity->Image.Width * 0.6f *
                    ((r32)(ShadowPosition.y - Entity->Position.y)) /
                    (r32)GameBackBuffer->Height);
  DEBUGDrawEllipse(ShadowPosition, Width, Width / 5, 0x00111111);

  // Draw the entity
  DEBUGDrawCircle(Entity->Position, Entity->Radius, 0x00FF0000);
}

internal bool32
Collides(v2 Center, int Radius, v2 RectCorner, int RectWidth, int RectHeight) {
  // Detects collisions betweer a ball and a rect
  if ((Center.x - (r32)Radius) > (RectCorner.x + (r32)RectWidth)) return false;
  if ((Center.x + (r32)Radius) < RectCorner.x) return false;
  if ((Center.y + (r32)Radius) < RectCorner.y) return false;

  return true;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  // Update global vars
  GameBackBuffer = Buffer;
  GameMemory = Memory;

  if (!Player) {
    Player = (entity *)GameMemoryAlloc(sizeof(entity));

    Player->Position = {300, 300};
    Player->Velocity = {1, 0};

    Player->Center = {36, 35};
    Player->Radius = 32;
    Player->Mass = 10;
    Player->NotJumped = true;
  }

  // Fill background
  DEBUGDrawRectangle({0, 0}, GameBackBuffer->Width, GameBackBuffer->Height,
                     0x00002222);  // OMG

  // Update Player
  {
    v2 Direction = {};
    player_input *Input = &NewInput->Players[0];

    if (Input->Up.EndedDown && Player->NotJumped) Direction.y -= 2.0f;
    if (Input->Right.EndedDown) Direction.x += 1.0f;
    if (Input->Left.EndedDown) Direction.x -= 1.0f;

    if (Direction.x != 0 && Direction.y != 0) {
      Direction *= 0.70710678118f;
    }

    Direction *= 0.25f;                         // speed, px/ms
    Direction.x -= 0.2f * Player->Velocity.x;   // friction
    Direction.y -= 0.05f * Player->Velocity.y;  // friction

    Player->Velocity += Direction;

    r32 FloorY = (r32)GameBackBuffer->Height * 0.95f - (r32)Player->Image.Height;
    if (Player->Position.y <= (FloorY - Player->Image.Height / 2)) {
      Player->Velocity.y += 0.1f;  // gravity
    }

    Player->Position += Player->Velocity * NewInput->dtForFrame;

    CollideWithWalls(Player);

    r32 MaxJump = (r32)GameBackBuffer->Height * 0.5f;
    if (Player->Position.y <= MaxJump) {
      Player->NotJumped = false;
    }

    LimitVelocity(Player, 1.0f);

  }

  DEBUGDrawEntity(Player);
}
