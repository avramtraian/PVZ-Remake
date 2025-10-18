// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_memory.h"

//====================================================================================================================//
//--------------------------------------------------- MEMORY ARENA ---------------------------------------------------//
//====================================================================================================================//

function void*
MemoryArena_Allocate(memory_arena* Arena, memory_size AllocationSize, memory_size AllocationAlignment)
{
    void* BaseAllocationAddress = (u8*)Arena->MemoryBlock + Arena->AllocatedByteCount;
    memory_size AlignmentOffset = AllocationAlignment - ((memory_index)BaseAllocationAddress % AllocationAlignment);
    if (AlignmentOffset == AllocationAlignment)
    {
        // NOTE(Traian): No alignment is required.
        AlignmentOffset = 0;
    }

    const memory_size TotalAllocationSize = AlignmentOffset + AllocationSize;
    if (Arena->AllocatedByteCount + TotalAllocationSize <= Arena->ByteCount)
    {
        Arena->AllocatedByteCount += TotalAllocationSize;
        return (u8*)BaseAllocationAddress + AlignmentOffset;
    }
    else
    {
        PANIC("Out-of-memory when trying to allocate from a memory arena!");
    }
}

function void
MemoryArena_Reset(memory_arena* Arena)
{
    ZeroMemory(Arena->MemoryBlock, Arena->AllocatedByteCount);
    Arena->AllocatedByteCount = 0;
}

function memory_temporary_arena
MemoryArena_BeginTemporary(memory_arena* Arena)
{
	memory_temporary_arena TemporaryArena = {};
	TemporaryArena.Arena = Arena;
	TemporaryArena.BaseAllocatedByteCount = Arena->AllocatedByteCount;
	return TemporaryArena;
}

function void
MemoryArena_EndTemporary(memory_temporary_arena* TemporaryArena)
{
	if (TemporaryArena->Arena)
	{
		memory_arena* Arena = TemporaryArena->Arena;
		ASSERT(TemporaryArena->BaseAllocatedByteCount <= Arena->AllocatedByteCount);
		ZeroMemory((u8*)Arena->MemoryBlock + TemporaryArena->BaseAllocatedByteCount,
				   Arena->AllocatedByteCount - TemporaryArena->BaseAllocatedByteCount);
		Arena->AllocatedByteCount = TemporaryArena->BaseAllocatedByteCount;
	}

	ZERO_STRUCT_POINTER(TemporaryArena);
}

//====================================================================================================================//
//--------------------------------------------------- MEMORY STREAM --------------------------------------------------//
//====================================================================================================================//

function void
MemoryStream_Initialize(memory_stream* Stream, void* MemoryBlock, memory_size ByteCount)
{
    ZERO_STRUCT_POINTER(Stream);
    Stream->MemoryBlock = MemoryBlock;
    Stream->ByteCount = ByteCount;
    Stream->ByteOffset = 0;
}

function void
MemoryStream_Reset(memory_stream* Stream)
{
    Stream->ByteOffset = 0;
}

function void*
MemoryStream_Consume(memory_stream* Stream, memory_size ByteCount)
{
    if (Stream->ByteOffset + ByteCount <= Stream->ByteCount)
    {
        void* Result = (u8*)Stream->MemoryBlock + Stream->ByteOffset;
        Stream->ByteOffset += ByteCount;
        return Result;
    }
    else
    {
        PANIC("Buffer overflow when consuming from a memory stream!");
    }
}

function void*
MemoryStream_TryConsume(memory_stream* Stream, memory_size ByteCount)
{
    void* Result = NULL;
    if (Stream->ByteOffset + ByteCount <= Stream->ByteCount)
    {
        void* Result = (u8*)Stream->MemoryBlock + Stream->ByteOffset;
        Stream->ByteOffset += ByteCount;
    }
    return Result;
}

function void*
MemoryStream_Peek(memory_stream* Stream, memory_size ByteCount)
{
    if (Stream->ByteOffset + ByteCount <= Stream->ByteCount)
    {
        void* Result = (u8*)Stream->MemoryBlock + Stream->ByteOffset;
        return Result;
    }
    else
    {
        PANIC("Buffer overflow when peeking from a memory stream!");
    }
}
