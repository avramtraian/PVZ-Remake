// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

#include "pvz_math.h"
#include "pvz_memory.h"
#include "pvz_platform.h"

//====================================================================================================================//
//------------------------------------------------------ TEXTURE -----------------------------------------------------//
//====================================================================================================================//

struct renderer_texture
{
    void*       PixelBuffer;
    memory_size BytesPerPixel;
    u32         SizeX;
    u32         SizeY;
};

function void           Texture_AllocateFromArena   (renderer_texture* Texture, memory_arena* Arena,
                                                     memory_size BytesPerPixel, u32 SizeX, u32 SizeY);

function void           Texture_CreateFromBMP       (renderer_texture* Texture, memory_arena* Arena,
                                                     const void* BMPBuffer, memory_size BMPBufferSize);

function void*          Texture_GetPixelAddress     (renderer_texture* Texture, u32 PixelIndexX, u32 PixelIndexY);

function const void*    Texture_GetPixelAddress     (const renderer_texture* Texture, u32 PixelIndexX, u32 PixelIndexY);

//====================================================================================================================//
//----------------------------------------------------- RENDERER -----------------------------------------------------//
//====================================================================================================================//

struct renderer_primitive
{
    u32     Index;
    vec2    MinPoint;
    vec2    MaxPoint;
    f32     ZOffset;
    color4  Color;
    vec2    MinUV;
    vec2    MaxUV;
    u32     TextureSlotIndex;
};

struct renderer_cluster
{
    u32                 MaxPrimitiveCount;
    u32                 CurrentPrimitiveIndex;
    renderer_primitive* Primitives;
    u32                 DrawRegionOffsetX;
    u32                 DrawRegionOffsetY;
    u32                 DrawRegionSizeX;
    u32                 DrawRegionSizeY;
};

struct renderer
{
    u32                 ClusterCount;
    renderer_cluster*   Clusters;
    u32                 MaxPrimitiveCount;
    u32                 CurrentPrimitiveIndex;
    renderer_primitive* Primitives;
    u32                 MaxTextureSlotCount;
    u32                 CurrentTextureSlotIndex;
    renderer_texture**  TextureSlots;
    u32                 ViewportSizeX;
    u32                 ViewportSizeY;
};

function void   Renderer_Initialize         (renderer* Renderer, memory_arena* Arena);

function void   Renderer_BeginFrame         (renderer* Renderer, u32 ViewportSizeX, u32 ViewportSizeY);

function void   Renderer_EndFrame           (renderer* Renderer);

function void   Renderer_PushPrimitive      (renderer* Renderer, vec2 MinPoint, vec2 MaxPoint, f32 ZOffset,
                                             color4 Color, vec2 MinUV = {}, vec2 MaxUV = {},
                                             renderer_texture* Texture = NULL);

function void   Renderer_DispatchClusters   (renderer* Renderer, renderer_texture* RenderTarget);
