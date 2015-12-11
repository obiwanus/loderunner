#ifndef LODERUNNER_MATH_H
#define LODERUNNER_MATH_H

#include <math.h>

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