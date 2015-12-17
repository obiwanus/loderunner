#include "loderunner.h"

// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global player gPlayer;
global level *Level;
global sprites *Sprites;

global bool32 gDrawDebug = false;

global int kTileWidth = 32;
global int kTileHeight = 32;
global int kHumanWidth = 24;
global int kHumanHeight = 32;

global const int kCrushedBrickCount = 30;
global crushed_brick CrushedBricks[kCrushedBrickCount];
global int NextBrickAvailable = 0;

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

internal void DrawSprite(v2 Position, sprite Sprite) {
  // @copypaste - possibly can me merged with DEBUGDrawImage

  int Width = Sprite.Width;
  int Height = Sprite.Height;
  int X = (int)Position.x;
  int Y = (int)Position.y;

  int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
  int SrcPitch = Sprite.Image->Width;

  u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y +
            X * GameBackBuffer->BytesPerPixel;
  u32 *BottomLeftCorner = (u32 *)Sprite.Image->Bitmap +
                          Sprite.Image->Width * (Sprite.Image->Height - 1);
  u32 *SrcRow = BottomLeftCorner - SrcPitch * Sprite.YOffset + Sprite.XOffset;

  for (int pY = Y; pY < Y + Height; pY++) {
    int *Pixel = (int *)Row;
    int *SrcPixel = (int *)SrcRow;

    for (int pX = X; pX < X + Width; pX++) {
      // TODO: bmp may not be masked
      u8 Red = UnmaskColor(*SrcPixel, Sprite.Image->RedMask);
      u8 Green = UnmaskColor(*SrcPixel, Sprite.Image->GreenMask);
      u8 Blue = UnmaskColor(*SrcPixel, Sprite.Image->BlueMask);
      u8 Alpha = UnmaskColor(*SrcPixel, Sprite.Image->AlphaMask);

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

bool32 CheckTile(int Row, int Col) {
  if (Row < 0 || Row >= Level->Height || Col < 0 || Col >= Level->Width) {
    return -1;  // Invalid tile
  }
  return Level->Contents[Row][Col];
}

void DrawTile(int Col, int Row) {
  if (Col < 0 || Row < 0 || Col >= Level->Width || Row >= Level->Height) {
    // Don't draw outside level boundaries
    return;
  }
  v2 Position = {};
  Position.x = (r32)(Col * kTileWidth);
  Position.y = (r32)(Row * kTileHeight);
  int Value = CheckTile(Row, Col);
  if (Value == LVL_BRICK) {
    DEBUGDrawImage(Position, Sprites->Brick);
  } else if (Value == LVL_LADDER) {
    DEBUGDrawImage(Position, Sprites->Ladder);
  } else if (Value == LVL_ROPE) {
    DEBUGDrawImage(Position, Sprites->Rope);
  } else if (Value == LVL_TREASURE) {
    DEBUGDrawImage(Position, Sprites->Treasure);
  } else if (Value == LVL_BLANK || LVL_BLANK_TMP || Value == LVL_PLAYER ||
             Value == LVL_ENEMY || Value == LVL_WIN_LADDER) {
    DEBUGDrawRectangle(Position, kTileWidth, kTileHeight, 0x000A0D0B);
  }
}

internal void DrawPlayer(player *Player) {
  // Redraw tiles covered by player
  {
    // Redraw in a dumb way
    for (int Row = Player->TileY - 1; Row <= Player->TileY + 1; Row++) {
      for (int Col = Player->TileX - 1; Col <= Player->TileX + 1; Col++) {
        DrawTile(Col, Row);
      }
    }
    // DrawTile(Player->TileX, Player->TileY);
    // int LeftBoundary = Player->TileX * kTileWidth;
    // int RightBoundary = (Player->TileX + 1) * kTileWidth;
    // int TopBoundary = Player->TileY * kTileHeight;
    // int BottomBoundary = (Player->TileY + 1) * kTileHeight;

    // // Clear the other tile that might be covered
    // bool32 LeftTileCovered = (Player->X < LeftBoundary + Player->Width / 2);
    // bool32 RightTileCovered = (Player->X > RightBoundary - Player->Width /
    // 2);
    // bool32 TopTileCovered = (Player->Y < TopBoundary + Player->Height / 2);
    // bool32 BottomTileCovered =
    //     (Player->Y > BottomBoundary - Player->Height / 2);

    // Assert(!(LeftTileCovered && RightTileCovered));
    // Assert(!(TopTileCovered && BottomTileCovered));

    // if (LeftTileCovered) {
    //   DrawTile(Player->TileX - 1, Player->TileY);
    // }
    // if (RightTileCovered) {
    //   DrawTile(Player->TileX + 1, Player->TileY);
    // }
    // if (TopTileCovered) {
    //   DrawTile(Player->TileX, Player->TileY - 1);
    // }
    // if (BottomTileCovered) {
    //   DrawTile(Player->TileX, Player->TileY + 1);
    // }
    // if (LeftTileCovered && TopTileCovered) {
    //   DrawTile(Player->TileX - 1, Player->TileY - 1);
    // }
    // if (LeftTileCovered && BottomTileCovered) {
    //   DrawTile(Player->TileX - 1, Player->TileY + 1);
    // }
    // if (RightTileCovered && TopTileCovered) {
    //   DrawTile(Player->TileX + 1, Player->TileY - 1);
    // }
    // if (RightTileCovered && BottomTileCovered) {
    //   DrawTile(Player->TileX + 1, Player->TileY + 1);
    // }
  }

  // Debug
  if (gDrawDebug) {
    v2 TilePosition = {};
    TilePosition.x = (r32)Player->TileX * kTileWidth;
    TilePosition.y = (r32)Player->TileY * kTileWidth;
    DEBUGDrawRectangle(TilePosition, kTileWidth, kTileHeight, 0x00333333);
  }

  v2 Position = {};
  Position.x = Player->Position.x - Player->Width / 2;
  Position.y = Player->Position.y - Player->Height / 2;

  DrawSprite(Position, Player->Sprite);
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

bool32 AcceptableMove(player *Player) {
  // Tells whether the player can be legitimately
  // placed in its position

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
      int Tile = CheckTile(Row, Col);
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  // Update global vars
  GameBackBuffer = Buffer;
  GameMemory = Memory;
  player *Player = &gPlayer;
  bool32 Animate = false;
  bool32 Turbo = false;

  // Load sprites
  if (!Sprites) {
    // @rewrite: put all environment in the sprites

    Sprites = (sprites *)GameMemoryAlloc(sizeof(sprites));
    Sprites->Brick = LoadSprite("img/brick.bmp");
    Sprites->BrickHard = LoadSprite("img/brick-hard.bmp");
    Sprites->Ladder = LoadSprite("img/ladder.bmp");
    Sprites->Rope = LoadSprite("img/rope.bmp");
    Sprites->Treasure = LoadSprite("img/treasure.bmp");
    Sprites->Sprites = LoadSprite("img/sprites.bmp");

    // TODO: do something about it
    Sprites->Falling.Image = Sprites->Sprites;
    Sprites->Falling.XOffset = 72;
    Sprites->Falling.YOffset = 0;
    Sprites->Falling.Width = kHumanWidth;
    Sprites->Falling.Height = kHumanHeight;

    Sprites->Breaking.Image = Sprites->Sprites;
    Sprites->Breaking.XOffset = 0;
    Sprites->Breaking.YOffset = 0;
    Sprites->Breaking.Width = kTileWidth;
    Sprites->Breaking.Height = kTileHeight * 2;
  }

  // Init player
  if (!Player->Width) {
    Player->Sprite = Sprites->Falling;
    Player->Width = Player->Sprite.Width;
    Player->Height = Player->Sprite.Height;
    Player->Animation = NULL;
    Player->Facing = RIGHT;

    // Init animations
    {
      frame *Frames = NULL;
      animation *Animation = NULL;

      // NOTE:
      // - Enemies: 6, 3, 3, speed = 2
      // - Player: 3, 1, 1, speed = 4

      // Going right
      Animation = &Player->GoingRight;
      Animation->FrameCount = 3;
      Animation->Frames[0] = {0, 0, 3};
      Animation->Frames[1] = {24, 0, 1};
      Animation->Frames[2] = {48, 0, 1};

      // Going left
      Animation = &Player->GoingLeft;
      Animation->FrameCount = 3;
      Animation->Frames[0] = {0, 32, 3};
      Animation->Frames[1] = {24, 32, 1};
      Animation->Frames[2] = {48, 32, 1};

      // NOTE:
      // - Enemies: 4, 4, 6, speed = 2
      // - Player: 2, 2, 3, speed = 4

      // On rope right
      Animation = &Player->RopeRight;
      Animation->FrameCount = 3;
      Animation->Frames[0] = {0, 64, 2};
      Animation->Frames[1] = {24, 64, 2};
      Animation->Frames[2] = {48, 64, 3};

      // On rope right
      Animation = &Player->RopeLeft;
      Animation->FrameCount = 3;
      Animation->Frames[0] = {0, 96, 2};
      Animation->Frames[1] = {24, 96, 2};
      Animation->Frames[2] = {48, 96, 3};

      // On ladder
      Animation = &Player->Climbing;
      Animation->FrameCount = 2;
      Animation->Frames[0] = {0, 128, 2};
      Animation->Frames[1] = {24, 128, 2};
    }
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

  // Update player
  {
    player_input *Input = &NewInput->Player2;

    bool32 PressedUp = Input->Up.EndedDown;
    bool32 PressedDown = Input->Down.EndedDown;
    bool32 PressedLeft = Input->Left.EndedDown;
    bool32 PressedRight = Input->Right.EndedDown;
    bool32 PressedFire = Input->Fire.EndedDown;

    if (Input->Debug.EndedDown) {
      gDrawDebug = !gDrawDebug;
    }

    r32 Speed = 4.0f;
#ifdef BUILD_INTERNAL
    // Turbo
    if (Input->Turbo.EndedDown) {
      Speed *= 5;
      Turbo = true;
    }
#endif

    // TODO: make the player fall from the ladder only when he
    // doesn't touch the ladder at all
    bool32 OnLadder = CheckTile(Player->TileY, Player->TileX) == LVL_LADDER;

    bool32 LadderBelow = false;
    {
      int Col = Player->TileX;
      int Row = Player->TileY + 1;
      int PlayerBottom = (int)Player->Y + Player->Height / 2;
      int TileTop = Row * kTileHeight;
      if (CheckTile(Row, Col) == LVL_LADDER) {
        if (PlayerBottom >= TileTop) LadderBelow = true;
      }
    }

    bool32 CanClimb = false;
    if (OnLadder || LadderBelow) {
      int LadderCenter = Player->TileX * kTileWidth + kTileWidth / 2;
      int PlayerX = (int)Player->X;
      CanClimb = Abs(PlayerX - LadderCenter) < (Player->Width / 3);
    }

    bool32 OnRope = false;
    if (CheckTile(Player->TileY, Player->TileX) == LVL_ROPE) {
      int RopeY = Player->TileY * kTileHeight;
      int PlayerTop = (int)Player->Y - Player->Height / 2;
      OnRope = (PlayerTop == RopeY) ||
               (RopeY - PlayerTop > 0 && RopeY - PlayerTop <= 3);
      // Adjust player on a rope
      if (OnRope) {
        Player->Y = (r32)(RopeY + Player->Height / 2);
      }
    }

    // Gravity
    bool32 IsFalling = false;
    {
      r32 Old = Player->Y;
      Player->Y += Speed;
      if (!AcceptableMove(Player) || OnLadder || LadderBelow || Turbo ||
          OnRope) {
        Player->Y = Old;
        IsFalling = false;
      } else {
        IsFalling = true;
      }
    }

    // Update based on movement keys
    if (PressedRight && (!IsFalling || Turbo)) {
      r32 Old = Player->X;
      Player->X += Speed;
      if (!AcceptableMove(Player)) {
        Player->X = Old;
      } else {
        if (!OnRope) {
          Player->Animation = &Player->GoingRight;
        } else {
          Player->Animation = &Player->RopeRight;
        }
        Animate = true;
        Player->Facing = RIGHT;
      }
    }

    if (PressedLeft && (!IsFalling || Turbo)) {
      r32 Old = Player->X;
      Player->X -= Speed;
      if (!AcceptableMove(Player)) {
        Player->X = Old;
      } else {
        if (!OnRope) {
          Player->Animation = &Player->GoingLeft;
        } else {
          Player->Animation = &Player->RopeLeft;
        }
        Animate = true;
        Player->Facing = LEFT;
      }
    }

    bool32 Climbing = false;
    if (PressedUp && (CanClimb || Turbo)) {
      r32 Old = Player->Y;
      Player->Y -= Speed;

      // @refactor?
      int PlayerTile = CheckTile(Player->TileY, Player->TileX);
      int PlayerBottom = (int)Player->Y + Player->Height / 2;

      bool32 GotFlying =
          (PlayerTile == LVL_BLANK || PlayerTile == LVL_TREASURE ||
           PlayerTile == LVL_ROPE) &&
          (PlayerBottom < (Player->TileY + 1) * kTileHeight);

      if (!AcceptableMove(Player) || GotFlying && !Turbo) {
        Player->Y = Old;
      } else {
        Climbing = true;
        Player->Animation = &Player->Climbing;
        Animate = true;
      }
    }

    bool32 Descending = false;
    if (PressedDown && (CanClimb || LadderBelow || OnRope || Turbo)) {
      r32 Old = Player->Y;
      Player->Y += Speed;
      if (!AcceptableMove(Player)) {
        Player->Y = Old;
      } else if (LadderBelow || CanClimb) {
        Descending = true;
        Player->Animation = &Player->Climbing;
        Animate = true;
      }
    }

    if (IsFalling) {
      Player->Sprite = Sprites->Falling;
      Animate = false;
    }

    // Adjust player on ladder
    if (CanClimb && ((Climbing && PressedUp) || (Descending && PressedDown))) {
      Player->X = (r32)(Player->TileX * kTileWidth + kTileWidth / 2);
    }

    Animate = Animate && !(PressedDown && PressedUp) &&
              !(PressedLeft && PressedRight);

    // Update player tile
    Player->TileX = (int)Player->X / kTileWidth;
    Player->TileY = (int)Player->Y / kTileHeight;

    // Fire
    if (PressedFire && !IsFalling && !Player->FireCooldown) {
      int TileY = Player->TileY + 1;
      int TileX = -10;
      if (Player->Facing == LEFT) {
        int BorderX = (Player->TileX + 1) * kTileWidth - (kHumanWidth / 2 - 4);
        if (Player->X < BorderX) {
          TileX = Player->TileX - 1;
        } else {
          TileX = Player->TileX;
          // Adjust
          Player->X += kHumanWidth / 2;
        }
      } else if (Player->Facing == RIGHT) {
        int BorderX = Player->TileX * kTileWidth + (kHumanWidth / 2 - 4);
        if (Player->X < BorderX) {
          TileX = Player->TileX;
          // Adjust
          Player->X -= kHumanWidth / 2;
        } else {
          TileX = Player->TileX + 1;
        }
      }
      Assert(TileX != -10);

      // @refactor: be consistent with the order of TileX, TileY
      // and maybe even the names (Col, Row)
      if (CheckTile(TileY, TileX) == LVL_BRICK) {
        // Crush the brick
        Level->Contents[TileY][TileX] = LVL_BLANK;
        DrawTile(TileX, TileY);
        Player->FireCooldown = 30;

        // Remember that
        crushed_brick *Brick = &CrushedBricks[NextBrickAvailable];
        NextBrickAvailable = (NextBrickAvailable + 1) % kCrushedBrickCount;

        Assert(Brick->IsUsed == false);

        Brick->IsUsed = true;
        Brick->TileX = TileX;
        Brick->TileY = TileY;
        Brick->Width = 32;
        Brick->Height = 64;
        Brick->Sprite = Sprites->Breaking;

        // Init animation
        animation *Animation = &Brick->Breaking;
        Animation->FrameCount = 3;
        Animation->Frames[0] = {96, 32, 2};
        Animation->Frames[1] = {128, 32, 2};
        Animation->Frames[2] = {160, 32, 2};
      }
    }
    if (Player->FireCooldown > 0) Player->FireCooldown--;
  }

  if (Animate && !Turbo && Player->Animation != NULL) {
    animation *Animation = Player->Animation;
    frame *Frame = &Animation->Frames[Animation->Frame];

    if (Animation->Counter > Frame->Lasting) {
      Animation->Counter = 0;
    }

    if (Animation->Counter == 0) {
      Player->Sprite.XOffset = Frame->XOffset;
      Player->Sprite.YOffset = Frame->YOffset;

      DrawPlayer(Player);

      Animation->Frame = (Animation->Frame + 1) % Animation->FrameCount;
    }

    Animation->Counter += 1;
  } else {
    DrawPlayer(Player);
  }

  // Animate bricks
  for (int i = 0; i < kCrushedBrickCount; i++) {
    crushed_brick *Brick = &CrushedBricks[i];
    if (Brick->IsUsed) {

      // @copypaste
      animation *Animation = &Brick->Breaking;
      frame *Frame = &Animation->Frames[Animation->Frame];

      if (Animation->Counter > Frame->Lasting) {
        Animation->Counter = 0;
        Animation->Frame++;
        Frame = &Animation->Frames[Animation->Frame];
      }

      // Don't show more frames than there is
      if (Animation->Frame == Animation->FrameCount) {
        Brick->IsUsed = false;
        continue;
      }

      if (Animation->Counter == 0) {
        Brick->Sprite.XOffset = Frame->XOffset;
        Brick->Sprite.YOffset = Frame->YOffset;
      }

      v2 Position = {};
      Position.x = (r32)Brick->TileX * kTileWidth;
      Position.y = (r32)(Brick->TileY - 1) * kTileHeight;
      DrawSprite(Position, Brick->Sprite);

      Animation->Counter += 1;
    }
  }
}
