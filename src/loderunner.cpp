#include "loderunner.h"

// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global player gPlayer;
global level *Level;
global sprites *Sprites;

global int kTileWidth = 32;
global int kTileHeight = 32;

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

internal void DEBUGDrawImage(v2 Position, bmp_file *Image) {
  int Width = Image->Width;
  int Height = Image->Height;
  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;
  u32 *SrcRow =
      (u32 *)Image->Bitmap + Height * Width - Width;  // bottom row first

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
    SrcRow -= Width;
  }
}

void DrawPlayer(player *Player) {
  v2 Position;
  Position.x = Player->Position.x - Player->Width / 2;
  Position.y = Player->Position.y - Player->Height / 2;
  DEBUGDrawImage(Position, Player->Sprite);
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

void DrawTile(int Col, int Row) {
  if (Col < 0 || Row < 0 || Col >= Level->Width || Row >= Level->Height) {
    // Don't draw outside level boundaries
    return;
  }
  v2 Position = {};
  Position.x = (r32)(Col * kTileWidth);
  Position.y = (r32)(Row * kTileHeight);
  int Value = Level->Contents[Row][Col];
  if (Value == LVL_BRICK) {
    DEBUGDrawImage(Position, Sprites->Brick);
  } else if (Value == LVL_LADDER) {
    DEBUGDrawImage(Position, Sprites->Ladder);
  } else if (Value == LVL_ROPE) {
    DEBUGDrawImage(Position, Sprites->Rope);
  } else if (Value == LVL_TREASURE) {
    DEBUGDrawImage(Position, Sprites->Treasure);
  } else if (Value == LVL_BLANK || Value == LVL_PLAYER || Value == LVL_ENEMY ||
             Value == LVL_WIN_LADDER) {
    DEBUGDrawRectangle(Position, kTileWidth, kTileHeight, 0x000A0D0B);
  }
}

bool32 AcceptableMove(player *Player) {
  // Tells whether the player can be legitimately
  // placed in its position

  // @copypaste
  int PlayerLeft = (int)Player->X - Player->Width / 2;
  int PlayerRight = (int)Player->X + Player->Width / 2;
  int PlayerTop = (int)Player->Y - Player->Height / 2;
  int PlayerBottom = (int)Player->Y + Player->Height / 2;

  // Don't go away from the level
  if (PlayerLeft < 0 || PlayerTop < 0 ||
      PlayerRight > Level->Width * kTileWidth ||
      PlayerBottom > Level->Height * kTileHeight)
    return false;

  int TileX = ((int)Player->X + Player->Width / 2) / kTileWidth;
  int TileY = ((int)Player->Y + Player->Width / 2) / kTileHeight;
  int StartCol = (TileX <= 0) ? 0 : TileX - 1;
  int EndCol = (TileX >= Level->Width - 1) ? TileX : TileX + 1;
  int StartRow = (TileY <= 0) ? 0 : TileY - 1;
  int EndRow = (TileY >= Level->Height - 1) ? TileY : TileY + 1;

  for (int Row = StartRow; Row <= EndRow; Row++) {
    for (int Col = StartCol; Col <= EndCol; Col++) {
      int Tile = Level->Contents[Row][Col];
      if (Tile != LVL_BRICK && Tile != LVL_BRICK_HARD) continue;

      // Collision check
      int TileLeft = Col * kTileWidth;
      int TileRight = (Col + 1) * kTileWidth;
      int TileTop = Row * kTileHeight;
      int TileBottom = (Row + 1) * kTileHeight;
      if (PlayerRight > TileLeft && PlayerLeft < TileRight &&
          PlayerBottom > TileTop && PlayerTop < TileBottom)
        return false;
    }
  }
  return true;
}

bool32 PlayerTouches(player *Player, int TileType) {
  // @copypaste
  int const Shrink = 40;
  int PlayerLeft = (int)Player->X - (Player->Width - Shrink) / 2;
  int PlayerRight = (int)Player->X + (Player->Width - Shrink) / 2;
  int PlayerTop = (int)Player->Y - Player->Height / 2;
  int PlayerBottom = (int)Player->Y + Player->Height / 2;

  int ColLeft = PlayerLeft / kTileWidth;
  int ColRight = PlayerRight / kTileWidth;
  int RowTop = PlayerTop / kTileHeight;
  int RowBottom = PlayerBottom / kTileHeight;

  for (int Row = RowTop; Row <= RowBottom; Row++) {
    for (int Col = ColLeft; Col <= ColRight; Col++) {
      if (Level->Contents[Row][Col] == TileType) return true;
    }
  }

  return false;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  // Update global vars
  GameBackBuffer = Buffer;
  GameMemory = Memory;
  player *Player = &gPlayer;

  // Load sprites
  if (!Sprites) {
    kTileWidth = 32;
    kTileHeight = 32;

    Sprites = (sprites *)GameMemoryAlloc(sizeof(sprites));
    Sprites->Brick = LoadSprite("img/brick.bmp");
    Sprites->BrickHard = LoadSprite("img/brick-hard.bmp");
    Sprites->Ladder = LoadSprite("img/ladder.bmp");
    Sprites->Rope = LoadSprite("img/rope.bmp");
    Sprites->Treasure = LoadSprite("img/treasure.bmp");
    Sprites->HeroLeft = LoadSprite("img/hero-left.bmp");
    Sprites->HeroRight = LoadSprite("img/hero-right.bmp");
  }

  // Init player
  if (!Player->Width) {
    Player->Sprite = Sprites->HeroRight;
    Player->Width = Player->Sprite->Width;
    Player->Height = Player->Sprite->Height;
  }

  // Init level
  if (!Level) {
    Level = (level *)GameMemoryAlloc(sizeof(level));
    // Load level
    {
      char const *Filename = "levels/level_1.txt";
      file_read_result FileReadResult =
          GameMemory->DEBUGPlatformReadEntireFile(Filename);

      char *String = (char *)FileReadResult.Memory;

      // Get level size
      {
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
        }
        Level->Width = MaxWidth;
        Level->Height = Height;
      }

      int Column = 0;
      int Row = 0;
      for (int i = 0; i < FileReadResult.MemorySize; i++) {
        char Symbol = String[i];
        int Value = LVL_BLANK;
        bool32 PlayerSet = false;

        if (Symbol == '|')
          Value = LVL_WIN_LADDER;
        else if (Symbol == 't')
          Value = LVL_TREASURE;
        else if (Symbol == '=')
          Value = LVL_BRICK;
        else if (Symbol == '#')
          Value = LVL_LADDER;
        else if (Symbol == '-')
          Value = LVL_ROPE;
        else if (Symbol == 'e')
          Value = LVL_ENEMY;
        else if (Symbol == 'p' && !PlayerSet) {
          Value = LVL_PLAYER;
          Player->TileX = Column;
          Player->TileY = Row;
          Player->X = (r32)(Player->TileX * kTileWidth + kTileWidth / 2);
          Player->Y = (r32)(Player->TileY * kTileHeight + kTileHeight / 2);
          PlayerSet = true;
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
    DEBUGDrawRectangle({0, 0}, GameBackBuffer->Width, GameBackBuffer->Height,
                       0x000A0D0B);

    for (int Row = 0; Row < Level->Height; ++Row) {
      for (int Col = 0; Col < Level->Width; ++Col) {
        DrawTile(Col, Row);
      }
    }
  }

  // Redraw tiles covered by player
  {
    DrawTile(Player->TileX, Player->TileY);
    int LeftBoundary = Player->TileX * kTileWidth;
    int RightBoundary = (Player->TileX + 1) * kTileWidth;
    int TopBoundary = Player->TileY * kTileHeight;
    int BottomBoundary = (Player->TileY + 1) * kTileHeight;

    // Clear the other tile that might be covered
    bool32 LeftTileCovered = (Player->X < LeftBoundary + Player->Width / 2);
    bool32 RightTileCovered = (Player->X > RightBoundary - Player->Width / 2);
    bool32 TopTileCovered = (Player->Y < TopBoundary + Player->Height / 2);
    bool32 BottomTileCovered =
        (Player->Y > BottomBoundary - Player->Height / 2);

    Assert(!(LeftTileCovered && RightTileCovered));
    Assert(!(TopTileCovered && BottomTileCovered));

    if (LeftTileCovered) {
      DrawTile(Player->TileX - 1, Player->TileY);
    }
    if (RightTileCovered) {
      DrawTile(Player->TileX + 1, Player->TileY);
    }
    if (TopTileCovered) {
      DrawTile(Player->TileX, Player->TileY - 1);
    }
    if (BottomTileCovered) {
      DrawTile(Player->TileX, Player->TileY + 1);
    }
    if (LeftTileCovered && TopTileCovered) {
      DrawTile(Player->TileX - 1, Player->TileY - 1);
    }
    if (LeftTileCovered && BottomTileCovered) {
      DrawTile(Player->TileX - 1, Player->TileY + 1);
    }
    if (RightTileCovered && TopTileCovered) {
      DrawTile(Player->TileX + 1, Player->TileY - 1);
    }
    if (RightTileCovered && BottomTileCovered) {
      DrawTile(Player->TileX + 1, Player->TileY + 1);
    }
  }

  // Update player
  {
    player_input *Input = &NewInput->Player2;

    r32 Speed = 0.2f * NewInput->dtForFrame;

    bool32 Turbo = false;
#ifdef BUILD_INTERNAL
    // Turbo
    if (Input->Turbo.EndedDown) {
      Speed *= 5;
      Turbo = true;
    }
#endif

    bool32 OnLadder = PlayerTouches(Player, LVL_LADDER);

    bool32 LadderBelow = false;
    // {
      int Col = Player->TileX;
      int Row = Player->TileY + 1;
      int PlayerBottom = (int)Player->Y + Player->Height / 2;
        int TileTop = Row * kTileHeight;
      if (Level->Contents[Row][Col] == LVL_LADDER) {

        if (PlayerBottom + 3 >= TileTop)  // +3 to compensate float
          LadderBelow = true;
      }
    // }



    // TODO:
    // - Fix the bug with the ladder (something's wrong with the)
    //   ladder below check.
    // - Don't allow to move while falling



    // Update based on movement keys
    if (Input->Right.EndedDown) {
      r32 Old = Player->X;
      Player->X += Speed;
      if (!AcceptableMove(Player)) {
        Player->X = Old;
      }
      Player->Sprite = Sprites->HeroRight;
    }
    if (Input->Left.EndedDown) {
      r32 Old = Player->X;
      Player->X -= Speed;
      if (!AcceptableMove(Player)) {
        Player->X = Old;
      }
      Player->Sprite = Sprites->HeroLeft;
    }
    if (Input->Up.EndedDown && (OnLadder || Turbo)) {
      r32 Old = Player->Y;
      Player->Y -= Speed;
      if (!AcceptableMove(Player)) {
        Player->Y = Old;
      }
    }
    if (Input->Down.EndedDown && (OnLadder || LadderBelow)) {
      r32 Old = Player->Y;
      Player->Y += Speed;
      if (!AcceptableMove(Player)) {
        Player->Y = Old;
      }
    }

    // Gravity
    {
      r32 Old = Player->Y;
      Player->Y += Speed;
      if (!AcceptableMove(Player) || OnLadder || LadderBelow || Turbo) {
        Player->Y = Old;
      }
    }

    // Update player tile
    Player->TileX = ((int)Player->X + Player->Width / 2) / kTileWidth;
    Player->TileY = ((int)Player->Y + Player->Width / 2) / kTileHeight;
  }

  // Draw
  DrawPlayer(Player);
}
