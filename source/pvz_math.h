// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

#include "pvz_platform.h"

//====================================================================================================================//
//----------------------------------------------------- UTILITIES ----------------------------------------------------//
//====================================================================================================================//

inline f32
Min(f32 A, f32 B)
{
    const f32 Result = (A < B) ? A : B;
    return Result;
}

inline f32
Max(f32 A, f32 B)
{
    const f32 Result = (A > B) ? A : B;
    return Result;
}

inline f32
Clamp(f32 Value, f32 MinBound, f32 MaxBound)
{
    const f32 Result = Min(MaxBound, Max(MinBound, Value));
    return Result;
}

inline f32
Abs(f32 Value)
{
    const f32 Result = (Value >= 0.0F) ? Value : -Value;
    return Result;
}

inline f32
Math_Lerp(f32 ValueA, f32 ValueB, f32 T)
{
    const f32 Result = ValueA + T * (ValueB - ValueA);
    return Result;
}

inline f32
Math_InverseLerp(f32 ValueA, f32 ValueB, f32 InterpolatedValue)
{
    ASSERT(ValueA != ValueB);
    const f32 Result = (InterpolatedValue - ValueA) / (ValueB - ValueA);
    return Result;
}

//====================================================================================================================//
//------------------------------------------------------ VECTORS -----------------------------------------------------//
//====================================================================================================================//

struct vec2
{
    f32 X;
    f32 Y;
};

inline vec2
Vec2(f32 X, f32 Y)
{
    vec2 Result;
    Result.X = X;
    Result.Y = Y;
    return Result;
}

inline vec2
Vec2(f32 Scalar)
{
    vec2 Result;
    Result.X = Scalar;
    Result.Y = Scalar;
    return Result;
}

inline vec2
operator+(vec2 A, vec2 B)
{
    vec2 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

inline vec2
operator-(vec2 A, vec2 B)
{
    vec2 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

inline vec2
operator-(vec2 Vec)
{
    vec2 Result;
    Result.X = -Vec.X;
    Result.Y = -Vec.Y;
    return Result;
}

inline vec2
operator*(vec2 Vec, f32 Scalar)
{
    vec2 Result;
    Result.X = Vec.X * Scalar;
    Result.Y = Vec.Y * Scalar;
    return Result;
}

inline vec2
operator*(f32 Scalar, vec2 Vec)
{
    vec2 Result;
    Result.X = Scalar * Vec.X;
    Result.Y = Scalar * Vec.Y;
    return Result;
}

inline f32
Vec2_DistanceSquared(vec2 A, vec2 B)
{
    const vec2 Delta = B - A;
    const f32 Result = (Delta.X * Delta.X) + (Delta.Y * Delta.Y);
    return Result;
}

inline vec2
Math_LerpVec2(vec2 A, vec2 B, f32 T)
{
    vec2 Result;
    Result.X = Math_Lerp(A.X, B.X, T);
    Result.Y = Math_Lerp(A.Y, B.Y, T);
    return Result;
}

//====================================================================================================================//
//------------------------------------------------------- RECTS ------------------------------------------------------//
//====================================================================================================================//

struct rect2D
{
    vec2 Min;
    vec2 Max;
};

inline rect2D
Rect2D(vec2 Min, vec2 Max)
{
    rect2D Result;
    Result.Min = Min;
    Result.Max = Max;
    return Result;
}

inline rect2D
Rect2D_FromOffsetSize(vec2 Offset, vec2 Size)
{
    rect2D Result;
    Result.Min.X = Offset.X;
    Result.Min.Y = Offset.Y;
    Result.Max.X = Result.Min.X + Size.X;
    Result.Max.Y = Result.Min.Y + Size.Y;
    return Result;
}

inline f32
Rect2D_GetSizeX(rect2D Rect)
{
    const f32 Result = Rect.Max.X - Rect.Min.X;
    return Result;
}

inline f32
Rect2D_GetSizeY(rect2D Rect)
{
    const f32 Result = Rect.Max.Y - Rect.Min.Y;
    return Result;
}

inline vec2
Rect2D_GetSize(rect2D Rect)
{
    vec2 Result;
    Result.X = Rect.Max.X - Rect.Min.X;
    Result.Y = Rect.Max.Y - Rect.Min.Y;
    return Result;
}

inline b8
Rect2D_IsDegenerated(rect2D Rect)
{
    const b8 Result =
        (Rect.Min.X >= Rect.Max.X) ||
        (Rect.Min.Y >= Rect.Max.Y);
    return Result;
}

inline rect2D
Rect2D_Intersect(rect2D A, rect2D B)
{
    rect2D Result;
    Result.Min.X = Max(A.Min.X, B.Min.X);
    Result.Min.Y = Max(A.Min.Y, B.Min.Y);
    Result.Max.X = Min(A.Max.X, B.Max.X);
    Result.Max.Y = Min(A.Max.Y, B.Max.Y);
    return Result;
}

//====================================================================================================================//
//------------------------------------------------------ COLORS ------------------------------------------------------//
//====================================================================================================================//

struct linear_color
{
    u8 B;
    u8 G;
    u8 R;
    u8 A;
};

struct color4
{
    f32 R;
    f32 G;
    f32 B;
    f32 A;
};

inline linear_color
LinearColor(u8 R, u8 G, u8 B, u8 A = 255)
{
    linear_color Result;
    Result.B = B;
    Result.G = G;
    Result.R = R;
    Result.A = A;
    return Result;
}

inline color4
Color4(f32 R, f32 G, f32 B, f32 A = 1.0F)
{
    color4 Result;
    Result.R = R;
    Result.G = G;
    Result.B = B;
    Result.A = A;
    return Result;
}

inline color4
Color4(f32 Grayscale)
{
    color4 Result;
    Result.R = Grayscale;
    Result.G = Grayscale;
    Result.B = Grayscale;
    Result.A = Grayscale;
    return Result;
}

inline color4
Color4_FromLinear(linear_color LinearColor)
{
    color4 Result;
    Result.R = (f32)LinearColor.R / 255.0F;
    Result.G = (f32)LinearColor.G / 255.0F;
    Result.B = (f32)LinearColor.B / 255.0F;
    Result.A = (f32)LinearColor.A / 255.0F;
    return Result;
}

inline linear_color
Color4_ToLinear(color4 Color)
{
    linear_color Result;
    Result.R = (u8)(Color.R * 255.0F);
    Result.G = (u8)(Color.G * 255.0F);
    Result.B = (u8)(Color.B * 255.0F);
    Result.A = (u8)(Color.A * 255.0F);
    return Result;
}

internal color4
Math_LerpColor4(color4 A, color4 B, f32 T)
{
    color4 Result;
    Result.R = Math_Lerp(A.R, B.R, T);
    Result.G = Math_Lerp(A.G, B.G, T);
    Result.B = Math_Lerp(A.B, B.B, T);
    Result.A = Math_Lerp(A.A, B.A, T);
    return Result;
}

inline linear_color
LinearColor_UnpackFromBGRA(u32 PackedColor)
{
    const linear_color UnpackedColor = *(linear_color*)&PackedColor;
    return UnpackedColor;
}

inline u32
LinearColor_PackToBGRA(linear_color UnpackedColor)
{
    const u32 PackedColor = *(u32*)&UnpackedColor;
    return PackedColor;
}

//====================================================================================================================//
//------------------------------------------------------ RANDOM ------------------------------------------------------//
//====================================================================================================================//

//
// NOTE(Traian): The pseudo-number-generator used by the engine follows the PCG algorithm.
// This implementation is closely tied to 'https://en.wikipedia.org/wiki/Permuted_congruential_generator'.
//

struct random_series
{
    u64 State;
    u64 Increment;
};

inline u32
Random_NextU32(random_series* Series)
{
    const u64 OldState = Series->State;
    Series->State = (OldState * 6364136223846793005ULL) + Series->Increment;
    const u32 XORShifted = ((OldState >> 18) ^ OldState) >> 27;
    const u32 Rotated = OldState >> 59;
    const u32 Result = (XORShifted >> Rotated) | (XORShifted << ((-Rotated) & 31));
    return Result;
}

inline void
Random_InitializeSeries(random_series* Series, u64 Seed = 42, u64 Sequence = 54)
{
    Series->State = 0;
    Series->Increment = (Sequence << 1) | 1;

    Random_NextU32(Series);
    Series->State += Seed;
    Random_NextU32(Series);
}

internal u32
Random_RangeU32(random_series* Series, u32 MinBound, u32 MaxBound)
{
    ASSERT(MinBound <= MaxBound);
    const u32 Result = MinBound + (Random_NextU32(Series) % (MaxBound - MinBound + 1));
    return Result;
}

internal f32
Random_NextF32(random_series* Series)
{
    const u32 NextU32 = Random_NextU32(Series);
    const f32 Result = (f32)NextU32 / (f32)(0xFFFFFFFF);
    return Result;
}

internal f32
Random_RangeF32(random_series* Series, f32 MinBound, f32 MaxBound)
{
    // ASSERT(MinBound <= MaxBound);
    const f32 NextF32 = Random_NextF32(Series);
    const f32 Result = Math_Lerp(MinBound, MaxBound, NextF32);
    return Result;
}

internal f32
Random_SignF32(random_series* Series)
{
    const u32 NextU32 = Random_NextU32(Series);
    const f32 Result = (2.0F * (f32)(NextU32 & 0x01)) - 1.0F;
    return Result;
}

internal vec2
Random_PointInRectangle2D(random_series* Series, rect2D Rectangle)
{
    ASSERT(!Rect2D_IsDegenerated(Rectangle));
    vec2 Result;
    Result.X = Random_RangeF32(Series, Rectangle.Min.X, Rectangle.Max.X);
    Result.Y = Random_RangeF32(Series, Rectangle.Min.Y, Rectangle.Max.Y);
    return Result;
}
