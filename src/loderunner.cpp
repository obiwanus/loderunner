#include "loderunner.h"

// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global player gPlayers[2];
global enemy *gEnemies;
global treasure *gTreasures;
global level *Level;
global bmp_file *gImage;

global bool32 gDrawDebug = false;

global int kTileWidth = 32;
global int kTileHeight = 32;
global int kHumanWidth = 24;
global int kHumanHeight = 32;

global const int kCrushedBrickCount = 30;
global crushed_brick CrushedBricks[kCrushedBrickCount];
global int NextBrickAvailable = 0;

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

internal void DrawSprite(v2i Position, int Width, int Height, int XOffset,
                         int YOffset) {
  // NOTE: if we need a draw image function, it's easily derived from this one

  // If we ever need another image, we'll need a new func
  bmp_file *Image = gImage;

  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  int SrcPitch = gImage->Width;

  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;
  u32 *BottomLeftCorner =
      (u32 *)Image->Bitmap + Image->Width * (Image->Height - 1);
  u32 *SrcRow = BottomLeftCorner - SrcPitch * YOffset + XOffset;

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    int *SrcPixel = (int *)SrcRow;

    for (int pX = X; pX < X + Width; pX++) {
      // TODO: bmp may not be masked
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
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    return LVL_INVALID;
  }
  return Level->Contents[Row][Col];
}

void SetTile(int Col, int Row, tile_type Value) {
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    return;  // Invalid tile
  }
  Level->Contents[Row][Col] = Value;
}

void DrawTile(int Col, int Row) {
  if (Col < 0 || Row < 0 || Col >= Level->Width || Row >= Level->Height) {
    // Don't draw outside level boundaries
    return;
  }
  v2i Position = {};
  Position.x = (Col * kTileWidth);
  Position.y = (Row * kTileHeight);
  int Value = CheckTile(Col, Row);
  if (Value == LVL_BRICK) {
    DrawSprite(Position, kTileWidth, kTileHeight, 160, 96);
  } else if (Value == LVL_BRICK_HARD) {
    DrawSprite(Position, kTileWidth, kTileHeight, 128, 96);
  } else if (Value == LVL_LADDER) {
    DrawSprite(Position, kTileWidth, kTileHeight, 96, 128);
  } else if (Value == LVL_ROPE) {
    DrawSprite(Position, kTileWidth, kTileHeight, 128, 128);
  } else if (Value == LVL_TREASURE) {
    DrawSprite(Position, kTileWidth, kTileHeight, 96, 96);
  } else if (Value == LVL_BLANK || Value == LVL_PLAYER || Value == LVL_ENEMY ||
             Value == LVL_WIN_LADDER) {
    DrawRectangle(Position, kTileWidth, kTileHeight, 0x000A0D0B);
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

internal bool32 CanGoThroughTile(int TileX, int TileY) {
  tile_type Tile = CheckTile(TileX, TileY);
  if (Tile == LVL_BRICK || Tile == LVL_BRICK_HARD || Tile == LVL_INVALID) {
    return false;
  } else {
    return true;
  }
}

bool32 AcceptableMove(entity *Entity) {
  // Tells whether the player can be legitimately
  // placed in its position

  int EntityLeft = (int)Entity->X - Entity->Width / 2;
  int EntityRight = (int)Entity->X + Entity->Width / 2;
  int EntityTop = (int)Entity->Y - Entity->Height / 2;
  int EntityBottom = (int)Entity->Y + Entity->Height / 2;

  // Don't go away from the level
  if (EntityLeft < 0 || EntityTop < 0 ||
      EntityRight > Level->Width * kTileWidth ||
      EntityBottom > Level->Height * kTileHeight)
    return false;

  int TileX = ((int)Entity->X + Entity->Width / 2) / kTileWidth;
  int TileY = ((int)Entity->Y + Entity->Width / 2) / kTileHeight;
  int StartCol = (TileX <= 0) ? 0 : TileX - 1;
  int EndCol = (TileX >= Level->Width - 1) ? TileX : TileX + 1;
  int StartRow = (TileY <= 0) ? 0 : TileY - 1;
  int EndRow = (TileY >= Level->Height - 1) ? TileY : TileY + 1;

  for (int Row = StartRow; Row <= EndRow; Row++) {
    for (int Col = StartCol; Col <= EndCol; Col++) {
      int Tile = CheckTile(Col, Row);
      if (Tile != LVL_BRICK && Tile != LVL_BRICK_HARD) continue;

      // Collision check
      int TileLeft = Col * kTileWidth;
      int TileRight = (Col + 1) * kTileWidth;
      int TileTop = Row * kTileHeight;
      int TileBottom = (Row + 1) * kTileHeight;
      if (EntityRight > TileLeft && EntityLeft < TileRight &&
          EntityBottom > TileTop && EntityTop < TileBottom)
        return false;
    }
  }
  return true;
}

inline void SetWMapPoint(int Col, int Row, water_point Point) {
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    Assert(0);
    return;
  }
  Level->WaterMap[Row][Col] = Point;
}

inline water_point CheckWMapPoint(int Col, int Row) {
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    return WATERMAP_OBSTACLE;
  }
  return Level->WaterMap[Row][Col];
}

inline void SetDMapPoint(int Col, int Row, int X, int Y) {
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    Assert(0);
    return;
  }
  int Value = Y * Level->Width + X;
  Level->DirectionMap[Row][Col] = Value;
}

#define DM_TARGET -1
#define FRONTIER_MAX_SIZE 500

void FindPath(enemy *Enemy, player *Player) {
  Enemy->PathFound = false;

  // NOTE: -1 works with memset, but -2 would not
  memset(Level->DirectionMap, -1, sizeof(Level->DirectionMap));
  memset(Level->WaterMap, 0, sizeof(Level->WaterMap));

  Level->DirectionMap[Player->TileY][Player->TileX] = DM_TARGET;

  // Pre-fill watermap with obstacles
  for (int Row = 0; Row < Level->Height; Row++) {
    for (int Col = 0; Col < Level->Width; Col++) {
      if (!CanGoThroughTile(Col, Row)) {
        SetWMapPoint(Col, Row, WATERMAP_OBSTACLE);
      }
    }
  }
  Level->WaterMap[Player->TileY][Player->TileX] = WATERMAP_WATER;

  int Iteration = 0;
  while (Iteration++ < MAX_PATH_LENGTH) {

    for (int Row = 0; Row < Level->Height; Row++) {
      for (int Col = 0; Col < Level->Width; Col++) {
        if (CheckWMapPoint(Col, Row) != WATERMAP_WATER) continue;

        int X = Col;
        int Y = Row;

        if (X == Enemy->TileX && Y == Enemy->TileY) {
          Enemy->PathFound = true;
          break;
        }

        // Point above
        X = Col;
        Y = Row - 1;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if (CanGoThroughTile(X, Y)) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point below
        X = Col;
        Y = Row + 1;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if (CheckTile(X, Y) == LVL_LADDER) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point on the left
        X = Col - 1;
        Y = Row;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if ((CanGoThroughTile(X, Y) && (!CanGoThroughTile(X, Y + 1) ||
                                          CheckTile(X, Y + 1) == LVL_LADDER)) ||
              CheckTile(X, Y) == LVL_ROPE) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }

        // Point on the right
        X = Col + 1;
        Y = Row;
        if (CheckWMapPoint(X, Y) == WATERMAP_NOT_VISITED) {
          if ((CanGoThroughTile(X, Y) && (!CanGoThroughTile(X, Y + 1) ||
                                          CheckTile(X, Y + 1) == LVL_LADDER)) ||
              CheckTile(X, Y) == LVL_ROPE) {
            SetWMapPoint(X, Y, WATERMAP_WATER);
            SetDMapPoint(X, Y, Col, Row);
          }
        }
      }
    }

    if (Enemy->PathFound) break;
  }

  // Build path if found
  if (Enemy->PathFound) {
    int X = Enemy->TileX;
    int Y = Enemy->TileY;
    for (int i = 0; i < MAX_PATH_LENGTH; i++) {
      int NextStep = Level->DirectionMap[Y][X];
      X = NextStep % Level->Width;
      Y = NextStep / Level->Width;
      Enemy->Path[i].x = X;
      Enemy->Path[i].y = Y;
      if (X == Player->TileX && Y == Player->TileY) {
        Enemy->PathLength = i + 1;
        break;
      }
    }
  }
}

void UpdatePerson(person *Person, int Speed, bool32 PressedUp,
                  bool32 PressedDown, bool32 PressedLeft, bool32 PressedRight,
                  bool32 PressedFire, bool32 Turbo) {
  bool32 Animate = false;

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
  }

  bool32 LadderBelow = false;
  {
    int Col = Person->TileX;
    int Row = Person->TileY + 1;
    int PersonBottom = (int)Person->Y + Person->Height / 2;
    int TileTop = Row * kTileHeight;
    if (CheckTile(Col, Row) == LVL_LADDER) {
      if (PersonBottom >= TileTop) LadderBelow = true;
    }
  }

  Person->CanClimb = false;
  if (OnLadder) {
    int LadderCenter = LadderTileX * kTileWidth + kTileWidth / 2;
    Person->CanClimb = Abs(Person->X - LadderCenter) < (Person->Width / 3);
  }

  bool32 OnRope = false;
  if (CheckTile(Person->TileX, Person->TileY) == LVL_ROPE) {
    int RopeY = Person->TileY * kTileHeight;
    int PersonTop = Person->Y - Person->Height / 2;
    OnRope = (PersonTop == RopeY) ||
             (RopeY - PersonTop > 0 && RopeY - PersonTop <= 3);
    // Adjust Person on a rope
    if (OnRope) {
      Person->Y = RopeY + Person->Height / 2;
    }
  }

  // Gravity
  Person->IsFalling = false;
  {
    int Old = Person->Y;
    Person->Y += Speed;
    if (!AcceptableMove(Person) || OnLadder || LadderBelow || Turbo || OnRope) {
      Person->Y = Old;
      Person->IsFalling = false;
    } else {
      Person->IsFalling = true;
      Animate = true;
      Person->Animation = &Person->Falling;
    }
  }

  // Update based on movement keys
  if (PressedRight && (!Person->IsFalling || Turbo)) {
    int Old = Person->X;
    Person->X += Speed;
    if (!AcceptableMove(Person)) {
      Person->X = Old;
      Person->IsStuck = true;
    } else {
      if (!OnRope) {
        Person->Animation = &Person->GoingRight;
      } else {
        Person->Animation = &Person->RopeRight;
      }
      Animate = true;
      Person->Facing = RIGHT;
    }
  }

  if (PressedLeft && (!Person->IsFalling || Turbo)) {
    int Old = Person->X;
    Person->X -= Speed;
    if (!AcceptableMove(Person)) {
      Person->X = Old;
      Person->IsStuck = true;
    } else {
      if (!OnRope) {
        Person->Animation = &Person->GoingLeft;
      } else {
        Person->Animation = &Person->RopeLeft;
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

    bool32 GotFlying = (PlayerTile == LVL_BLANK || PlayerTile == LVL_TREASURE ||
                        PlayerTile == LVL_ROPE) &&
                       (PersonBottom < (Person->TileY + 1) * kTileHeight);

    if (!AcceptableMove(Person) || GotFlying && !Turbo) {
      Person->Y = Old;
      Person->IsStuck = true;
    } else {
      Climbing = true;
      Person->Animation = &Person->Climbing;
      Animate = true;
    }
  }

  Person->CanDescend = false;
  if (Person->CanClimb || LadderBelow || OnRope || Turbo) {
    int Old = Person->Y;
    Person->Y += Speed;
    if (AcceptableMove(Person)) {
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
  } else if (PressedDown) {
    Person->IsStuck = true;
  }

  // Adjust Person on ladder
  if (Person->CanClimb &&
      ((Climbing && PressedUp) || (Descending && PressedDown))) {
    Person->X = Person->TileX * kTileWidth + kTileWidth / 2;
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
      Level->Contents[TileY][TileX] = LVL_BLANK;
      DrawTile(TileX, TileY);
      Person->FireCooldown = 30;

      // Remember that
      crushed_brick *Brick = &CrushedBricks[NextBrickAvailable];
      NextBrickAvailable = (NextBrickAvailable + 1) % kCrushedBrickCount;

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

  // Redraw tiles covered by person
  for (int Row = Person->TileY - 1; Row <= Person->TileY + 1; Row++) {
    for (int Col = Person->TileX - 1; Col <= Person->TileX + 1; Col++) {
      DrawTile(Col, Row);
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
  if (!Level) {
    Level = (level *)GameMemoryAlloc(sizeof(level));
    char const *Filename = "levels/level_1.txt";
    file_read_result FileReadResult =
        GameMemory->DEBUGPlatformReadEntireFile(Filename);

    char *String = (char *)FileReadResult.Memory;

    // Get level info
    int MaxWidth = 0;
    int Width = 0;
    int Height = 1;
    for (int i = 0; i < FileReadResult.MemorySize; i++) {
      char Symbol = String[i];
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
        Level->PlayerCount++;
      }
      if (Symbol == 'e') {
        Level->EnemyCount++;
      }
      if (Symbol == 't') {
        Level->TreasureCount++;
      }
    }
    Level->Width = MaxWidth;
    Level->Height = Height;

    // Allocate memory for enemies and treasures
    gEnemies = (enemy *)GameMemoryAlloc(sizeof(enemy) * Level->EnemyCount);
    gTreasures =
        (treasure *)GameMemoryAlloc(sizeof(treasure) * Level->TreasureCount);

    // Read level data
    {
      int Column = 0;
      int Row = 0;
      int EnemyNum = 0;
      int TreasureNum = 0;

      for (int i = 0; i < FileReadResult.MemorySize; i++) {
        char Symbol = String[i];
        tile_type Value = LVL_BLANK;

        if (Symbol == '|')
          Value = LVL_WIN_LADDER;
        else if (Symbol == 't') {
          Value = LVL_BLANK;
          treasure *Treasure = &gTreasures[TreasureNum];
          TreasureNum++;
          *Treasure = {};  // zero everything
          Treasure->TileX = Column;
          Treasure->TileY = Row;
          Treasure->Width = kTileWidth;
          Treasure->Height = kTileHeight;
          Treasure->X = Treasure->TileX * kTileWidth;
          Treasure->Y = Treasure->TileY * kTileHeight;
        } else if (Symbol == '=')
          Value = LVL_BRICK;
        else if (Symbol == '+')
          Value = LVL_BRICK_HARD;
        else if (Symbol == '#')
          Value = LVL_LADDER;
        else if (Symbol == '-')
          Value = LVL_ROPE;
        else if (Symbol == 'e') {
          Value = LVL_BLANK;
          enemy *Enemy = &gEnemies[EnemyNum];
          EnemyNum++;
          *Enemy = {};  // zero everything
          Enemy->TileX = Column;
          Enemy->TileY = Row;
          Enemy->X = Enemy->TileX * kTileWidth + kTileWidth / 2;
          Enemy->Y = Enemy->TileY * kTileHeight + kTileHeight / 2;
        } else if (Symbol == 'p') {
          Value = LVL_BLANK;
          player *Player = &gPlayers[0];
          if (Player->IsActive) {
            Player = &gPlayers[1];
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
          Assert(Column < Level->Width);
          Assert(Row < Level->Height);
          Level->Contents[Row][Column] = Value;
          ++Column;
        }
      }
    }

    // Draw level
    DrawRectangle({0, 0}, GameBackBuffer->Width, GameBackBuffer->Height,
                  0x000A0D0B);

    for (int Row = 0; Row < Level->Height; ++Row) {
      for (int Col = 0; Col < Level->Width; ++Col) {
        DrawTile(Col, Row);
      }
    }
  }

  // Init players
  for (int i = 0; i < 2; i++) {
    player *Player = &gPlayers[i];
    if (Player->IsInitialized) {
      continue;
    }

    Player->IsInitialized = true;
    Player->Width = kHumanWidth;
    Player->Height = kHumanHeight;
    Player->Animation = &Player->Falling;
    Player->Facing = RIGHT;

    // Init animations
    frame *Frames = NULL;
    animation *Animation = NULL;

    // Falling
    Animation = &Player->Falling;
    Animation->FrameCount = 1;
    Animation->Frames[0] = {72, 0, 0};

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
  for (int i = 0; i < Level->EnemyCount; i++) {
    enemy *Enemy = &gEnemies[i];
    if (Enemy->IsInitialized) {
      continue;
    }

    Enemy->IsInitialized = true;

    Enemy->Width = kHumanWidth;
    Enemy->Height = kHumanHeight;
    Enemy->Animation = &Enemy->Falling;

    // Init animations
    frame *Frames = NULL;
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

  // Update players
  for (int i = 0; i < 2; i++) {
    player *Player = &gPlayers[i];
    if (!Player->IsActive) {
      continue;
    }

    bool32 Turbo = false;
    int Speed = 4;

    // Update player
    player_input *Input = &NewInput->Players[i];

    if (Input->Debug.EndedDown) {
      gDrawDebug = !gDrawDebug;
    }

    bool32 PressedUp = Input->Up.EndedDown;
    bool32 PressedDown = Input->Down.EndedDown;
    bool32 PressedLeft = Input->Left.EndedDown;
    bool32 PressedRight = Input->Right.EndedDown;
    bool32 PressedFire = Input->Fire.EndedDown;

#ifdef BUILD_INTERNAL
    // Turbo
    if (Input->Turbo.EndedDown) {
      Speed *= 5;
      Turbo = true;
    }
#endif

    UpdatePerson(Player, Speed, PressedUp, PressedDown, PressedLeft,
                 PressedRight, PressedFire, Turbo);
  }

  // Update enemies
  for (int i = 0; i < Level->EnemyCount; i++) {
    enemy *Enemy = &gEnemies[i];

    bool32 Animate = false;
    int Speed = 2;
    int Turbo = false;
    int kPathCooldown = 60;

    // @debug
    if (Enemy->PathFound) {
      for (int j = 0; j < Enemy->PathLength; j++) {
        DrawTile(Enemy->Path[j].x, Enemy->Path[j].y);
      }
    }

    player *Player = Enemy->Pursuing;
    // if (Player == NULL || Enemy->PathCooldown <= 0) {
      // Choose a player to pursue
      if (Level->PlayerCount > 1) {
        player *Player1 = &gPlayers[0];
        player *Player2 = &gPlayers[1];

        if (Abs(Player1->TileX - Enemy->TileX) +
                Abs(Player1->TileY - Enemy->TileY) <
            Abs(Player2->TileX - Enemy->TileX) +
                Abs(Player2->TileY - Enemy->TileY)) {
          Player = Player1;
        } else {
          Player = Player2;
        }
      } else {
        Player = &gPlayers[0];
      }
      Enemy->Pursuing = Player;

      FindPath(Enemy, Player);
      Enemy->PathCooldown = kPathCooldown;
    // }

    // Enemy->PathCooldown--;

    // If going nowhere
    if (Enemy->Direction == NOWHERE) {
      if (Player->X < Enemy->X) {
        Enemy->Direction = LEFT;
      } else {
        Enemy->Direction = RIGHT;
      }
    }

    // See if stuck
    if (!Enemy->IsStuck) {
      Enemy->IsStuck = (Enemy->Direction == UP && !Enemy->CanClimb) ||
                       (Enemy->Direction == DOWN && !Enemy->CanDescend);
    }

    // If going horizontally
    if (Enemy->Direction == LEFT || Enemy->Direction == RIGHT) {
      if (Enemy->IsStuck) {
        // If stuck, try the opposite direction
        if (Enemy->Direction == LEFT) {
          Enemy->Direction = RIGHT;
        } else {
          Enemy->Direction = LEFT;
        }
      } else if (Enemy->CanClimb && Player->Y < Enemy->Y) {
        Enemy->Direction = UP;
      } else if (Enemy->CanDescend && Player->Y > Enemy->Y) {
        Enemy->Direction = DOWN;
      }
    }

    // If going vertically
    if (Enemy->Direction == UP || Enemy->Direction == DOWN) {
      if (Enemy->IsStuck) {
        if (Player->X < Enemy->X) {
          Enemy->Direction = LEFT;
        } else {
          Enemy->Direction = RIGHT;
        }
      }
    }

    // If fell and nowhere to go
    if (!Enemy->CanClimb && !Enemy->CanDescend &&
        !CanGoThroughTile(Enemy->TileX - 1, Enemy->TileY) &&
        !CanGoThroughTile(Enemy->TileX + 1, Enemy->TileY)) {
      // Stand and wait
      Enemy->Direction = NOWHERE;
    }

    Enemy->IsStuck = false;

    bool32 PressedUp = Enemy->Direction == UP;
    bool32 PressedDown = Enemy->Direction == DOWN;
    bool32 PressedLeft = Enemy->Direction == LEFT;
    bool32 PressedRight = Enemy->Direction == RIGHT;
    bool32 PressedFire = false;

    UpdatePerson(Enemy, Speed, PressedUp, PressedDown, PressedLeft,
                 PressedRight, PressedFire, Turbo);
  }

  // Process and draw bricks
  for (int i = 0; i < kCrushedBrickCount; i++) {
    crushed_brick *Brick = &CrushedBricks[i];
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
        Brick->IsUsed = false;
        continue;
      }
      Animation->Counter += 1;
    }
  }

  // Update and draw treasures
  for (int i = 0; i < Level->TreasureCount; i++) {
    treasure *Treasure = &gTreasures[i];
    int Speed = 4;  // divides kTileHeight, so no problems

    int BottomTile = CheckTile(Treasure->TileX,
                               (Treasure->Y + Treasure->Height) / kTileHeight);

    bool32 AcceptableMove =
        BottomTile != LVL_BRICK && BottomTile != LVL_BRICK_HARD &&
        BottomTile != LVL_LADDER && BottomTile != LVL_INVALID;

    // Check if there's another treasure below - O(n2)
    bool32 TreasureBelow = false;
    for (int j = 0; j < Level->TreasureCount; j++) {
      if (i == j) continue;
      treasure *AnotherTreasure = &gTreasures[j];
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
  for (int i = 0; i < Level->TreasureCount; i++) {
    DrawSprite(gTreasures[i].Position, kTileWidth, kTileHeight, 96, 96);
  }

  // Draw players
  for (int p = 0; p < 2; p++) {
    // We're drawing them in a separate loop so they don't
    // overdraw each other

    player *Player = &gPlayers[p];
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
    if (gDrawDebug) {
      v2i TilePosition = {};
      TilePosition.x = Player->TileX * kTileWidth;
      TilePosition.y = Player->TileY * kTileWidth;
      DrawRectangle(TilePosition, kTileWidth, kTileHeight, 0x00333333);
    }

    Frame = &Animation->Frames[Animation->Frame];
    v2i Position = {Player->X - Player->Width / 2,
                    Player->Y - Player->Height / 2};
    DrawSprite(Position, Player->Width, Player->Height, Frame->XOffset,
               Frame->YOffset);
  }

  // Draw enemies
  for (int i = 0; i < Level->EnemyCount; i++) {
    enemy *Enemy = &gEnemies[i];

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

    // Draw path
    if (Enemy->PathFound) {
      for (int j = 0; j < Enemy->PathLength - 1; j++) {
        v2i PointPosition;
        PointPosition.x = Enemy->Path[j].x * kTileWidth;
        PointPosition.y = Enemy->Path[j].y * kTileHeight;
        DrawRectangle(PointPosition, kTileWidth, kTileHeight, 0x00333333);
      }
    }
  }
}
