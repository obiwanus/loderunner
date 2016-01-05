#include "loderunner.h"
#include "loderunner_levels.cpp"

// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global bool32 gClock = true;

global level Level;
global bmp_file *gImage;
global i32 gScore;
global bool32 gUpdateScore = true;

global bool32 gDebug = false;

global int kTileWidth = 32;
global int kTileHeight = 32;
global int kHumanWidth = 24;
global int kHumanHeight = 32;

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

internal void DrawRectangle(v2i Position, int Width, int Height, u32 Color) {
  int X = Position.x;
  int Y = Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + GameBackBuffer->StartOffset +
            Pitch * Y + X * GameBackBuffer->BytesPerPixel;

  for (int pY = Y; pY < Y + Height; pY++) {
    u32 *Pixel = (u32 *)Row;
    for (int pX = X; pX < X + Width; pX++) {
      *Pixel++ = Color;
    }
    Row += Pitch;
  }
}

internal void DrawRectangle(int X, int Y, int Width, int Height, u32 Color) {
  v2i Position = {X, Y};
  DrawRectangle(Position, Width, Height, Color);
}

inline void SetPixel(int X, int Y, u32 Color) {
  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + GameBackBuffer->StartOffset +
            Pitch * Y + X * GameBackBuffer->BytesPerPixel;
  u32 *Pixel = (u32 *)Row;
  *Pixel = Color;
}

internal void DrawSprite(v2i Position, int Width, int Height, int XOffset,
                         int YOffset) {
  // NOTE: if we need a draw image function, it's easily derived from this one

  // If we ever need another image, we'll need a new func
  bmp_file *Image = gImage;

  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  int SrcPitch = gImage->Width;

  u8 *Row = (u8 *)GameBackBuffer->Memory + GameBackBuffer->StartOffset +
            Pitch * Y + X * GameBackBuffer->BytesPerPixel;
  u32 *BottomLeftCorner =
      (u32 *)Image->Bitmap + Image->Width * (Image->Height - 1);
  u32 *SrcRow = BottomLeftCorner - SrcPitch * YOffset + XOffset;

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    int *SrcPixel = (int *)SrcRow;

    for (int pX = X; pX < X + Width; pX++) {
      u8 Red = UnmaskColor(*SrcPixel, Image->RedMask);
      u8 Green = UnmaskColor(*SrcPixel, Image->GreenMask);
      u8 Blue = UnmaskColor(*SrcPixel, Image->BlueMask);
      u8 Alpha = UnmaskColor(*SrcPixel, Image->AlphaMask);

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
    SrcRow -= SrcPitch;
  }
}

tile_type CheckTile(int Col, int Row) {
  if (Row < 0 || Row >= Level.Height || Col < 0 || Col >= Level.Width) {
    return LVL_INVALID;
  }
  return Level.Contents[Row][Col];
}

void SetTile(int Col, int Row, tile_type Value) {
  if (Row < 0 || Row >= Level.Height || Col < 0 || Col >= Level.Width) {
    return;  // Invalid tile
  }
  Level.Contents[Row][Col] = Value;
}

void DrawTile(int Col, int Row) {
  if (Col < 0 || Row < 0 || Col >= Level.Width || Row >= Level.Height) {
    // Don't draw outside level boundaries
    return;
  }
  v2i Position = {};
  Position.x = (Col * kTileWidth);
  Position.y = (Row * kTileHeight);
  int Value = CheckTile(Col, Row);
  if (Value == LVL_BRICK || Value == LVL_BRICK_FAKE) {
    DrawSprite(Position, kTileWidth, kTileHeight, 160, 96);
  } else if (Value == LVL_BRICK_HARD) {
    DrawSprite(Position, kTileWidth, kTileHeight, 128, 96);
  } else if (Value == LVL_LADDER) {
    DrawSprite(Position, kTileWidth, kTileHeight, 96, 128);
  } else if (Value == LVL_ROPE) {
    DrawSprite(Position, kTileWidth, kTileHeight, 128, 128);
  } else {
    DrawRectangle(Position, kTileWidth, kTileHeight, 0x000A0D0B);
  }
}

void DrawText(char *String, int X, int Y) {
  v2i Position = {};
  Position.x = X;
  Position.y = Y;
  v2i Offset = {};

  while (*String != '\0') {
    char c = *String++;
    bool32 CharFound = true;

    if (c == 'a') {
      Offset = {96, 192};
    } else if (c == 'b') {
      Offset = {128, 192};
    } else if (c == 'c') {
      Offset = {160, 192};
    } else if (c == 'd') {
      Offset = {192, 192};
    } else if (c == 'e') {
      Offset = {224, 192};
    } else if (c == 'o') {
      Offset = {96, 224};
    } else if (c == 'r') {
      Offset = {128, 224};
    } else if (c == 'u') {
      Offset = {160, 224};
    } else if (c == 'n') {
      Offset = {192, 224};
    } else if (c == 'l') {
      Offset = {224, 224};
    } else if (c == 'v') {
      Offset = {96, 256};
    } else if (c == 's') {
      Offset = {128, 256};
    } else if (c == 'i') {
      Offset = {160, 256};
    } else if (c == 'f') {
      Offset = {192, 256};
    } else if (c == 'y') {
      Offset = {224, 256};
    } else if (c == 'k') {
      Offset = {96, 288};
    } else if (c == 'p') {
      Offset = {128, 288};
    } else if (c == 'm') {
      Offset = {160, 288};
    } else if (c == 't') {
      Offset = {192, 288};
    } else if (c == '0') {
      Offset = {96, 320};
    } else if (c == '1') {
      Offset = {128, 320};
    } else if (c == '2') {
      Offset = {160, 320};
    } else if (c == '3') {
      Offset = {192, 320};
    } else if (c == '4') {
      Offset = {224, 320};
    } else if (c == '5') {
      Offset = {96, 352};
    } else if (c == '6') {
      Offset = {128, 352};
    } else if (c == '7') {
      Offset = {160, 352};
    } else if (c == '8') {
      Offset = {192, 352};
    } else if (c == '9') {
      Offset = {224, 352};
    } else {
      CharFound = false;
    }

    if (CharFound) {
      DrawSprite(Position, kTileWidth, kTileHeight, Offset.x, Offset.y);
    }

    Position.x += kTileWidth;
  }
}

internal bmp_file DEBUGReadBMPFile(char const *Filename) {
  bmp_file Result = {};
  file_read_result FileReadResult =
      GameMemory->DEBUGPlatformReadEntireFile(Filename);

  Assert(FileReadResult.MemorySize > 0);

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

internal bmp_file *LoadSprite(char const *Filename) {
  bmp_file *Result = (bmp_file *)GameMemoryAlloc(sizeof(bmp_file));
  *Result = DEBUGReadBMPFile(Filename);

  return Result;
}

void LoadLevel(int Index) {
  // Zero everything
  Level = {};
  Level.IsInitialized = true;
  Level.Index = Index;
  Level.IsDrawn = false;
  Level.TileBeingDrawn = 0;
  gUpdateScore = true;

  // Init disappearing animation
  {
    animation *Animation = &Level.Disappearing;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {96, 160, 2};
    Animation->Frames[1] = {128, 160, 2};
    Animation->Frames[2] = {160, 160, 2};
  }

  const char *LevelString = LEVELS[Index];

  // Get level info
  int MaxWidth = 0;
  int Width = 0;
  int Height = 1;

  int i = 0;
  char Symbol = LevelString[0];

  while ((Symbol = LevelString[i++]) != '\0') {
    if (Symbol == '\n') {
      if (MaxWidth < Width) {
        MaxWidth = Width;
      }
      Width = 0;
      Height += 1;
    } else if (Symbol != '\r') {
      Width += 1;
    }
    if (Symbol == 'p') {
      Level.PlayerCount++;
    }
    if (Symbol == 'e' || Symbol == 'E') {
      Level.EnemyCount++;
    }
    if (Symbol == 't') {
      Level.TreasureCount++;
    }
  }

  Level.Width = MaxWidth;
  Level.DrawTilesPerFrame = Level.Width / 4;
  Level.Height = Height;

  // Allocate memory for enemies and treasures
  Level.Enemies = (enemy *)GameMemoryAlloc(sizeof(enemy) * Level.EnemyCount);
  Level.Treasures =
      (treasure *)GameMemoryAlloc(sizeof(treasure) * Level.TreasureCount);

  // Read level data
  {
    int Column = 0;
    int Row = 0;
    int EnemyNum = 0;
    int TreasureNum = 0;

    i = 0;
    Symbol = LevelString[0];

    while ((Symbol = LevelString[i++]) != '\0') {
      tile_type Value = LVL_BLANK;

      if (Symbol == '|')
        Value = LVL_WIN_LADDER;
      else if (Symbol == 't') {
        Value = LVL_BLANK;
        treasure *Treasure = &Level.Treasures[TreasureNum];
        TreasureNum++;
        *Treasure = {};  // zero everything
        Treasure->TileX = Column;
        Treasure->TileY = Row;
        Treasure->Width = kTileWidth;
        Treasure->Height = kTileHeight;
        Treasure->X = Treasure->TileX * kTileWidth;
        Treasure->Y = Treasure->TileY * kTileHeight;
      } else if (Symbol == 'r') {
        Value = LVL_RESPAWN;
        Level.Respawns[Level.RespawnCount] = {Column, Row};
        Level.RespawnCount++;
      } else if (Symbol == '=')
        Value = LVL_BRICK;
      else if (Symbol == '+')
        Value = LVL_BRICK_HARD;
      else if (Symbol == 'f')
        Value = LVL_BRICK_FAKE;
      else if (Symbol == 'T')
        Value = LVL_TREASURE_HOLDER;
      else if (Symbol == 'd')
        Value = LVL_BLANK_TMP;
      else if (Symbol == '#')
        Value = LVL_LADDER;
      else if (Symbol == '-')
        Value = LVL_ROPE;
      else if (Symbol == 'e' || Symbol == 'E') {
        Value = Symbol == 'e' ? LVL_BLANK : LVL_WIN_LADDER;
        enemy *Enemy = &Level.Enemies[EnemyNum];
        EnemyNum++;
        *Enemy = {};  // zero everything
        Enemy->TileX = Column;
        Enemy->TileY = Row;
        Enemy->X = Enemy->TileX * kTileWidth + kTileWidth / 2;
        Enemy->Y = Enemy->TileY * kTileHeight + kTileHeight / 2;
      } else if (Symbol == 'p') {
        Value = LVL_BLANK;
        player *Player = &Level.Players[0];
        if (Player->IsActive) {
          Player = &Level.Players[1];
        }
        *Player = {};  // zero everything
        Player->IsActive = true;
        Player->TileX = Column;
        Player->TileY = Row;
        Player->X = Player->TileX * kTileWidth + kTileWidth / 2;
        Player->Y = Player->TileY * kTileHeight + kTileHeight / 2;
      }

      if (Symbol == '\n') {
        Column = 0;
        ++Row;
      } else if (Symbol != '\r') {
        Assert(Column < Level.Width);
        Assert(Row < Level.Height);
        Level.Contents[Row][Column] = Value;
        ++Column;
      }
    }
  }

  // Init players
  for (int player_num = 0; player_num < 2; player_num++) {
    player *Player = &Level.Players[player_num];
    if (Player->IsInitialized) {
      continue;
    }

    Player->IsInitialized = true;
    Player->Width = kHumanWidth;
    Player->Height = kHumanHeight;
    Player->Animation = &Player->Blinking;
    Player->Animate = true;
    Player->Facing = RIGHT;

    // Init animations
    frame *Frames = NULL;
    animation *Animation = NULL;

    // Falling
    Animation = &Player->Falling;
    Animation->FrameCount = 1;
    Animation->Frames[0] = {72, 0, 0};

    // Blinking
    Animation = &Player->Blinking;
    Animation->FrameCount = 2;
    Animation->Frames[0] = {72, 0, 8};
    Animation->Frames[1] = {72, 32, 8};

    // Going right
    Animation = &Player->GoingRight;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 0, 3};
    Animation->Frames[1] = {24, 0, 2};
    Animation->Frames[2] = {48, 0, 3};

    // Going left
    Animation = &Player->GoingLeft;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 32, 3};
    Animation->Frames[1] = {24, 32, 2};
    Animation->Frames[2] = {48, 32, 3};

    // On rope right
    Animation = &Player->RopeRight;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 64, 3};
    Animation->Frames[1] = {24, 64, 2};
    Animation->Frames[2] = {48, 64, 3};

    // On rope left
    Animation = &Player->RopeLeft;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 96, 3};
    Animation->Frames[1] = {24, 96, 2};
    Animation->Frames[2] = {48, 96, 3};

    // On ladder
    Animation = &Player->Climbing;
    Animation->FrameCount = 2;
    Animation->Frames[0] = {0, 128, 4};
    Animation->Frames[1] = {24, 128, 4};
  }

  // Init enemies
  for (int enemy_num = 0; enemy_num < Level.EnemyCount; enemy_num++) {
    enemy *Enemy = &Level.Enemies[enemy_num];
    if (Enemy->IsInitialized) {
      continue;
    }

    Enemy->IsInitialized = true;

    Enemy->Width = kHumanWidth;
    Enemy->Height = kHumanHeight;
    Enemy->Animation = &Enemy->Falling;
    Enemy->CarriesTreasure = -1;

    // Init animations
    animation *Animation = NULL;

    // Falling
    Animation = &Enemy->Falling;
    Animation->FrameCount = 1;
    Animation->Frames[0] = {72, 160, 0};

    // Going right
    Animation = &Enemy->GoingRight;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 160, 6};
    Animation->Frames[1] = {24, 160, 4};
    Animation->Frames[2] = {48, 160, 6};

    // Going left
    Animation = &Enemy->GoingLeft;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 192, 6};
    Animation->Frames[1] = {24, 192, 4};
    Animation->Frames[2] = {48, 192, 6};

    // On rope right
    Animation = &Enemy->RopeRight;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 224, 6};
    Animation->Frames[1] = {24, 224, 4};
    Animation->Frames[2] = {48, 224, 6};

    // On rope left
    Animation = &Enemy->RopeLeft;
    Animation->FrameCount = 3;
    Animation->Frames[0] = {0, 256, 6};
    Animation->Frames[1] = {24, 256, 4};
    Animation->Frames[2] = {48, 256, 6};

    // On ladder
    Animation = &Enemy->Climbing;
    Animation->FrameCount = 2;
    Animation->Frames[0] = {0, 288, 8};
    Animation->Frames[1] = {24, 288, 8};
  }
}

internal bool32 CanGoThroughTile(int TileX, int TileY) {
  tile_type Tile = CheckTile(TileX, TileY);
  if (Tile == LVL_BRICK || Tile == LVL_BRICK_HARD || Tile == LVL_BLANK_TMP ||
      Tile == LVL_INVALID || Tile == LVL_BRICK_FAKE) {
    return false;
  } else {
    return true;
  }
}

rect GetBoundingRect(entity *Entity) {
  rect Result;

  Result.Left = Entity->X - Entity->Width / 2;
  Result.Right = Entity->X + Entity->Width / 2;
  Result.Top = Entity->Y - Entity->Height / 2;
  Result.Bottom = Entity->Y + Entity->Height / 2;

  return Result;
}

rect GetTileRect(int TileX, int TileY) {
  rect Result;

  Result.Left = TileX * kTileWidth;
  Result.Right = (TileX + 1) * kTileWidth;
  Result.Top = TileY * kTileHeight;
  Result.Bottom = (TileY + 1) * kTileHeight;

  return Result;
}

inline bool32 RectsCollide(rect Rect1, rect Rect2) {
  if (Rect1.Right > Rect2.Left && Rect1.Left < Rect2.Right &&
      Rect1.Bottom > Rect2.Top && Rect1.Top < Rect2.Bottom) {
    return true;
  }

  return false;
}

bool32 EntitiesCollide(entity *Entity1, entity *Entity2) {
  rect Rect1 = GetBoundingRect(Entity1);
  rect Rect2 = GetBoundingRect(Entity2);

  return RectsCollide(Rect1, Rect2);
}

bool32 EntitiesCollide(entity *Entity1, entity *Entity2, int XAdjust,
                       int YAdjust) {
  rect Rect1 = GetBoundingRect(Entity1);
  rect Rect2 = GetBoundingRect(Entity2);

  Rect1.Left -= XAdjust;
  Rect1.Right += XAdjust;
  Rect1.Top -= YAdjust;
  Rect1.Bottom += YAdjust;

  Rect2.Left -= XAdjust;
  Rect2.Right += XAdjust;
  Rect2.Top -= YAdjust;
  Rect2.Bottom += YAdjust;

  return RectsCollide(Rect1, Rect2);
}

bool32 AcceptableMove(person *Person, bool32 IsEnemy) {
  // Tells whether the player can be legitimately
  // placed in its position

  rect PersonRect = GetBoundingRect(Person);

  // Don't go away from the level
  if (PersonRect.Left < 0 || PersonRect.Top < 0 ||
      PersonRect.Right > Level.Width * kTileWidth ||
      PersonRect.Bottom > Level.Height * kTileHeight)
    return false;

  int TileX = ((int)Person->X + Person->Width / 2) / kTileWidth;
  int TileY = ((int)Person->Y + Person->Width / 2) / kTileHeight;
  int StartCol = (TileX <= 0) ? 0 : TileX - 1;
  int EndCol = (TileX >= Level.Width - 1) ? TileX : TileX + 1;
  int StartRow = (TileY <= 0) ? 0 : TileY - 1;
  int EndRow = (TileY >= Level.Height - 1) ? TileY : TileY + 1;

  for (int Row = StartRow; Row <= EndRow; Row++) {
    for (int Col = StartCol; Col <= EndCol; Col++) {
      int Tile = CheckTile(Col, Row);
      if (Tile != LVL_BRICK && Tile != LVL_BRICK_HARD) continue;

      // Collision check
      rect TileRect = GetTileRect(Col, Row);

      if (RectsCollide(PersonRect, TileRect)) {
        return false;
      }
    }
  }

  // Collisions with enemies
  for (int i = 0; i < Level.EnemyCount; i++) {
    enemy *Enemy = &Level.Enemies[i];
    if (Enemy == (enemy *)Person) continue;
    if (EntitiesCollide(Enemy, Person)) {
      return false;
    }
  }

  if (IsEnemy) {
    // Do not fall through player-made pits
    if (CheckTile(Person->TileX, Person->TileY) == LVL_BLANK_TMP &&
        PersonRect.Bottom > (Person->TileY + 1) * kTileWidth) {
      return false;
    }
  }

  return true;
}

inline void SetWMapPoint(int Col, int Row, water_point Point) {
  if (Row < 0 || Row >= Level.Height || Col < 0 || Col >= Level.Width) {
    Assert(0);
    return;
  }
  Level.WaterMap[Row][Col] = Point;
}

inline water_point CheckWMapPoint(int Col, int Row) {
  if (Row < 0 || Row >= Level.Height || Col < 0 || Col >= Level.Width) {
    return WATERMAP_OBSTACLE;
  }
  return Level.WaterMap[Row][Col];
}

inline void SetDMapPoint(int Col, int Row, int X, int Y) {
  if (Row < 0 || Row >= Level.Height || Col < 0 || Col >= Level.Width) {
    Assert(0);
    return;
  }
  int Value = Y * Level.Width + X;
  Level.DirectionMap[Row][Col] = Value;
}

#define DM_TARGET -1
#define FRONTIER_MAX_SIZE 500

void FindPath(enemy *Enemy, player *Player) {
  // NOTE: -1 works with memset, but -2 would not
  memset(Level.DirectionMap, -1, sizeof(Level.DirectionMap));
  memset(Level.WaterMap, 0, sizeof(Level.WaterMap));

  Level.DirectionMap[Player->TileY][Player->TileX] = DM_TARGET;

  // Pre-fill watermap with obstacles
  for (int Row = 0; Row < Level.Height; Row++) {
    for (int Col = 0; Col < Level.Width; Col++) {
      if (!CanGoThroughTile(Col, Row) &&
          !(Col == Enemy->TileX && Row == Enemy->TileY)) {
        SetWMapPoint(Col, Row, WATERMAP_OBSTACLE);
      }
    }
  }
  Level.WaterMap[Player->TileY][Player->TileX] = WATERMAP_WATER;

  bool32 NewPathFound = false;
  int Iteration = 0;
  while (Iteration++ < MAX_PATH_LENGTH) {
    for (int Row = 0; Row < Level.Height; Row++) {
      for (int Col = 0; Col < Level.Width; Col++) {
        if (CheckWMapPoint(Col, Row) != WATERMAP_WATER) continue;

        int X = Col;
        int Y = Row;

        if (X == Enemy->TileX && Y == Enemy->TileY) {
          NewPathFound = true;
          break;
        }

        // Point above
        X = Col;
        Y = Row - 1;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if (CanGoThroughTile(X, Y) && CheckTile(X, Y) != LVL_BLANK_TMP) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point below
        X = Col;
        Y = Row + 1;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if (CheckTile(X, Y) == LVL_LADDER ||
              Enemy->ParalyseImmunityCooldown > 0 &&
                  CheckTile(X, Y) == LVL_BLANK_TMP) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point on the left
        X = Col - 1;
        Y = Row;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if ((CanGoThroughTile(X, Y) &&
               (!CanGoThroughTile(X, Y + 1) ||
                CheckTile(X, Y + 1) == LVL_LADDER ||
                CheckTile(X, Y + 1) == LVL_BLANK_TMP)) ||
              CheckTile(X, Y) == LVL_ROPE) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point on the right
        X = Col + 1;
        Y = Row;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if ((CanGoThroughTile(X, Y) &&
               (!CanGoThroughTile(X, Y + 1) ||
                CheckTile(X, Y + 1) == LVL_LADDER ||
                CheckTile(X, Y + 1) == LVL_BLANK_TMP)) ||
              CheckTile(X, Y) == LVL_ROPE) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }
      }
    }

    if (NewPathFound) {
      Enemy->PathExists = true;
      Enemy->PathPointIndex = 0;
      break;
    }
  }

  // Build path if found
  if (NewPathFound) {
    int X = Enemy->TileX;
    int Y = Enemy->TileY;
    for (int i = 0; i < MAX_PATH_LENGTH; i++) {
      int NextStep = Level.DirectionMap[Y][X];
      X = NextStep % Level.Width;
      Y = NextStep / Level.Width;
      Enemy->Path[i].x = X;
      Enemy->Path[i].y = Y;
      if (X == Player->TileX && Y == Player->TileY) {
        Enemy->PathLength = i + 1;
        break;
      }
    }
  }
}

void ErasePerson(person *Person) {
  // Redraw tiles covered by person
  for (int Row = Person->TileY - 1; Row <= Person->TileY + 1; Row++) {
    for (int Col = Person->TileX - 1; Col <= Person->TileX + 1; Col++) {
      DrawTile(Col, Row);
    }
  }
}

void AddScore(int Value) {
  gScore += Value;
  gUpdateScore = true;
}

void UpdatePerson(person *Person, bool32 IsEnemy, int Speed, bool32 PressedUp,
                  bool32 PressedDown, bool32 PressedLeft, bool32 PressedRight,
                  bool32 PressedFire, bool32 Turbo) {
  bool32 Animate = false;

  int OldX = Person->X;
  int OldY = Person->Y;

  bool32 OnLadder = false;
  int LadderTileX = 0;
  {
    int Left = (Person->X - Person->Width / 2) / kTileWidth;
    int Right = (Person->X + Person->Width / 2) / kTileWidth;
    int Top = (Person->Y - Person->Height / 2) / kTileHeight;
    int Bottom = (Person->Y + Person->Height / 2) / kTileHeight;
    if (CheckTile(Left, Top) == LVL_LADDER ||
        CheckTile(Left, Bottom) == LVL_LADDER) {
      OnLadder = true;
      LadderTileX = Left;
    } else if (CheckTile(Right, Top) == LVL_LADDER ||
               CheckTile(Right, Bottom) == LVL_LADDER) {
      OnLadder = true;
      LadderTileX = Right;
    }
    if (Person->ParalyseImmunityCooldown > 0 && OnLadder == false) {
      if (CheckTile(Left, Top) == LVL_BLANK_TMP ||
          CheckTile(Left, Bottom) == LVL_BLANK_TMP) {
        OnLadder = true;
        LadderTileX = Left;
      } else if (CheckTile(Right, Top) == LVL_BLANK_TMP ||
                 CheckTile(Right, Bottom) == LVL_BLANK_TMP) {
        OnLadder = true;
        LadderTileX = Right;
      }
    }
  }

  bool32 LadderBelow = false;
  {
    int Col = Person->TileX;
    int Row = Person->TileY + 1;
    int PersonBottom = (int)Person->Y + Person->Height / 2;
    int TileTop = Row * kTileHeight;
    if (CheckTile(Col, Row) == LVL_LADDER ||
        (Person->ParalyseImmunityCooldown > 0 &&
         CheckTile(Col, Row) == LVL_BLANK_TMP)) {
      if (PersonBottom >= TileTop) LadderBelow = true;
    }
  }

  Person->CanClimb = false;
  if (OnLadder) {
    int LadderCenter = LadderTileX * kTileWidth + kTileWidth / 2;
    Person->CanClimb = Abs(Person->X - LadderCenter) < (Person->Width / 3);
  }

  Person->OnRope = false;
  if (CheckTile(Person->TileX, Person->TileY) == LVL_ROPE) {
    int RopeY = Person->TileY * kTileHeight;
    int PersonTop = Person->Y - Person->Height / 2;
    Person->OnRope = (PersonTop == RopeY) ||
                     (RopeY - PersonTop > 0 && RopeY - PersonTop <= 3);
    // Adjust Person on a rope
    if (Person->OnRope) {
      Person->Y = RopeY + Person->Height / 2;
    }
  }

  // Gravity
  bool32 WasFalling = Person->IsFalling;
  Person->IsFalling = false;
  {
    int Old = Person->Y;
    Person->Y += Speed;
    if (!AcceptableMove(Person, IsEnemy) || OnLadder || LadderBelow || Turbo ||
        Person->OnRope) {
      Person->Y = Old;
      Person->IsFalling = false;
    } else {
      Person->IsFalling = true;
      Animate = true;
      Person->Animation = &Person->Falling;
    }
  }

  // Adjust in a player-made pit
  if (Person->IsFalling && !WasFalling) {
    for (int i = Person->TileY + 1; i < Level.Height; i++) {
      if (CheckTile(Person->TileX, i) == LVL_BLANK_TMP) {
        Person->X = Person->TileX * kTileWidth + kTileWidth / 2;
        break;
      }
    }
  }

  // Update based on movement keys
  if (PressedRight && (!Person->IsFalling || Turbo)) {
    int Old = Person->X;
    Person->X += Speed;
    if (!AcceptableMove(Person, IsEnemy)) {
      Person->X = Old;
    } else {
      if (!Person->OnRope) {
        Person->Animation = &Person->GoingRight;
      } else {
        Person->Animation = &Person->RopeRight;
      }
      // Adjust so it's easy to grab a rope
      if (!IsEnemy && !PressedUp && !PressedDown &&
          CheckTile(Person->TileX, Person->TileY) == LVL_LADDER &&
          CheckTile(Person->TileX + 1, Person->TileY) == LVL_ROPE) {
        int PersonTop = Person->Y - Person->Height / 2;
        int RopeY = Person->TileY * kTileHeight;
        if (Abs(PersonTop - RopeY) < 10) {
          Person->Y = RopeY + Person->Height / 2;
        }
      }
      Animate = true;
      Person->Facing = RIGHT;
    }
  }

  if (PressedLeft && (!Person->IsFalling || Turbo)) {
    int Old = Person->X;
    Person->X -= Speed;
    if (!AcceptableMove(Person, IsEnemy)) {
      Person->X = Old;
    } else {
      if (!Person->OnRope) {
        Person->Animation = &Person->GoingLeft;
      } else {
        Person->Animation = &Person->RopeLeft;
      }
      // @copypaste
      // Adjust so it's easy to grab a rope
      if (!IsEnemy && !PressedUp && !PressedDown &&
          CheckTile(Person->TileX, Person->TileY) == LVL_LADDER &&
          CheckTile(Person->TileX - 1, Person->TileY) == LVL_ROPE) {
        int PersonTop = Person->Y - Person->Height / 2;
        int RopeY = Person->TileY * kTileHeight;
        if (Abs(PersonTop - RopeY) < 10) {
          Person->Y = RopeY + Person->Height / 2;
        }
      }
      Animate = true;
      Person->Facing = LEFT;
    }
  }

  bool32 Climbing = false;
  if (PressedUp && (Person->CanClimb || Turbo)) {
    int Old = Person->Y;
    Person->Y -= Speed;

    // @refactor?
    int PlayerTile = CheckTile(Person->TileX, Person->TileY);
    int PersonBottom = (int)Person->Y + Person->Height / 2;

    bool32 GotFlying =
        (PlayerTile == LVL_BLANK || PlayerTile == LVL_WIN_LADDER ||
         PlayerTile == LVL_ROPE) &&
        (PersonBottom < (Person->TileY + 1) * kTileHeight);

    if (!AcceptableMove(Person, IsEnemy) || GotFlying && !Turbo) {
      Person->Y = Old;

      // Check if we won
      if (!IsEnemy && Level.AllTreasuresCollected && OnLadder &&
          Person->Y <= Person->Height / 2) {
        Level.Index++;
        if (Level.Index == kLevelCount) {
          exit(0);
        }
        AddScore(Level.EnemyCount * Level.TreasureCount * 100);
        Level.IsDisappearing = true;
        return;
      }
    } else {
      Climbing = true;
      Person->Animation = &Person->Climbing;
      Animate = true;
    }
  }

  Person->CanDescend = false;
  if (Person->CanClimb || LadderBelow || Person->OnRope || Turbo) {
    int Old = Person->Y;
    Person->Y += Speed;
    if (AcceptableMove(Person, IsEnemy)) {
      Person->CanDescend = true;
    }
    Person->Y = Old;
  }

  bool32 Descending = false;
  if (PressedDown && Person->CanDescend) {
    Person->Y += Speed;
    Descending = true;
    Person->Animation = &Person->Climbing;
    Animate = true;
  }

  // Get the direction the person is going in
  {
    Person->Going = NOWHERE;

    int DeltaX = Person->X - OldX;
    int DeltaY = Person->Y - OldY;

    if (Abs(DeltaX) >= Abs(DeltaY)) {
      Person->Going = DeltaX > 0 ? RIGHT : LEFT;
    } else {
      Person->Going = DeltaY > 0 ? DOWN : UP;
    }
  }

  // Adjust Person on ladder
  if (Person->CanClimb &&
      ((Climbing && PressedUp) || (Descending && PressedDown))) {
    Person->X = Person->TileX * kTileWidth + kTileWidth / 2;
  }

  // Check for collisions with enemies
  if (Person->BumpCooldown <= 0) {
    for (int i = 0; i < Level.EnemyCount; i++) {
      enemy *Enemy = &Level.Enemies[i];
      if (Enemy == (enemy *)Person) continue;

      // Use a larger bounding rect
      const int kRectAdjust = 4;

      if (EntitiesCollide(Person, Enemy, kRectAdjust, 0)) {
        Person->BumpCooldown = 40;
        Enemy->BumpCooldown = 20;

        // Send in different directions
        if (Person->X < Enemy->X) {
          Person->DirectionX = LEFT;
          Enemy->DirectionX = RIGHT;
        } else if (Person->X > Enemy->X) {
          Person->DirectionX = RIGHT;
          Enemy->DirectionX = LEFT;
        }

        if (Person->Y < Enemy->Y) {
          Person->DirectionY = UP;
          Enemy->DirectionY = DOWN;
        } else if (Person->Y > Enemy->Y) {
          Person->DirectionY = DOWN;
          Enemy->DirectionY = UP;
        }
      }
    }
  }

  Person->Animate =
      Animate && !(PressedDown && PressedUp) && !(PressedLeft && PressedRight);

  // Update Person tile
  Person->TileX = (int)Person->X / kTileWidth;
  Person->TileY = (int)Person->Y / kTileHeight;

  // Fire
  if (PressedFire && !Person->IsFalling && !Person->FireCooldown) {
    int TileY = Person->TileY + 1;
    int TileX = -10;
    int AdjustPersonX = 0;

    if (Person->Facing == LEFT) {
      int BorderX = (Person->TileX + 1) * kTileWidth - (kHumanWidth / 2 - 4);
      if (Person->X < BorderX) {
        TileX = Person->TileX - 1;
      } else {
        TileX = Person->TileX;
        AdjustPersonX = kHumanWidth / 2;
      }
    } else if (Person->Facing == RIGHT) {
      int BorderX = Person->TileX * kTileWidth + (kHumanWidth / 2 - 4);
      if (Person->X < BorderX) {
        TileX = Person->TileX;
        AdjustPersonX = -kHumanWidth / 2;
      } else {
        TileX = Person->TileX + 1;
      }
    }
    Assert(TileX != -10);

    int TileToBreak = CheckTile(TileX, TileY);
    int TileAbove = CheckTile(TileX, TileY - 1);

    if (TileToBreak == LVL_BRICK && TileAbove != LVL_BRICK &&
        TileAbove != LVL_BRICK_HARD && TileAbove != LVL_LADDER) {
      // Adjust the Person
      Person->X += AdjustPersonX;

      // Crush the brick
      Level.Contents[TileY][TileX] = LVL_BLANK_TMP;
      DrawTile(TileX, TileY);
      Person->FireCooldown = 30;

      // Remember that
      crushed_brick *Brick =
          &Level.CrushedBricks[Level.NextCrushedBrickAvailable];
      Level.NextCrushedBrickAvailable =
          (Level.NextCrushedBrickAvailable + 1) % kCrushedBrickCount;

      Assert(Brick->IsUsed == false);

      Brick->IsUsed = true;
      Brick->TileX = TileX;
      Brick->TileY = TileY;
      Brick->Width = 32;
      Brick->Height = 64;
      Brick->State = Brick->CRUSHING;
      Brick->Countdown = 60 * 8;  // 8 seconds

      // Init animation
      animation *Animation = NULL;

      Animation = &Brick->Breaking;
      Animation->FrameCount = 3;
      Animation->Frame = 0;
      Animation->Frames[0] = {96, 32, 2};
      Animation->Frames[1] = {128, 32, 2};
      Animation->Frames[2] = {160, 32, 2};

      Animation = &Brick->Restoring;
      Animation->FrameCount = 4;
      Animation->Frame = 0;
      Animation->Frames[0] = {192, 0, 8};
      Animation->Frames[1] = {192, 32, 8};
      Animation->Frames[2] = {192, 64, 8};
      Animation->Frames[3] = {192, 96, 8};
    }
  }
  if (Person->FireCooldown > 0) Person->FireCooldown--;

  // Death?
  if (!IsEnemy) {
    int kRectAdjust = 5;
    for (int enemy_num = 0; enemy_num < Level.EnemyCount; enemy_num++) {
      enemy *Enemy = &Level.Enemies[enemy_num];
      if (EntitiesCollide(Person, Enemy, -kRectAdjust, -kRectAdjust)) {
        Person->IsDead = true;
        gClock = false;
      }
    }
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  // Update global vars
  GameBackBuffer = Buffer;
  GameMemory = Memory;

  // Load sprites
  if (gImage == NULL) {
    gImage = LoadSprite("img/sprites.bmp");
  }

  // Init level
  if (!Level.IsInitialized) {
    Level.Index = 0;

    LoadLevel(Level.Index);
  }

  if (RedrawLevel || (!Level.IsDrawn && Level.TileBeingDrawn == 0)) {
    // Fill background
    u32 *Pixel = (u32 *)GameBackBuffer->Memory;
    for (int i = 0; i < GameBackBuffer->Width * GameBackBuffer->Height; i++) {
      *Pixel++ = 0x000A0D0B;
    }

    // Set the offset to draw in the centre
    {
      int Left = (GameBackBuffer->Width - Level.Width * kTileWidth) / 2;
      int Top =
          (GameBackBuffer->Height - ((Level.Height + 2) * kTileHeight)) / 2;
      GameBackBuffer->StartOffset =
          (Top * GameBackBuffer->Width + Left) * GameBackBuffer->BytesPerPixel;
    }
  }

  bool32 DrawFooter = false;

  if (RedrawLevel) {
    // Draw the whole level in one go
    for (int Row = 0; Row < Level.Height; ++Row) {
      for (int Col = 0; Col < Level.Width; ++Col) {
        DrawTile(Col, Row);
      }
    }
    DrawFooter = true;
  }

  if (!Level.IsDrawn) {
    // Draw the level line by line
    for (int i = 0; i < Level.DrawTilesPerFrame; i++) {
      int Col = Level.TileBeingDrawn % Level.Width;
      int Row = Level.TileBeingDrawn / Level.Width;
      DrawTile(Col, Row);
      Level.TileBeingDrawn++;
      if (Level.TileBeingDrawn >= Level.Width * Level.Height) {
        Level.IsDrawn = true;
        DrawFooter = true;
        break;
      }
    }
    if (!Level.IsDrawn) {
      // if still not drawn
      return;
    }
  }

  if (Level.IsDisappearing) {
    animation *Animation = &Level.Disappearing;
    frame *Frame = &Animation->Frames[Animation->Frame];

    v2i Position = {};
    for (int Row = 0; Row < Level.Height; Row++) {
      for (int Col = 0; Col < Level.Width; Col++) {
        Position.x = Col * kTileWidth;
        Position.y = Row * kTileHeight;
        DrawSprite(Position, kTileWidth, kTileHeight, Frame->XOffset,
                   Frame->YOffset);
      }
    }

    if (Animation->Counter > Frame->Lasting) {
      Animation->Counter = 0;
      Animation->Frame++;
    }
    Animation->Counter += 1;

    if (Animation->Frame >= Animation->FrameCount) {
      Level.IsDisappearing = false;
      LoadLevel(Level.Index);
    }
    return;
  }

  if (DrawFooter) {
    int LevelBottomY = Level.Height * kTileHeight + kTileHeight / 4;
    DrawRectangle(0, LevelBottomY + 2, kTileWidth * Level.Width,
                  kTileHeight / 2, 0x009C659C);
    LevelBottomY += kTileHeight - 4;
    DrawText("score", 0, LevelBottomY);
    DrawText("level", (Level.Width - 8) * kTileWidth, LevelBottomY);

    char LevelString[3] = "00";
    LevelString[2] = 0;
    LevelString[1] = (char)('0' + (Level.Index + 1) % 10);
    LevelString[0] = (char)('0' + ((Level.Index + 1) / 10) % 10);
    DrawText(LevelString, (Level.Width - 2) * kTileWidth, LevelBottomY);
    gUpdateScore = true;
  }

  // Draw score
  if (gUpdateScore) {
    gUpdateScore = false;
    const int MaxScore = 99999999;
    if (gScore > MaxScore) {
      gScore = MaxScore;
    }
    if (gScore < 0) {
      gScore = 0;
    }
    char String[9] = "00000000";
    int i = 7;  // last digit index
    int value = gScore;
    while (i >= 0) {
      String[i] = (char)('0' + value % 10);
      value /= 10;
      i--;
    }
    DrawText(String, 6 * kTileWidth,
             Level.Height * kTileHeight + kTileHeight / 4 + kTileHeight - 4);
  }

  // Update players
  for (int i = 0; i < 2; i++) {
    player *Player = &Level.Players[i];
    if (!Player->IsActive) {
      continue;
    }

    bool32 Turbo = false;
    int Speed = 4;

    // Update player
    player_input *Input = &NewInput->Players[i];

    bool32 PressedUp = Input->Up.EndedDown;
    bool32 PressedDown = Input->Down.EndedDown;
    bool32 PressedLeft = Input->Left.EndedDown;
    bool32 PressedRight = Input->Right.EndedDown;
    bool32 PressedFire = Input->Fire.EndedDown;

    if (Player->IsDead && PressedFire) {
      AddScore(-2150);
      gClock = true;
      Level.IsDisappearing = true;
      return;
    }
    if (!gClock) return;

    ErasePerson(Player);

#ifdef BUILD_INTERNAL
    // Turbo
    if (Input->Turbo.EndedDown) {
      Speed *= 5;
      Turbo = true;
    }
#endif

    if (!Level.HasStarted) {
      for (int button = 0; button < INPUT_BUTTON_COUNT; button++) {
        if (Input->Buttons[button].EndedDown) {
          Level.HasStarted = true;
          // Prevent player from disappearing
          Player->Animation = &Player->Falling;
          break;
        }
      }
    }
    if (!Level.HasStarted) break;

    bool32 IsEnemy = false;
    UpdatePerson(Player, IsEnemy, Speed, PressedUp, PressedDown, PressedLeft,
                 PressedRight, PressedFire, Turbo);
  }

  // Update enemies
  for (int i = 0; i < Level.EnemyCount; i++) {
    enemy *Enemy = &Level.Enemies[i];

    if (!Level.HasStarted) break;

    ErasePerson(Enemy);

    bool32 Animate = false;
    int Speed = 2;
    int Turbo = false;
    int kPathCooldown = 30;

    if (Enemy->IsDead) {
      AddScore(245);

      Enemy->IsDead = false;
      Enemy->IsParalysed = false;
      Enemy->ParalyseCooldown = 0;
      Enemy->ParalyseImmunityCooldown = 0;

      v2i Position = Level.Respawns[randint(Level.RespawnCount)];

      Enemy->TileX = Position.x;
      Enemy->TileY = Position.y;
      Enemy->X = Position.x * kTileWidth + Enemy->Width / 2;
      Enemy->Y = Position.y * kTileHeight + Enemy->Height / 2;
    }

    if (Enemy->IsParalysed && Enemy->ParalyseCooldown <= 0) {
      Enemy->IsParalysed = false;
      Enemy->ParalyseImmunityCooldown = 30;
      Enemy->PathCooldown = 0;  // build a path
    }

    player *Player = Enemy->Pursuing;
    if (Player == NULL || Enemy->PathCooldown <= 0) {
      if (gDebug) {
        // Erase old drawn path
        if (Enemy->PathExists) {
          for (int j = 0; j < Enemy->PathLength; j++) {
            DrawTile(Enemy->Path[j].x, Enemy->Path[j].y);
          }
        }
      }
      // Choose a player to pursue
      if (Level.PlayerCount > 1) {
        player *Player1 = &Level.Players[0];
        player *Player2 = &Level.Players[1];

        if (Abs(Player1->TileX - Enemy->TileX) +
                Abs(Player1->TileY - Enemy->TileY) <
            Abs(Player2->TileX - Enemy->TileX) +
                Abs(Player2->TileY - Enemy->TileY)) {
          Player = Player1;
        } else {
          Player = Player2;
        }
      } else {
        Player = &Level.Players[0];
      }
      Enemy->Pursuing = Player;

      FindPath(Enemy, Player);

      Enemy->PathCooldown = kPathCooldown;
    }

    Enemy->PathCooldown--;

    // If sees the player directly
    bool32 SeesDirectly = false;
    if (Player->Y == Enemy->Y && Abs(Player->TileX - Enemy->TileX) <= 3) {
      SeesDirectly = true;

      // Check for obstacles
      int Step = Player->TileX < Enemy->TileX ? 1 : -1;
      for (int tile = Player->TileX; tile != Enemy->TileX; tile += Step) {
        if (!CanGoThroughTile(tile, Player->TileY)) {
          SeesDirectly = false;
        }
      }
    }
    if (Player->X == Enemy->X && Abs(Player->TileY - Enemy->TileY) <= 3) {
      SeesDirectly = true;

      // Check for obstacles
      int Step = Player->TileY < Enemy->TileY ? 1 : -1;
      for (int tile = Player->TileY; tile != Enemy->TileY; tile += Step) {
        if (!CanGoThroughTile(Player->TileX, tile)) {
          SeesDirectly = false;
        }
      }
    }

    if (SeesDirectly) {
      // Grab him!

      // @copypaste
      int DeltaX = Player->X - Enemy->X;
      int DeltaY = Player->Y - Enemy->Y;

      if (DeltaX > 0) {
        Enemy->DirectionX = RIGHT;
      } else if (DeltaX < 0) {
        Enemy->DirectionX = LEFT;
      } else {
        Enemy->DirectionX = NOWHERE;
      }
      if (DeltaY > 0) {
        Enemy->DirectionY = DOWN;
      } else if (DeltaY < 0) {
        Enemy->DirectionY = UP;
      } else {
        Enemy->DirectionY = NOWHERE;
      }
    }

    if (Enemy->PathExists && Enemy->BumpCooldown <= 0) {
      v2i NextPoint = Enemy->Path[Enemy->PathPointIndex];
      int TargetX = NextPoint.x * kTileWidth + kTileWidth / 2;
      int TargetY = NextPoint.y * kTileHeight + kTileHeight / 2;

      // If reached the point
      if (Abs(TargetX - Enemy->X) <= 4 && Abs(TargetY - Enemy->Y) <= 4) {
        Enemy->PathPointIndex++;
        NextPoint = Enemy->Path[Enemy->PathPointIndex];
        TargetX = NextPoint.x * kTileWidth + kTileWidth / 2;
        TargetY = NextPoint.y * kTileHeight + kTileHeight / 2;
      }

      // If reached the end of the path
      if (Enemy->PathPointIndex >= Enemy->PathLength - 1) {
        Enemy->PathExists = false;
      }

      if (!SeesDirectly) {
        int DeltaX = TargetX - Enemy->X;
        int DeltaY = TargetY - Enemy->Y;

        if (DeltaX > 0) {
          Enemy->DirectionX = RIGHT;
        } else if (DeltaX < 0) {
          Enemy->DirectionX = LEFT;
        } else {
          Enemy->DirectionX = NOWHERE;
        }
        if (DeltaY > 0) {
          Enemy->DirectionY = DOWN;
        } else if (DeltaY < 0) {
          Enemy->DirectionY = UP;
        } else {
          Enemy->DirectionY = NOWHERE;
        }

        // Get off ladders easily if needed
        if (CheckTile(Enemy->TileX, Enemy->TileY) == LVL_LADDER &&
            Abs(DeltaY) <= 1 && Abs(DeltaX) > 2) {
          Enemy->DirectionY = NOWHERE;
        }
      }

      // Don't fall from ropes when not needed
      if (CheckTile(Enemy->TileX, Enemy->TileY) == LVL_ROPE &&
          Abs(Player->Y - Enemy->Y) <= 2) {
        Enemy->DirectionY = UP;
      }
    }

    if (Enemy->BumpCooldown > 0) {
      Enemy->BumpCooldown--;
    }

    if (Enemy->ParalyseCooldown > 0) {
      Enemy->ParalyseCooldown--;
      Enemy->DirectionX = NOWHERE;
      Enemy->DirectionY = NOWHERE;
    }

    if (Enemy->ParalyseImmunityCooldown > 0) {
      Enemy->ParalyseImmunityCooldown--;
    }

    if (!Enemy->IsParalysed && Enemy->ParalyseImmunityCooldown <= 0 &&
        CheckTile(Enemy->TileX, Enemy->TileY) == LVL_BLANK_TMP) {
      Enemy->IsParalysed = true;
      AddScore(185);
      Enemy->ParalyseCooldown = 4 * 60;
    }

    // Maybe drop treasure
    if (Enemy->CarriesTreasure >= 0 && (Enemy->X % kTileWidth == 0) &&
        CheckTile(Enemy->TileX, Enemy->TileY) != LVL_LADDER &&
        Enemy->ParalyseCooldown <= 0) {
      if (randint(100) < 8) {
        bool32 AnotherTreasureOccupiesThisTile = false;
        for (int j = 0; j < Level.TreasureCount; j++) {
          if (Enemy->CarriesTreasure == j) continue;
          treasure *AnotherTreasure = &Level.Treasures[j];
          if (AnotherTreasure->IsCollected) continue;
          if (AnotherTreasure->TileX == Enemy->TileX &&
              AnotherTreasure->TileY == Enemy->TileY) {
            AnotherTreasureOccupiesThisTile = true;
          }
        }
        if (!AnotherTreasureOccupiesThisTile) {
          treasure *Treasure = &Level.Treasures[Enemy->CarriesTreasure];
          Treasure->IsCollected = false;
          Treasure->X = Enemy->X;
          Treasure->Y = Enemy->TileY * kTileHeight;
          Treasure->TileX = Enemy->TileX;
          Treasure->TileY = Enemy->TileY;
          Enemy->CarriesTreasure = -1;
        }
      }
    }

    // If in a pit, drop the treasure
    if (Enemy->CarriesTreasure >= 0 && Enemy->IsParalysed) {
      treasure *Treasure = &Level.Treasures[Enemy->CarriesTreasure];
      Treasure->IsCollected = false;
      Treasure->TileX = Enemy->TileX;
      Treasure->TileY = Enemy->TileY - 1;
      Treasure->X = Treasure->TileX * kTileWidth;
      Treasure->Y = Treasure->TileY * kTileHeight;
      Enemy->CarriesTreasure = -1;
    }

    bool32 PressedUp = Enemy->DirectionY == UP;
    bool32 PressedDown = Enemy->DirectionY == DOWN;
    bool32 PressedLeft = Enemy->DirectionX == LEFT;
    bool32 PressedRight = Enemy->DirectionX == RIGHT;
    bool32 PressedFire = false;

    bool32 IsEnemy = true;
    UpdatePerson(Enemy, IsEnemy, Speed, PressedUp, PressedDown, PressedLeft,
                 PressedRight, PressedFire, Turbo);
  }

  // Process and draw bricks
  for (int i = 0; i < kCrushedBrickCount; i++) {
    crushed_brick *Brick = &Level.CrushedBricks[i];
    if (!Brick->IsUsed) {
      continue;
    }

    if (Brick->State == Brick->CRUSHING) {
      // @copypaste
      animation *Animation = &Brick->Breaking;
      frame *Frame = &Animation->Frames[Animation->Frame];

      v2i Position = {};
      Position.x = Brick->TileX * kTileWidth;
      Position.y = (Brick->TileY - 1) * kTileHeight;
      DrawSprite(Position, kTileWidth, kTileHeight * 2, Frame->XOffset,
                 Frame->YOffset);

      if (Animation->Counter > Frame->Lasting) {
        Animation->Counter = 0;
        Animation->Frame++;
      }
      if (Animation->Frame >= Animation->FrameCount) {
        DrawTile(Brick->TileX, Brick->TileY - 1);
        DrawTile(Brick->TileX, Brick->TileY);
        Brick->State = Brick->WAITING;
        continue;
      }
      Animation->Counter += 1;
    }

    if (Brick->State == Brick->WAITING) {
      Brick->Countdown--;
      if (Brick->Countdown <= 0) {
        Brick->State = Brick->RESTORING;
      }
    }

    if (Brick->State == Brick->RESTORING) {
      // @copypaste
      animation *Animation = &Brick->Restoring;
      frame *Frame = &Animation->Frames[Animation->Frame];

      v2i Position = {};
      Position.x = Brick->TileX * kTileWidth;
      Position.y = Brick->TileY * kTileHeight;
      DrawSprite(Position, kTileWidth, kTileHeight, Frame->XOffset,
                 Frame->YOffset);

      if (Animation->Counter > Frame->Lasting) {
        Animation->Counter = 0;
        Animation->Frame++;
      }

      if (Animation->Frame >= Animation->FrameCount) {
        SetTile(Brick->TileX, Brick->TileY, LVL_BRICK);
        DrawTile(Brick->TileX, Brick->TileY - 1);
        DrawTile(Brick->TileX, Brick->TileY);

        rect TileRect = GetTileRect(Brick->TileX, Brick->TileY);

        // See if we've killed someone
        for (int e = 0; e < Level.EnemyCount; e++) {
          enemy *Enemy = &Level.Enemies[e];

          rect EnemyRect = GetBoundingRect(Enemy);
          if (RectsCollide(EnemyRect, TileRect)) {
            Enemy->IsDead = true;
            break;
          }
        }

        for (int p = 0; p < Level.PlayerCount; p++) {
          player *Player = &Level.Players[p];
          rect PlayerRect = GetBoundingRect(Player);
          if (RectsCollide(PlayerRect, TileRect)) {
            Player->IsDead = true;
            gClock = false;
          }
        }

        Brick->IsUsed = false;
        continue;
      }
      Animation->Counter += 1;
    }
  }

  // Update and draw treasures
  for (int i = 0; i < Level.TreasureCount; i++) {
    treasure *Treasure = &Level.Treasures[i];

    if (Treasure->IsCollected) continue;

    const int kCollectMargin = 10;

    // Check if it's being collected
    for (int j = 0; j < Level.EnemyCount; j++) {
      enemy *Enemy = &Level.Enemies[j];

      if (Enemy->CarriesTreasure >= 0) continue;

      if (Abs(Enemy->X - (Treasure->X + kTileWidth / 2)) < kCollectMargin &&
          Abs(Enemy->Y - (Treasure->Y + kTileHeight / 2)) < kCollectMargin) {
        if (randint(100) < 5) {
          Treasure->IsCollected = true;
          Enemy->CarriesTreasure = i;
        }
      }
    }
    for (int j = 0; j < Level.PlayerCount; j++) {
      player *Player = &Level.Players[j];
      if (Abs(Player->X - (Treasure->X + kTileWidth / 2)) < kCollectMargin &&
          Abs(Player->Y - (Treasure->Y + kTileHeight / 2)) < kCollectMargin) {
        Treasure->IsCollected = true;
        Level.TreasuresCollected++;
        AddScore(305);
        if (Level.TreasuresCollected == Level.TreasureCount) {
          // All treasures collected
          Level.AllTreasuresCollected = true;
          for (int Row = 0; Row < Level.Height; Row++) {
            for (int Col = 0; Col < Level.Width; Col++) {
              if (CheckTile(Col, Row) == LVL_WIN_LADDER) {
                SetTile(Col, Row, LVL_LADDER);
                DrawTile(Col, Row);
              }
            }
          }
        }
      }
    }

    int Speed = 4;  // divides kTileHeight, so no problems

    int BottomTile = CheckTile(Treasure->TileX,
                               (Treasure->Y + Treasure->Height) / kTileHeight);

    // Treasure doesn't fall through the following tiles
    bool32 AcceptableMove =
        BottomTile != LVL_BRICK && BottomTile != LVL_BRICK_HARD &&
        BottomTile != LVL_LADDER && BottomTile != LVL_INVALID &&
        BottomTile != LVL_BLANK_TMP && BottomTile != LVL_BRICK_FAKE &&
        BottomTile != LVL_ROPE && BottomTile != LVL_TREASURE_HOLDER;

    // Check if there's another treasure below - O(n2)
    bool32 TreasureBelow = false;
    for (int j = 0; j < Level.TreasureCount; j++) {
      if (i == j) continue;
      treasure *AnotherTreasure = &Level.Treasures[j];
      if (AnotherTreasure->IsCollected) continue;
      if (Treasure->TileX != AnotherTreasure->TileX) continue;
      int Bottom = Treasure->Y + Treasure->Height;
      if (Bottom + Speed > AnotherTreasure->Y &&
          Treasure->Y < AnotherTreasure->Y) {
        TreasureBelow = true;
        break;
      }
    }

    if (AcceptableMove && !TreasureBelow) {
      Treasure->Y += Speed;
      Treasure->TileY = Treasure->Y / kTileHeight;
      DrawTile(Treasure->TileX, Treasure->TileY);
      DrawTile(Treasure->TileX, Treasure->TileY + 1);
    }
  }

  // Draw all treasures so they don't blink
  for (int i = 0; i < Level.TreasureCount; i++) {
    treasure *Treasure = &Level.Treasures[i];
    if (Treasure->IsCollected) continue;
    DrawSprite(Treasure->Position, kTileWidth, kTileHeight, 96, 96);
  }

  if (gDebug) {
    for (int i = 0; i < Level.EnemyCount; i++) {
      enemy *Enemy = &Level.Enemies[i];

      if (Enemy->PathExists) {
        for (int j = 0; j < Enemy->PathLength - 1; j++) {
          DrawRectangle(Enemy->Path[j].x * kTileWidth,
                        Enemy->Path[j].y * kTileHeight, kTileWidth, kTileHeight,
                        0x00333333);
        }
      }
    }
  }

  // Draw players
  for (int p = 0; p < 2; p++) {
    // We're drawing them in a separate loop so they don't
    // overdraw each other

    player *Player = &Level.Players[p];
    if (!Player->IsActive) {
      continue;
    }

    animation *Animation = Player->Animation;
    frame *Frame = &Animation->Frames[Animation->Frame];

    if (Player->Animate) {
      if (Animation->Counter >= Frame->Lasting) {
        Animation->Counter = 0;
        Animation->Frame = (Animation->Frame + 1) % Animation->FrameCount;
      }
      Animation->Counter += 1;
    }

    // Debug
    if (gDebug) {
      DrawRectangle(Player->TileX * kTileWidth, Player->TileY * kTileWidth,
                    kTileWidth, kTileHeight, 0x00333333);
    }

    Frame = &Animation->Frames[Animation->Frame];
    v2i Position = {Player->X - Player->Width / 2,
                    Player->Y - Player->Height / 2};
    DrawSprite(Position, Player->Width, Player->Height, Frame->XOffset,
               Frame->YOffset);
  }

  // Draw enemies
  for (int i = 0; i < Level.EnemyCount; i++) {
    enemy *Enemy = &Level.Enemies[i];

    animation *Animation = Enemy->Animation;
    frame *Frame = &Animation->Frames[Animation->Frame];

    if (Enemy->Animate) {
      if (Animation->Counter >= Frame->Lasting) {
        Animation->Counter = 0;
        Animation->Frame = (Animation->Frame + 1) % Animation->FrameCount;
      }
      Animation->Counter += 1;
    }

    Frame = &Animation->Frames[Animation->Frame];
    v2i Position = {Enemy->X - Enemy->Width / 2, Enemy->Y - Enemy->Height / 2};
    DrawSprite(Position, Enemy->Width, Enemy->Height, Frame->XOffset,
               Frame->YOffset);

    if (gDebug) {
      if (Enemy->PathExists) {
        v2i Pos = Enemy->Path[Enemy->PathPointIndex];
        Pos.x = Pos.x * kTileWidth + kTileWidth / 2 - 2;
        Pos.y = Pos.y * kTileHeight + kTileHeight / 2 - 2;
        DrawRectangle(Pos, 4, 4, 0x00FF0000);
      }
    }
  }
}
