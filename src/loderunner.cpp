#include "loderunner.h"

// TODO: delete
#include <windows.h>


// These are saved as global vars on the first call of GameUpdateAndRender
global game_offscreen_buffer *GameBackBuffer;
global game_memory *GameMemory;

global entity *Players;
global entity *Ball;


inline int
TruncateReal32(r32 Value)
{
    int Result = (int) Value;
    return Result;
}


inline int
RoundReal32(r32 Value)
{
    // TODO: think about overflow
    return TruncateReal32(Value + 0.5f);
}


void *
GameMemoryAlloc(int SizeInBytes)
{
    void *Result = GameMemory->Free;

    GameMemory->Free = (void *)((u8 *)GameMemory->Free + SizeInBytes);
    i64 CurrentSize = ((u8 *)GameMemory->Free - (u8 *) GameMemory->Start);
    Assert(CurrentSize < GameMemory->MemorySize);

    return Result;
}


inline u8
UnmaskColor(u32 Pixel, u32 ColorMask)
{
    int BitOffset = 0;
    switch (ColorMask)
    {
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

    return (u8)((Pixel&ColorMask) >> BitOffset);
}


internal void
DEBUGDrawRectangle(v2 Position, int Width, int Height, u32 Color)
{
    int X = (int)Position.x;
    int Y = (int)Position.y;

    int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
    u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y + X * GameBackBuffer->BytesPerPixel;

    for (int pY = Y; pY < Y + Height; pY++)
    {
        u32 *Pixel = (u32 *) Row;
        for (int pX = X; pX < X + Width; pX++)
        {
            *Pixel++ = Color;
        }
        Row += Pitch;
    }
}


inline void
SetPixel(int X, int Y, u32 Color)
{
    int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
    u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y + X * GameBackBuffer->BytesPerPixel;
    u32 *Pixel = (u32 *)Row;
    *Pixel = Color;
}


internal void
DrawLine(r32 StartX, r32 StartY, r32 EndX, r32 EndY, u32 Color)
{
    // Bresenham's algorithm
    r32 DeltaX = EndX - StartX;
    r32 DeltaY = EndY - StartY;
    int SignX = (DeltaX < 0) ? -1: 1;
    int SignY = (DeltaY < 0) ? -1: 1;
    int X = (int)StartX;
    int Y = (int)StartY;
    r32 Error = 0;

    if (DeltaX)
    {
        r32 DeltaErr = Abs(DeltaY / DeltaX);
        while (X != (int)EndX)
        {
            SetPixel(X, Y, Color);
            X += SignX;
            Error += DeltaErr;
            while (Error >= 0.5f)
            {
                SetPixel(X, Y, Color);
                Y += SignY;
                Error -= 1.0f;
            }
        }
    }
    else if (DeltaY)
    {
        while (Y != (int)EndY)
        {
            SetPixel(X, Y, Color);
            Y += SignY;
        }
    }

}


internal void
DEBUGDrawCircle(v2 Center, int Radius, u32 Color)
{
    int X = (int)Center.x - Radius;
    int Y = (int)Center.y - Radius;
    int Height = Radius * 2;
    int Width = Radius * 2;
    int CenterX = (int)Center.x;
    int CenterY = (int)Center.y;
    int RadiusSq = Radius * Radius;

    int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
    u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y + X * GameBackBuffer->BytesPerPixel;

    for (int pY = Y; pY < Y + Height; pY++)
    {
        int *Pixel = (int *) Row;
        for (int pX = X; pX < X + Width; pX++)
        {
            int nX = CenterX - pX;
            int nY = CenterY - pY;
            if ((nX * nX + nY * nY) <= RadiusSq)
            {
                *Pixel = Color;
            }
            Pixel++;
        }
        Row += Pitch;
    }
}


internal void
DEBUGDrawEllipse(v2 Center, int Width, int Height, u32 Color)
{
    int X = (int)Center.x - Width / 2;
    int Y = (int)Center.y - Height / 2;
    int CenterX = (int)Center.x;
    int CenterY = (int)Center.y;
    int HeightSq = Height * Height / 4;
    int WidthSq = Width * Width / 4;
    int RadiusSq = HeightSq * WidthSq;

    int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
    u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y + X * GameBackBuffer->BytesPerPixel;

    for (int pY = Y; pY < Y + Height; pY++)
    {
        int *Pixel = (int *) Row;
        for (int pX = X; pX < X + Width; pX++)
        {
            int nX = CenterX - pX;
            int nY = CenterY - pY;
            if ((nX * nX * HeightSq + nY * nY * WidthSq) <= RadiusSq)
            {
                *Pixel = Color;
            }
            Pixel++;
        }
        Row += Pitch;
    }
}


internal void
DEBUGDrawImage(v2 Position, bmp_file Image)
{
    int Width = Image.Width;
    int Height = Image.Width;
    int X = (int)Position.x;
    int Y = (int)Position.y;

    int Pitch = GameBackBuffer->Width * GameBackBuffer->BytesPerPixel;
    u8 *Row = (u8 *)GameBackBuffer->Memory + Pitch * Y + X * GameBackBuffer->BytesPerPixel;
    u32 *SrcRow = (u32 *)Image.Bitmap + Height * Width - Width;  // bottom row first

    for (int pY = Y; pY < Y + Height; pY++)
    {
        int *Pixel = (int *) Row;
        int *SrcPixel = (int *) SrcRow;

        for (int pX = X; pX < X + Width; pX++)
        {
            // TODO: bmp may not be masked
            u8 Red = UnmaskColor(*SrcPixel, Image.RedMask);
            u8 Green = UnmaskColor(*SrcPixel, Image.GreenMask);
            u8 Blue = UnmaskColor(*SrcPixel, Image.BlueMask);
            u8 Alpha = UnmaskColor(*SrcPixel, Image.AlphaMask);

            u32 ResultingColor = Red << 16 | Green << 8 | Blue;

            if (Alpha > 0 && Alpha < 0xFF)
            {
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

                *Pixel = (((u8)NewRed << 16) |
                          ((u8)NewGreen << 8) |
                          ((u8)NewBlue << 0));
            }
            else if (Alpha == 0xFF)
            {
                *Pixel = ResultingColor;
            }
            else
            {
                // do nothing
            }

            Pixel++;
            SrcPixel++;
        }
        Row += Pitch;
        SrcRow -= Width;
    }
}


internal bmp_file
DEBUGReadBMPFile(char *Filename)
{
    bmp_file Result = {};
    file_read_result FileReadResult = GameMemory->DEBUGPlatformReadEntireFile(Filename);

    bmp_file_header *BMPFileHeader = (bmp_file_header *)FileReadResult.Memory;
    bmp_info_header *BMPInfoHeader = (bmp_info_header *)((u8 *)FileReadResult.Memory
                                                          + sizeof(bmp_file_header));

    Result.Bitmap = (void *) ((u8 *)FileReadResult.Memory +
                              BMPFileHeader->bfOffBits);
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


internal void
InitEntity(entity *Entity, char *ImgPath, v2 Position, v2 Velocity,
           v2 Center, int Radius, r32 Mass)
{
    bmp_file BMPFile = DEBUGReadBMPFile(ImgPath);
    Entity->Image = BMPFile;
    Entity->Position = Position;
    Entity->Velocity = Velocity;

    Entity->Center = Center;
    Entity->Radius = Radius;
    Entity->Mass = Mass;
    Entity->NotJumped = true;
}


internal void
CollideWithWalls(entity *Entity)
{
    r32 MinX = (r32)GameBackBuffer->Width * 0.05f;
    r32 MinY = 0.0f;
    r32 MaxX = (r32)GameBackBuffer->Width * 0.95f - (r32)Entity->Image.Width;
    r32 MaxY = (r32)GameBackBuffer->Height * 0.95f - (r32)Entity->Image.Height;

    if (Entity->Position.x < MinX)
    {
        Entity->Position.x = MinX;
        Entity->Velocity.x = -Entity->Velocity.x;
    }
    if (Entity->Position.y < MinY)
    {
        Entity->Position.y = MinY;
        Entity->Velocity.y = -Entity->Velocity.y;
    }
    if (Entity->Position.x > MaxX)
    {
        Entity->Position.x = MaxX;
        Entity->Velocity.x = -Entity->Velocity.x;
    }
    if (Entity->Position.y > MaxY)
    {
        Entity->Position.y = MaxY;
        Entity->Velocity.y = -Entity->Velocity.y;
        Entity->NotJumped = true;
    }
}


internal void
LimitVelocity(entity *Entity, r32 Max)
{
    if (Entity->Velocity.x > Max)
        Entity->Velocity.x = Max;
    if (Entity->Velocity.x < -Max)
        Entity->Velocity.x = -Max;
    if (Entity->Velocity.y > Max)
        Entity->Velocity.y = Max;
    if (Entity->Velocity.y < -Max)
        Entity->Velocity.y = -Max;
}


internal void
UpdatePlayer(entity *Player, player_input *Input, r32 dtForFrame)
{
    v2 Direction = {};

    if (Input->Up.EndedDown && Player->NotJumped)
        Direction.y -= 2.0f;
    if (Input->Right.EndedDown)
        Direction.x += 1.0f;
    if (Input->Left.EndedDown)
        Direction.x -= 1.0f;

    if (Direction.x != 0 && Direction.y != 0)
    {
        Direction *= 0.70710678118f;
    }

    Direction *= 0.25f;  // speed, px/ms
    Direction.x -= 0.2f * Player->Velocity.x;  // friction
    Direction.y -= 0.05f * Player->Velocity.y;  // friction

    Player->Velocity += Direction;

    r32 FloorY = (r32)GameBackBuffer->Height * 0.95f - (r32)Player->Image.Height;
    if (Player->Position.y <= (FloorY - Player->Image.Height / 2))
    {
        Player->Velocity.y += 0.1f;  // gravity
    }

    Player->Position += Player->Velocity * dtForFrame;

    CollideWithWalls(Player);

    r32 MaxJump = (r32)GameBackBuffer->Height * 0.5f;
    if (Player->Position.y <= MaxJump)
    {
        Player->NotJumped = false;
    }

    LimitVelocity(Player, 1.0f);

    // char Buffer[256];
    // sprintf_s(Buffer, "%.2f, %.2f\n", Player->Velocity.x, Player->Velocity.y);
    // OutputDebugStringA(Buffer);
}


internal void
DEBUGDrawEntity(entity *Entity)
{
    // Draw the shadow
    v2 ShadowPosition = Entity->Position + Entity->Center;
    ShadowPosition.y = (r32)GameBackBuffer->Height * 0.95f - 5.0f;  // floor
    int Width = Entity->Image.Width -
                (int)((r32)Entity->Image.Width * 0.6f *
                     ((r32)(ShadowPosition.y - Entity->Position.y)) / (r32)GameBackBuffer->Height);
    DEBUGDrawEllipse(ShadowPosition, Width, Width / 5, 0x00111111);

    // Draw the entity
    DEBUGDrawImage(Entity->Position, Entity->Image);
}


internal bool32
Collides(v2 Center, int Radius, v2 RectCorner, int RectWidth, int RectHeight)
{
    // Detects collisions betweer a ball and a rect
    if ((Center.x - (r32)Radius) > (RectCorner.x + (r32)RectWidth)) return false;
    if ((Center.x + (r32)Radius) < RectCorner.x) return false;
    if ((Center.y + (r32)Radius) < RectCorner.y) return false;

    return true;
}


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // Update global vars
    GameBackBuffer = Buffer;
    GameMemory = Memory;

    if (!Players)
    {
        Players = (entity *)GameMemoryAlloc(sizeof(entity) * 2);  // 2 players
        Ball = (entity *)GameMemoryAlloc(sizeof(entity) * 2);  // 1 ball

        InitEntity(
            &Players[0],
            "./img/player.bmp",
            {100, 400},
            {0, 0},
            {64, 69},
            60,
            4);

        InitEntity(
            &Players[1],
            "./img/player.bmp",
            {600, 400},
            {0, 0},
            {64, 69},
            60,
            4);

        InitEntity(
            Ball,
            "./img/ball.bmp",
            {300, 300},
            {0, 0},
            // {-0.5f, -0.5f},
            {36, 35},
            32,
            10);
    }

    // Move and draw image
    DEBUGDrawRectangle({0, 0}, GameBackBuffer->Width, GameBackBuffer->Height, 0x00002222);  // OMG

    // Update players
    {
        UpdatePlayer(&Players[0], &NewInput->Players[0], NewInput->dtForFrame);
        UpdatePlayer(&Players[1], &NewInput->Players[1], NewInput->dtForFrame);
    }

    // Update ball
    {
        Ball->Position += Ball->Velocity * NewInput->dtForFrame;
        Ball->Velocity.y += 0.01f;
        CollideWithWalls(Ball);
        LimitVelocity(Ball, 1.0f);
    }

    // Ball-players collisions

    // TODO: don't allow intersections at all?
    for (int i = 0; i < COUNT_OF(NewInput->Players); i++)
    {
        entity *Player = &Players[i];
        v2 pCenter = Player->Position + Player->Center;
        v2 bCenter = Ball->Position + Ball->Center;
        if (DistanceBetween(pCenter, bCenter) < (Player->Radius + Ball->Radius))
        {
            // Collision!
            v2 CollisionNormal = (bCenter - pCenter) * (1.0f / DistanceBetween(bCenter, pCenter));

            // The components of the velocities collinear with the normal
            v2 BallComponent = CollisionNormal * DotProduct(Ball->Velocity, CollisionNormal);
            v2 PlayerComponent = CollisionNormal * DotProduct(Player->Velocity, CollisionNormal);
            v2 ResultingV = BallComponent - PlayerComponent;

            v2 ResultingBallV = -1.0f * ResultingV * ((Ball->Mass + Player->Mass) / (2.0f * Ball->Mass));
            v2 ResultingPlayerV = 0.5f * ResultingV * ((Ball->Mass + Player->Mass) / (2.0f * Player->Mass));

            Ball->Velocity += ResultingBallV - BallComponent;
            Player->Velocity += ResultingPlayerV - PlayerComponent;
        }
    }

    // Collisions with the net
    int NetWidth = 26;
    r32 NetHeight = 0.5f;  // percent
    r32 Width = (r32)GameBackBuffer->Width;
    r32 Height = (r32)GameBackBuffer->Height;
    r32 Bottom = Height;
    r32 Right = Width;
    int CenterX = (int)Width / 2;
    {
        int RealHeight = (int)(Height * NetHeight + 0.1f * Height / 6.0f);
        v2 LeftCorner = {(r32)(CenterX - NetWidth / 2), (r32)((int)Bottom - RealHeight)};  // for collisions

        // Ball
        v2 BallCenter = Ball->Position + Ball->Center;
        if (Collides(BallCenter, Ball->Radius, LeftCorner, NetWidth, RealHeight))
        {
            if ((BallCenter.y + (Ball->Radius / 2.0f)) <= LeftCorner.y)
            {
                Ball->Velocity.y = -Ball->Velocity.y;
            }
            else
            {
                Ball->Velocity.x = -Ball->Velocity.x;
            }
        }

        // Players
        for (int i = 0; i < COUNT_OF(NewInput->Players); i++)
        {
            entity *Player = &Players[i];
            v2 pCenter = Player->Position + Player->Center;

            if (Collides(pCenter, Player->Radius, LeftCorner, NetWidth, RealHeight))
            {
                Player->Velocity.x = -Player->Velocity.x;
                if (pCenter.x < LeftCorner.x)
                {
                    Player->Position.x = LeftCorner.x - (r32)Player->Radius * 2.0f;
                }
                else
                {
                    Player->Position.x = LeftCorner.x + (r32)NetWidth;
                }
            }
        }

    }

    // Draw background
    {
        u32 Color = 0x00001111;

        DrawLine(0, Bottom, 0.1f * Width, 0.9f * Height, Color);
        DrawLine(Right, Bottom, 0.9f * Width, 0.9f * Height, Color);
        DrawLine(0.1f * Width, 0.9f * Height, 0.9f * Width, 0.9f * Height, Color);
        DrawLine(0.9f * Width, 0, 0.9f * Width, 0.9f * Height, Color);
        DrawLine(0.1f * Width, 0.9f * Height, 0.1f * Width, 0, Color);

        // The net
        v2 LeftCorner = {(r32)(CenterX - NetWidth / 2), Bottom - NetHeight * Height};
        DEBUGDrawRectangle(LeftCorner, NetWidth, (int)(NetHeight * Height), Color);
        int X1, X2, Y;  // the furthermost points of the net
        Y = (int)LeftCorner.y - (int)(0.1f * Height / 3.0f);
        X1 = (int)LeftCorner.x + (NetWidth / 3);
        X2 = (int)LeftCorner.x + NetWidth - (NetWidth / 3);
        DrawLine(LeftCorner.x, LeftCorner.y, (r32)X1, (r32)Y, Color);
        DrawLine(LeftCorner.x + NetWidth, LeftCorner.y, (r32)X2, (r32)Y, Color);
        DrawLine((r32)X1, (r32)Y, (r32)X2, (r32)Y, Color);
    }

    // Draw moving objects
    {
        DEBUGDrawEntity(&Players[0]);
        DEBUGDrawEntity(&Players[1]);
        DEBUGDrawEntity(Ball);
    }
}


