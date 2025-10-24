// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_math.h"
#include "pvz_memory.h"
#include "pvz_platform.h"
#include "pvz_renderer.h"

[[noreturn]] function void
Platform_Panic(const char* Message)
{
    MessageBoxA(NULL, Message, "PVZ-Remake has crashed!", MB_OK);
}

//====================================================================================================================//
//---------------------------------------------------- TASK QUEUE ----------------------------------------------------//
//====================================================================================================================//

struct win32_task_thread_info
{
    HANDLE                  Handle;
    DWORD                   PlatformID;
    platform_task_queue*    TaskQueue;
    u32                     LogicalIndex;
};

enum win32_task_entry_state : u8
{
    WIN32_TASK_ENTRY_STATE_AVAILABLE = 0,
    WIN32_TASK_ENTRY_STATE_PENDING_EXECUTION,
    WIN32_TASK_ENTRY_STATE_IN_EXECUTION,
};

struct win32_task_entry
{
    win32_task_entry_state  State;
    platform_task_pfn       TaskFunction;
    void*                   UserData;
};

struct platform_task_queue
{
    HANDLE                  TasksAvailableSemaphore;
    u32                     ThreadCount;
    win32_task_thread_info* Threads;
    u32                     EntryCount;
    win32_task_entry*       Entries;
    volatile LONG64         NextEntryIndexToDispatch;
    volatile LONG64         CurrentEntryIndex;
    volatile LONG           UnfinishedTaskCount;
};

enum win32_dispatch_task_result : u8
{
    WIN32_DISPATCH_TASK_RESULT_DISPATCHED,
    WIN32_DISPATCH_TASK_RESULT_NO_AVAILABLE_TASKS,
    WIN32_DISPATCH_TASK_RESULT_INDEX_ALREADY_ACQUIRED,
};

internal win32_dispatch_task_result
Win32_DispatchNextTask(platform_task_queue* TaskQueue, s32 ThreadLogicalIndex)
{
    win32_dispatch_task_result DispatchResult;
    const u64 EntryIndexToDispatch = TaskQueue->NextEntryIndexToDispatch;
    
    if (EntryIndexToDispatch < TaskQueue->CurrentEntryIndex)
    {
        const u64 ActualEntryIndex = InterlockedCompareExchange64(&TaskQueue->NextEntryIndexToDispatch,
                                                                  TaskQueue->NextEntryIndexToDispatch + 1,
                                                                  EntryIndexToDispatch);
        if (EntryIndexToDispatch == ActualEntryIndex)
        {
            // NOTE(Traian): This thread has acquired the entry with index 'EntryIndexToDispatch'.
            win32_task_entry* Task = TaskQueue->Entries + (EntryIndexToDispatch % TaskQueue->EntryCount);
            ASSERT(Task->State == WIN32_TASK_ENTRY_STATE_PENDING_EXECUTION);
            Task->State = WIN32_TASK_ENTRY_STATE_IN_EXECUTION;
            
            if (Task->TaskFunction)
            {
                Task->TaskFunction(ThreadLogicalIndex, Task->UserData);
            }
            
            Task->State = WIN32_TASK_ENTRY_STATE_AVAILABLE;
            InterlockedDecrement(&TaskQueue->UnfinishedTaskCount);
            
            DispatchResult = WIN32_DISPATCH_TASK_RESULT_DISPATCHED;
        }
        else
        {
            DispatchResult = WIN32_DISPATCH_TASK_RESULT_INDEX_ALREADY_ACQUIRED;
        }
    }
    else
    {
        DispatchResult = WIN32_DISPATCH_TASK_RESULT_NO_AVAILABLE_TASKS;
    }

    return DispatchResult;
}

internal DWORD CALLBACK
Win32_TaskQueueThreadProcedure(void* OpaqueThreadInfo)
{
    win32_task_thread_info* ThreadInfo = (win32_task_thread_info*)OpaqueThreadInfo;
    platform_task_queue* TaskQueue = ThreadInfo->TaskQueue;
    
    while (true)
    {
        // NOTE(Traian): Try to dispatch tasks until none are available.
        while (Win32_DispatchNextTask(TaskQueue, ThreadInfo->LogicalIndex) != WIN32_DISPATCH_TASK_RESULT_NO_AVAILABLE_TASKS);
        
        // NOTE(Traian): No tasks are currently available. Instead of busy-waiting and wasting the system resources,
        // put this thread to "sleep" until the tasks available semaphore is signaled.
        WaitForSingleObject(TaskQueue->TasksAvailableSemaphore, INFINITE);
    }

    return 0;
}

internal void
Win32_PlatformTaskQueue_Initialize(platform_task_queue* TaskQueue, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(TaskQueue);

    //
    // NOTE(Traian): Allocate the task entries.
    //

    TaskQueue->EntryCount = 32;
    TaskQueue->Entries = PUSH_ARRAY(Arena, win32_task_entry, TaskQueue->EntryCount);

    //
    // NOTE(Traian): Determine the number of threads and create their handles.
    //

    SYSTEM_INFO SystemInfo = {};
    GetSystemInfo(&SystemInfo);
    const u32 HardwareThreadCount = SystemInfo.dwNumberOfProcessors;

    const f32 MAX_SYSTEM_USAGE_PERCENTAGE = 0.8F;
    // NOTE(Traian): One thread is the main thread, which should not be included the the thread pool of the task queue.
    TaskQueue->ThreadCount = Max(1, (u32)(HardwareThreadCount * MAX_SYSTEM_USAGE_PERCENTAGE));
    TaskQueue->Threads = PUSH_ARRAY(Arena, win32_task_thread_info, TaskQueue->ThreadCount);
    for (u32 ThreadIndex = 0; ThreadIndex < TaskQueue->ThreadCount; ++ThreadIndex)
    {
        win32_task_thread_info* ThreadInfo = TaskQueue->Threads + ThreadIndex;
        ThreadInfo->Handle = CreateThread(NULL, MEGABYTES(1), Win32_TaskQueueThreadProcedure, ThreadInfo,
                                          CREATE_SUSPENDED, &ThreadInfo->PlatformID);
        if (ThreadInfo->Handle == NULL)
        {
            PANIC("Failed to create the Win32 thread for the platform task queue!");
        }

        ThreadInfo->TaskQueue = TaskQueue;
        ThreadInfo->LogicalIndex = ThreadIndex;

        ResumeThread(ThreadInfo->Handle);
    }

    //
    // NOTE(Traian): Initialize the task queue counters.
    //

    TaskQueue->NextEntryIndexToDispatch = 0;
    TaskQueue->CurrentEntryIndex = 0;
    TaskQueue->UnfinishedTaskCount = 0;

    TaskQueue->TasksAvailableSemaphore = CreateSemaphore(NULL, 0, TaskQueue->EntryCount, NULL);
    if (TaskQueue->TasksAvailableSemaphore == NULL)
    {
        PANIC("Failed to create the Win32 semaphore that orchestrates the platform task queue!");
    }
}

function void
PlatformTaskQueue_Push(platform_task_queue* TaskQueue, platform_task_pfn TaskFunction, void* UserData)
{
    const u32 EntryIndex = TaskQueue->CurrentEntryIndex % TaskQueue->EntryCount;
    win32_task_entry* Entry = TaskQueue->Entries + EntryIndex;
    ASSERT(Entry->State == WIN32_TASK_ENTRY_STATE_AVAILABLE);
    
    Entry->State = WIN32_TASK_ENTRY_STATE_PENDING_EXECUTION;
    Entry->TaskFunction = TaskFunction;
    Entry->UserData = UserData;
    
    TaskQueue->CurrentEntryIndex++;
    ReleaseSemaphore(TaskQueue->TasksAvailableSemaphore, 1, NULL);
    
    InterlockedIncrement(&TaskQueue->UnfinishedTaskCount);
}

function void
PlatformTaskQueue_WaitForAll(platform_task_queue* TaskQueue)
{
    // NOTE(Traian): Convert this thread to a worker thread and don't exit this
    // loop until there are no pending-execution tasks.
    while (Win32_DispatchNextTask(TaskQueue, -1) != WIN32_DISPATCH_TASK_RESULT_NO_AVAILABLE_TASKS) {}
    
    // NOTE(Traian): With the current way the async task queue is implemented, some busy-waiting will happen,
    // as long as we don't introduce other synchronization primitives.
    while (TaskQueue->UnfinishedTaskCount > 0) {}
}


//====================================================================================================================//
//----------------------------------------------------- FILE API -----------------------------------------------------//
//====================================================================================================================//

struct win32_file_descriptor
{
    HANDLE                  FileHandle;
    memory_size             FileSize;
    platform_file_access    Access;
};
static_assert(sizeof(win32_file_descriptor) <= sizeof(platform_file_handle));

internal inline platform_file_handle
Win32_GetFileHandleFromDescriptor(win32_file_descriptor Descriptor)
{
    platform_file_handle Result = {};
    CopyMemory(&Result, &Descriptor, sizeof(win32_file_descriptor));
    return Result;
}

internal inline win32_file_descriptor
Win32_GetDescriptorFromFileHandle(platform_file_handle FileHandle)
{
    win32_file_descriptor Result = {};
    CopyMemory(&Result, &FileHandle, sizeof(win32_file_descriptor));
    return Result;
}

function platform_file_handle
Platform_OpenFile(const char* FileName, platform_file_access Access, b8 CreateIfMissing, b8 Truncate)
{
    DWORD DesiredAccess = 0;
    if (Access & PLATFORM_FILE_ACCESS_READ)  { DesiredAccess |= GENERIC_READ; }
    if (Access & PLATFORM_FILE_ACCESS_WRITE) { DesiredAccess |= GENERIC_WRITE; }
    
    DWORD FileShareMode = 0;
    if (Access & PLATFORM_FILE_ACCESS_READ)  { FileShareMode = FILE_SHARE_READ; }
    if (Access & PLATFORM_FILE_ACCESS_WRITE) { FileShareMode = 0; }

    DWORD CreationDisposition;
    if (CreateIfMissing)
    {
        if (Truncate)
        {
            CreationDisposition = CREATE_ALWAYS;
        }
        else
        {
            CreationDisposition = OPEN_ALWAYS;
        }
    }
    else
    {
        if (Truncate)
        {
            CreationDisposition = TRUNCATE_EXISTING;
        }
        else
        {
            CreationDisposition = OPEN_EXISTING;
        }
    }

    platform_file_handle ResultHandle = {};
    HANDLE FileHandle = CreateFileA(FileName, DesiredAccess, FileShareMode,
                                    NULL, CreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (FileHandle)
    {
        LARGE_INTEGER FileSizeValue = {};
        if (GetFileSizeEx(FileHandle, &FileSizeValue))
        {
            win32_file_descriptor FileDescriptor = {};
            FileDescriptor.FileHandle = FileHandle;
            FileDescriptor.FileSize = FileSizeValue.QuadPart;
            FileDescriptor.Access = Access;
            ResultHandle = Win32_GetFileHandleFromDescriptor(FileDescriptor);
        }
        else
        {
            CloseHandle(FileHandle);
        }
    }

    return ResultHandle;
}

function void
Platform_CloseFile(platform_file_handle FileHandle)
{
    win32_file_descriptor FileDescriptor = Win32_GetDescriptorFromFileHandle(FileHandle);
    CloseHandle(FileDescriptor.FileHandle);
}

function memory_size
Platform_GetFileSize(platform_file_handle FileHandle)
{
    win32_file_descriptor FileDescriptor = Win32_GetDescriptorFromFileHandle(FileHandle);
    const memory_size Result = FileDescriptor.FileSize;
    return Result;
}

function platform_read_file_result
Platform_ReadEntireFile(platform_file_handle FileHandle, memory_arena* Arena)
{
    win32_file_descriptor FileDescriptor = Win32_GetDescriptorFromFileHandle(FileHandle);
    const platform_read_file_result Result = Platform_ReadFromFile(FileHandle, 0, FileDescriptor.FileSize, Arena);
    return Result;
}

function platform_read_file_result
Platform_ReadFromFile(platform_file_handle FileHandle,
                      memory_size ReadOffset, memory_size ReadByteCount,
                      memory_arena* Arena)
{
    win32_file_descriptor FileDescriptor = Win32_GetDescriptorFromFileHandle(FileHandle);
    platform_read_file_result Result = {};
    Result.IsValid = false;

    if (ReadOffset + ReadByteCount <= FileDescriptor.FileSize)
    {
        // NOTE(Traian): This temporary arena is more used as a marker - if all read operations are successful, then
        // this temporary arena is never ended. Otherwise, 'MemoryArena_EndTemporary' is called at the end of the function.
        memory_temporary_arena ReadArena = MemoryArena_BeginTemporary(Arena);

        if (Platform_IsFileHandleValid(FileHandle) && FileDescriptor.FileHandle != NULL)
        {
            memory_size RemainingNumberOfBytes = ReadByteCount;
            void* FirstReadData = NULL;

            while (RemainingNumberOfBytes > 0)
            {
                // NOTE(Traian): The Win32 API only allows for reading a maximum number of bytes that fits inside a 32-bit
                // variable (DWORD), and thus we determine how many bytes to read this pass.
                const DWORD NumberOfBytesToRead = (DWORD)(RemainingNumberOfBytes & 0x00000000FFFFFFFF);
                DWORD NumberOfBytesRead = 0;

                // NOTE(Traian): Allocate memory from the provided arena to store the data read from the file. The first
                // time we read from the file, we align the allocation the same as a pointer - subsequent allocations
                // effectively have no alignment requirements
                void* ReadData = MemoryArena_Allocate(ReadArena.Arena, NumberOfBytesToRead,
                                                      (FirstReadData == NULL) ? sizeof(void*) : 1);

                // NOTE(Traian): Determine the offset from where to read from the file.
                OVERLAPPED Overlapped = {};
                Overlapped.Offset       = (DWORD)(((ReadOffset + (ReadByteCount - RemainingNumberOfBytes)) & 0x00000000FFFFFFFF) >> 0);
                Overlapped.OffsetHigh   = (DWORD)(((ReadOffset + (ReadByteCount - RemainingNumberOfBytes)) & 0xFFFFFFFF00000000) >> 32);

                // NOTE(Traian): Read from the file using the Win32 API.
                if (ReadFile(FileDescriptor.FileHandle, ReadData, NumberOfBytesToRead, &NumberOfBytesRead, &Overlapped) &&
                    (NumberOfBytesToRead == NumberOfBytesRead))
                {
                    RemainingNumberOfBytes -= NumberOfBytesRead;
                    if (FirstReadData == NULL)
                    {
                        FirstReadData = ReadData;
                    }
                }
                else
                {
                    break;
                }
            }

            if (RemainingNumberOfBytes == 0)
            {
                // NOTE(Traian): If there are no more bytes left to read, the operation is considered successful.
                Result.IsValid = true;
                Result.ReadData = FirstReadData;
                Result.ReadByteCount = ReadByteCount;
            }
            else
            {
                // NOTE(Traian): If there are bytes left to read, one of the read operation has failed.
                MemoryArena_EndTemporary(&ReadArena);
            }
        }
    }

    return Result;
}

//====================================================================================================================//
//----------------------------------------------------- GAME LOOP ----------------------------------------------------//
//====================================================================================================================//

internal u64
Win32_GetPerformanceCounter()
{
    LARGE_INTEGER PerformanceCounter = {};
    if (QueryPerformanceCounter(&PerformanceCounter))
    {
        const u64 Result = PerformanceCounter.QuadPart;
        return Result;
    }
    else
    {
        PANIC("Failed to query the performance counter!");
    }
}

function void
Platform_SeedRandomSeries(random_series* Series)
{
    // NOTE(Traian): Use low bits as seed, high bits as sequence for extra entropy.
    const u64 Counter = Win32_GetPerformanceCounter();
    const u64 Seed = (u64)(Counter >> 0);
    const u64 Sequence = (u64)(Counter >> 32);
    Random_InitializeSeries(Series, Seed, Sequence);
}

internal u64
Win32_GetPerformanceCounterFrequency()
{
    LARGE_INTEGER PerformanceFrequency = {};
    if (QueryPerformanceFrequency(&PerformanceFrequency))
    {
        const u64 Result = PerformanceFrequency.QuadPart;
        return Result;
    }
    else
    {
        PANIC("Failed to query the performance frequency!");
    }
}

struct win32_offscreen_bitmap
{
    renderer_image  Image;
    BITMAPINFO      Info;
};

internal void
Win32_GetWindowClientSize(HWND WindowHandle, u32* OutSizeX, u32* OutSizeY)
{
    RECT ClientRect;
    if (GetClientRect(WindowHandle, &ClientRect))
    {
        *OutSizeX = ClientRect.right - ClientRect.left;
        *OutSizeY = ClientRect.bottom - ClientRect.top;
    }
    else
    {
        *OutSizeX = 0;
        *OutSizeY = 0;
    }
}

internal void
Win32_ReallocateOffscreenBitmap(win32_offscreen_bitmap* Bitmap, HWND WindowHandle)
{
    if (Bitmap->Image.PixelBuffer)
    {
        VirtualFree(Bitmap->Image.PixelBuffer, 0, MEM_RELEASE);
    }
    ZERO_STRUCT_POINTER(Bitmap);

    u32 WindowSizeX;
    u32 WindowSizeY;
    Win32_GetWindowClientSize(WindowHandle, &WindowSizeX, &WindowSizeY);

    if (WindowSizeX > 0 && WindowSizeY > 0)
    {
        Bitmap->Image.SizeX = WindowSizeX;
        Bitmap->Image.SizeY = WindowSizeY;
        Bitmap->Image.Format = RENDERER_IMAGE_FORMAT_B8G8R8A8;
        const memory_size PixelBufferByteCount = Image_GetPixelBufferByteCount(Bitmap->Image.SizeX,
                                                                               Bitmap->Image.SizeY,
                                                                               Bitmap->Image.Format);
        Bitmap->Image.PixelBuffer = VirtualAlloc(NULL, PixelBufferByteCount, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        Bitmap->Info.bmiHeader.biSize = sizeof(Bitmap->Info.bmiHeader);
        Bitmap->Info.bmiHeader.biWidth = Bitmap->Image.SizeX;
        Bitmap->Info.bmiHeader.biHeight = Bitmap->Image.SizeY;
        Bitmap->Info.bmiHeader.biPlanes = 1;
        Bitmap->Info.bmiHeader.biBitCount = 8 * Image_GetBytesPerPixelForFormat(Bitmap->Image.Format);
    }
}

internal void
Win32_PresentOffscreenBitmap(const win32_offscreen_bitmap* Bitmap, HWND WindowHandle, HDC WindowDeviceContext)
{
    u32 WindowSizeX;
    u32 WindowSizeY;
    Win32_GetWindowClientSize(WindowHandle, &WindowSizeX, &WindowSizeY);

    StretchDIBits(WindowDeviceContext, 0, 0, WindowSizeX, WindowSizeY,
                  0, 0, Bitmap->Image.SizeX, Bitmap->Image.SizeY,
                  Bitmap->Image.PixelBuffer, &Bitmap->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

internal b8                             GameIsRunning;
internal platform_game_input_state      GameInputState;

internal inline game_input_key
Win32_TranslateVirtualKeyCodeToGameInputKey(int VirtualKeyCode)
{
    switch (VirtualKeyCode)
    {
        case VK_ESCAPE: return GAME_INPUT_KEY_ESCAPE;
        case VK_F1: return GAME_INPUT_KEY_F1;
        case VK_F2: return GAME_INPUT_KEY_F2;
        case VK_F3: return GAME_INPUT_KEY_F3;
    }
    return GAME_INPUT_KEY_NONE;
}

internal LRESULT CALLBACK
Win32_WindowProcedure(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message)
    {
        case WM_CLOSE:
        case WM_QUIT:
        {
            GameIsRunning = false;
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            platform_input_key_state* KeyState = &GameInputState.Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON];
            if (!KeyState->IsDown)
            {
                KeyState->IsDown = true;
                KeyState->WasPressedThisFrame = true;
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            platform_input_key_state* KeyState = &GameInputState.Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON];
            if (KeyState->IsDown)
            {
                KeyState->IsDown = false;
                KeyState->WasReleasedThisFrame = true;
            }
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            const int VirtualKeyCode = (int)WParam;
            const game_input_key InputKey = Win32_TranslateVirtualKeyCodeToGameInputKey(VirtualKeyCode);
            if (InputKey != GAME_INPUT_KEY_NONE)
            {
                platform_input_key_state* KeyState = &GameInputState.Keys[InputKey];
                if (!KeyState->IsDown)
                {
                    KeyState->IsDown = true;
                    KeyState->WasPressedThisFrame = true;
                }
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            const int VirtualKeyCode = (int)WParam;
            const game_input_key InputKey = Win32_TranslateVirtualKeyCodeToGameInputKey(VirtualKeyCode);
            if (InputKey != GAME_INPUT_KEY_NONE)
            {
                platform_input_key_state* KeyState = &GameInputState.Keys[InputKey];
                if (KeyState->IsDown)
                {
                    KeyState->IsDown = false;
                    KeyState->WasReleasedThisFrame = true;
                }
            }
            return 0;
        }
    }

    return DefWindowProcA(WindowHandle, Message, WParam, LParam);
}

internal void
Win32_ResetInputKeyStates()
{
    for (u8 InputKey = 0; InputKey < GAME_INPUT_KEY_MAX_COUNT; ++InputKey)
    {
        platform_input_key_state* KeyState = &GameInputState.Keys[InputKey];
        KeyState->WasPressedThisFrame = false;
        KeyState->WasReleasedThisFrame = false;
    }
}

internal void
Win32_UpdateMousePosition(HWND WindowHandle)
{
    POINT ScreenMousePosition;
    if (GetCursorPos(&ScreenMousePosition))
    {
        POINT ClientMousePosition = ScreenMousePosition;
        if (ScreenToClient(WindowHandle, &ClientMousePosition))
        {
            u32 WindowSizeX;
            u32 WindowSizeY;
            Win32_GetWindowClientSize(WindowHandle, &WindowSizeX, &WindowSizeY);

            if (WindowSizeX > 0 && WindowSizeY > 0)
            {
                const f32 MousePositionX = Clamp((f32)ClientMousePosition.x / (f32)WindowSizeX, 0.0F, 1.0F);
                const f32 MousePositionY = Clamp((f32)((s32)WindowSizeY - ClientMousePosition.y) / (f32)WindowSizeY, 0.0F, 1.0F);

                GameInputState.MouseDeltaX = MousePositionX - GameInputState.MousePositionX;
                GameInputState.MouseDeltaY = MousePositionY - GameInputState.MousePositionY;
                GameInputState.MousePositionX = MousePositionX;
                GameInputState.MousePositionY = MousePositionY;
            }
            else
            {
                GameInputState.MouseDeltaX = 0.0F;
                GameInputState.MouseDeltaY = 0.0F;
                GameInputState.MousePositionX = 0.0F;
                GameInputState.MousePositionY = 0.0F;
            }
        }
        else
        {
            PANIC("Failed to convert from screen coordinates to client coordinates!");
        }
    }
    else
    {
        PANIC("Failed to query to screen mouse position!");
    }
}

function INT
WinMain(HINSTANCE Instance, HINSTANCE PreviousInstance, LPSTR CommandLine, INT ShowCommand)
{
    WNDCLASS WindowClass = {};
    WindowClass.lpfnWndProc = Win32_WindowProcedure;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "PVZRemakeWindowClass";
    if (!RegisterClassA(&WindowClass))
    {
        PANIC("Failed to register the class window!");
    }

    HWND WindowHandle = CreateWindowA("PVZRemakeWindowClass", "PvZ-Remake", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                      NULL, NULL, Instance, NULL);
    if (WindowHandle)
    {
        HDC WindowDeviceContext = GetDC(WindowHandle);
        win32_offscreen_bitmap OffscreenBitmap = {};

        // NOTE(Traian): Allocate the game memory.
        memory_arena PermanentArena = {};
        PermanentArena.ByteCount = MEGABYTES(1);
        PermanentArena.MemoryBlock = VirtualAlloc(NULL, PermanentArena.ByteCount,
                                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        memory_arena TransientArena = {};
        TransientArena.ByteCount = MEGABYTES(64);
        TransientArena.MemoryBlock = VirtualAlloc(NULL, TransientArena.ByteCount,
                                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        platform_game_memory GameMemory = {};
        GameMemory.PermanentArena = &PermanentArena;
        GameMemory.TransientArena = &TransientArena;

        // NOTE(Traian): Create the platform task queue.
        platform_task_queue TaskQueue = {};
        Win32_PlatformTaskQueue_Initialize(&TaskQueue, GameMemory.PermanentArena);

        // NOTE(Traian): Initialize the game layer.
        game_state* GameState = Game_Initialize(&GameMemory);

        // NOTE(Traian): Initialize frame timers.
        const u64 PerformanceCounterFrequency = Win32_GetPerformanceCounterFrequency();
        u64 LastFramePerformanceCounter = Win32_GetPerformanceCounter();
        f32 LastFrameDeltaTime = 1.0F / 60.0F;

        GameIsRunning = true;
        while (GameIsRunning)
        {
            // NOTE(Traian): Process the window message queue and update input.
            Win32_ResetInputKeyStates();
            MSG Message = {};
            while (PeekMessageA(&Message, WindowHandle, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            }
            Win32_UpdateMousePosition(WindowHandle);

            // NOTE(Traian): Rallocate the offscreen bitmap if the size of the window client area has changed.
            u32 WindowSizeX;
            u32 WindowSizeY;
            Win32_GetWindowClientSize(WindowHandle, &WindowSizeX, &WindowSizeY);
            if (OffscreenBitmap.Image.SizeX != WindowSizeX || OffscreenBitmap.Image.SizeY != WindowSizeY)
            {
                if (WindowSizeX > 0 && WindowSizeY > 0)
                {
                    Win32_ReallocateOffscreenBitmap(&OffscreenBitmap, WindowHandle);
                }
                else
                {
                    // NOTE(Traian): Reset the input state when the window gets minimized.
                    Win32_ResetInputKeyStates();
                }
            }

            // NOTE(Traian): Update and render the game.
            game_platform_state PlatformState = {};
            PlatformState.Memory = &GameMemory;
            PlatformState.Input = &GameInputState;
            PlatformState.TaskQueue = &TaskQueue;
            PlatformState.RenderTarget = &OffscreenBitmap.Image;
            Game_UpdateAndRender(GameState, &PlatformState, LastFrameDeltaTime);

            // NOTE(Traian): Present the offscreen bitmap to the window back-buffer.
            Win32_PresentOffscreenBitmap(&OffscreenBitmap, WindowHandle, WindowDeviceContext);

            // NOTE(Traian): Update the timers.
            const u64 CurrentPerformanceCounter = Win32_GetPerformanceCounter();
            const u64 DeltaPerformanceCounter = CurrentPerformanceCounter - LastFramePerformanceCounter;
            LastFramePerformanceCounter = CurrentPerformanceCounter;
            LastFrameDeltaTime = (f32)((f64)DeltaPerformanceCounter / (f64)PerformanceCounterFrequency);
        }
    }
    else
    {
        PANIC("Failed to create the game window!");
    }

    return 0;
}
