#ifndef LODERUNNER_MATH_H
#define LODERUNNER_MATH_H

#include <math.h>
#include <stdlib.h>

int randint(int n) {
  // Copied from StackOverflow
  if ((n - 1) == RAND_MAX) {
    return rand();
  } else {
    // Chop off all of the values that would cause skew...
    long end = RAND_MAX / n;  // truncate skew
    Assert(end > 0L);
    end *= n;

    int r;
    while ((r = rand()) >= end)
      ;

    return r % n;
  }
}

union v2 {
  struct {
    r32 x, y;
  };
  struct {
    r32 u, v;
  };
  r32 E[2];
};

inline v2 operator*(r32 Scalar, v2 A) {
  v2 Result;

  Result.x = A.x * Scalar;
  Result.y = A.y * Scalar;

  return Result;
}

inline v2 operator*(v2 A, r32 Scalar) { return Scalar * A; }

inline v2 &operator*=(v2 &A, r32 Scalar) {
  A = A * Scalar;

  return A;
}

inline v2 operator+(v2 A, v2 B) {
  v2 Result;

  Result.x = A.x + B.x;
  Result.y = A.y + B.y;

  return Result;
}

inline v2 &operator+=(v2 &A, v2 B) {
  A = A + B;

  return A;
}

inline v2 operator-(v2 A, v2 B) {
  v2 Result;

  Result.x = A.x - B.x;
  Result.y = A.y - B.y;

  return Result;
}

inline v2 &operator-=(v2 &A, v2 B) {
  A = A - B;

  return A;
}

// Unary
inline v2 operator-(v2 A) {
  v2 Result;

  Result.x = -A.x;
  Result.y = -A.y;

  return Result;
}

// -------------------------------------

union v2i {
  struct {
    int x, y;
  };
  struct {
    int u, v;
  };
  int E[2];
};

inline v2i operator*(int Scalar, v2i A) {
  v2i Result;

  Result.x = A.x * Scalar;
  Result.y = A.y * Scalar;

  return Result;
}

inline v2i operator*(v2i A, int Scalar) { return Scalar * A; }

inline v2i &operator*=(v2i &A, int Scalar) {
  A = A * Scalar;

  return A;
}

inline v2i operator+(v2i A, v2i B) {
  v2i Result;

  Result.x = A.x + B.x;
  Result.y = A.y + B.y;

  return Result;
}

inline v2i &operator+=(v2i &A, v2i B) {
  A = A + B;

  return A;
}

inline v2i operator-(v2i A, v2i B) {
  v2i Result;

  Result.x = A.x - B.x;
  Result.y = A.y - B.y;

  return Result;
}

inline v2i &operator-=(v2i &A, v2i B) {
  A = A - B;

  return A;
}

inline bool32 operator==(v2i &A, v2i B) {
  bool32 Result = (A.x == B.x && A.y == B.y);

  return Result;
}

inline bool32 operator!=(v2i &A, v2i B) {
  bool32 Result = (A.x != B.x || A.y != B.y);

  return Result;
}

// Unary
inline v2i operator-(v2i A) {
  v2i Result;

  Result.x = -A.x;
  Result.y = -A.y;

  return Result;
}

// -------------------------------------

inline r32 Square(r32 Real32) { return Real32 * Real32; }

inline int Square(int Int) { return Int * Int; }

inline r32 SquareRoot(r32 Real32) {
  r32 Result = sqrtf(Real32);
  return Result;
}

inline r32 Abs(r32 Value) {
  if (Value >= 0)
    return Value;
  else
    return -Value;
}

inline int Abs(int Value) {
  if (Value >= 0)
    return Value;
  else
    return -Value;
}

inline r32 V2Length(v2 Vector) {
  r32 Result = SquareRoot(Square(Vector.x) + Square(Vector.y));
  return Result;
}

inline r32 DistanceBetween(v2 Dot1, v2 Dot2) {
  v2 Diff = Dot2 - Dot1;
  return V2Length(Diff);
}

inline r32 DotProduct(v2 Vector1, v2 Vector2) {
  r32 Result = Vector1.x * Vector2.x + Vector1.y * Vector2.y;
  return Result;
}

#endif