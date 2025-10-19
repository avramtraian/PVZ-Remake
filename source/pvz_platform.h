// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

//====================================================================================================================//
//------------------------------------------------- DEFINES AND TYPES ------------------------------------------------//
//====================================================================================================================//

#ifdef PVZ_WINDOWS
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif // PVZ_WINDOWS

// TODO(Traian): Use our custom format function instead of 'snprintf'!
#include <stdio.h>

#define function
#define internal            static
#define local_persistent    static

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using s8 = signed char;
using s16 = signed short;
using s32 = signed int;
using s64 = signed long long;
using f32 = float;
using f64 = double;
using b8 = bool;
using b32 = int;
using usize = u64;
using ssize = s64;
using memory_index = u64;
using memory_size = u64;

#ifdef PVZ_WINDOWS
    #define ASSERT(...)         if (!(__VA_ARGS__)) { __debugbreak(); }
    #define ASSERT_NOT_REACHED  { __debugbreak(); }
#else
    #define ASSERT(...)
    #define ASSERT_NOT_REACHED
#endif // PVZ_WINDOWS

[[noreturn]] function void Platform_Panic(const char* Message);
#define PANIC(...) { Platform_Panic(__VA_ARGS__); __debugbreak(); }

#if defined(PVZ_INTERNAL) && defined(PVZ_WINDOWS)
    #define INTERNAL_LOG(...)                                               \
        {                                                                   \
            char MessageBuffer[512] = {};                                   \
            snprintf(MessageBuffer, sizeof(MessageBuffer), __VA_ARGS__);    \
            OutputDebugStringA(MessageBuffer);                              \
        }
#else
    #define INTERNAL_LOG(...)
#endif // PVZ_INTERNAL

//====================================================================================================================//
//---------------------------------------------------- TASK QUEUE ----------------------------------------------------//
//====================================================================================================================//

struct platform_task_queue;

//
// NOTE(Traian): The first parameter represents the logical thread index where the task is executed. If the main
// thread (the one that calls 'PlatformTaskQueue_WaitForAll') is executing the task, then the index is set to '-1'.
// The second parameter represents the user data provided when 'PlatformTaskQueue_Push' was invoked.
//
using platform_task_pfn = void(*)(s32, void*);

function void                           PlatformTaskQueue_Push          (platform_task_queue* TaskQueue,
                                                                         platform_task_pfn TaskFunction,
                                                                         void* UserData);

function void                           PlatformTaskQueue_WaitForAll    (platform_task_queue* TaskQueue);

//====================================================================================================================//
//----------------------------------------------------- FILE API -----------------------------------------------------//
//====================================================================================================================//

struct platform_file_handle
{
    u64 Storage[4];
};

inline b8
Platform_IsFileHandleValid(platform_file_handle Handle)
{
    const b8 Result = (Handle.Storage[0] != 0) ||
                      (Handle.Storage[1] != 0) ||
                      (Handle.Storage[2] != 0) ||
                      (Handle.Storage[3] != 0);
    return Result;
}

enum platform_file_access : u8
{
    PLATFORM_FILE_ACCESS_NONE      = 0x00,
    PLATFORM_FILE_ACCESS_READ      = 0x01,
    PLATFORM_FILE_ACCESS_WRITE     = 0x02,
    PLATFORM_FILE_ACCESS_READWRITE = PLATFORM_FILE_ACCESS_READ | PLATFORM_FILE_ACCESS_WRITE,
};

struct platform_read_file_result
{
    b8          IsValid;
    void*       ReadData;
    memory_size ReadByteCount;
};

function platform_file_handle       Platform_OpenFile       (const char* FileName, platform_file_access Access,
                                                             b8 CreateIfMissing, b8 Truncate);

function void                       Platform_CloseFile      (platform_file_handle FileHandle);

function memory_size                Platform_GetFileSize    (platform_file_handle FileHandle);

function platform_read_file_result  Platform_ReadEntireFile (platform_file_handle FileHandle, struct memory_arena* Arena);

function platform_read_file_result  Platform_ReadFromFile   (platform_file_handle FileHandle,
                                                             memory_size ReadOffset, memory_size ReadByteCount,
                                                             struct memory_arena* Arena);

//====================================================================================================================//
//----------------------------------------------------- GAME LOOP ----------------------------------------------------//
//====================================================================================================================//

struct platform_game_memory
{
    struct memory_arena* PermanentArena;
    struct memory_arena* TransientArena;
};

enum game_input_key : u8
{
    GAME_INPUT_KEY_NONE = 0,
    GAME_INPUT_KEY_LEFT_MOUSE_BUTTON,
    GAME_INPUT_KEY_RIGHT_MOUSE_BUTTON,
    GAME_INPUT_KEY_ESCAPE,
    GAME_INPUT_KEY_F1,
    GAME_INPUT_KEY_F2,
    GAME_INPUT_KEY_F3,
    GAME_INPUT_KEY_MAX_COUNT,
};

struct platform_input_key_state
{
    b8 IsDown;
    b8 WasPressedThisFrame;
    b8 WasReleasedThisFrame;
};

struct platform_game_input_state
{
    f32                         MousePositionX;
    f32                         MousePositionY;
    f32                         MouseDeltaX;
    f32                         MouseDeltaY;
    platform_input_key_state    Keys[GAME_INPUT_KEY_MAX_COUNT];
};

struct game_platform_state
{
    platform_game_memory*       Memory;
    platform_game_input_state*  Input;
    platform_task_queue*        TaskQueue;
    struct renderer_image*      RenderTarget;
};

function struct game_state*     Game_Initialize         (platform_game_memory* GameMemory);

function void                   Game_UpdateAndRender    (struct game_state* GameState,
                                                         game_platform_state* PlatformState,
                                                         f32 DeltaTime);
