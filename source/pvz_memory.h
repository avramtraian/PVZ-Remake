// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

#include "pvz_platform.h"

#define ZERO_STRUCT(S)          { ZeroMemory(&(S), sizeof(S)); }
#define ZERO_STRUCT_POINTER(P)  { ZeroMemory(P, sizeof(*(P))); }
#define ZERO_STRUCT_ARRAY(A, C) { ZeroMemory(A, (C) * sizeof((A)[0])); }

#define KILOBYTES(X)    ((memory_size)1024 * (memory_size)(X))
#define MEGABYTES(X)    ((memory_size)1024 * KILOBYTES(X))
#define GIGABYTES(X)    ((memory_size)1024 * MEGABYTES(X))

//====================================================================================================================//
//--------------------------------------------------- MEMORY ARENA ---------------------------------------------------//
//====================================================================================================================//

struct memory_arena
{
    void*       MemoryBlock;
    memory_size ByteCount;
    memory_size AllocatedByteCount;
};

struct memory_temporary_arena
{
    memory_arena*   Arena;
    memory_size     BaseAllocatedByteCount;
};

function void*                      MemoryArena_Allocate        (memory_arena* Arena, memory_size AllocationSize,
                                                                 memory_size AllocationAlignment);

function void                       MemoryArena_Reset           (memory_arena* Arena);

function memory_temporary_arena     MemoryArena_BeginTemporary  (memory_arena* Arena);

function void                       MemoryArena_EndTemporary    (memory_temporary_arena* TemporaryArena);

#define PUSH(Arena, Type)               (Type*)MemoryArena_Allocate(Arena, sizeof(Type), alignof(Type))
#define PUSH_ARRAY(Arena, Type, Count)  (Type*)MemoryArena_Allocate(Arena, (Count) * sizeof(Type), alignof(Type))

//====================================================================================================================//
//--------------------------------------------------- MEMORY STREAM --------------------------------------------------//
//====================================================================================================================//

struct memory_stream
{
    void*       MemoryBlock;
    memory_size ByteCount;
    memory_size ByteOffset;
};

function void                       MemoryStream_Initialize     (memory_stream* Stream,
                                                                 void* MemoryBlock, memory_size ByteCount);

function void                       MemoryStream_Reset          (memory_stream* Stream);

function void*                      MemoryStream_Consume        (memory_stream* Stream, memory_size ByteCount);

function void*                      MemoryStream_TryConsume     (memory_stream* Stream, memory_size ByteCount);

function void*                      MemoryStream_Peek           (memory_stream* Stream, memory_size ByteCount);

#define PEEK(Stream, Type)                  (Type*)MemoryStream_Peek(Stream, sizeof(Type))
#define PEEK_ARRAY(Stream, Type, Count)     (Type*)MemoryStream_Peek(Stream, (Count) * sizeof(Type))

#define CONSUME(Stream, Type)               (Type*)MemoryStream_Consume(Stream, sizeof(Type))
#define CONSUME_ARRAY(Stream, Type, Count)  (Type*)MemoryStream_Consume(Stream, (Count) * sizeof(Type))

#define EMIT(Stream, Value)                                         \
    {                                                               \
        void* Dst_ = MemoryStream_Consume(Stream, sizeof(Value));   \
        CopyMemory(Dst_, &(Value), sizeof(Value));                  \
    }

#define EMIT_ARRAY(Stream, Array, Count)                                \
    {                                                                   \
        const memory_size ByteCount_ = (Count) * sizeof((Array)[0]);    \
        void* Dst_ = MemoryStream_Consume(Stream, ByteCount_);          \
        CopyMemory(Dst_, Array, ByteCount_);                            \
    }
