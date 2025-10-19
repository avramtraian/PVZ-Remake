// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

#include "pvz_math.h"
#include "pvz_memory.h"
#include "pvz_platform.h"

//====================================================================================================================//
//------------------------------------------------------- IMAGE ------------------------------------------------------//
//====================================================================================================================//

enum renderer_image_format : u8
{
    RENDERER_IMAGE_FORMAT_UNKNOWN = 0,
    RENDERER_IMAGE_FORMAT_A8,
    RENDERER_IMAGE_FORMAT_B8G8R8A8,
};

struct renderer_image
{
    u32                     SizeX;
    u32                     SizeY;
    renderer_image_format   Format;
    void*                   PixelBuffer;
};

function memory_size    Image_GetBytesPerPixelForFormat (renderer_image_format Format);

function memory_size    Image_GetPixelBufferByteCount   (u32 SizeX, u32 SizeY, renderer_image_format Format);

function void           Image_AllocateFromArena         (renderer_image* Image, memory_arena* Arena,
                                                         renderer_image_format Format, u32 SizeX, u32 SizeY);

function void*          Image_GetPixelAddress           (renderer_image* Image, u32 PixelIndexX, u32 PixelIndexY);

function const void*    Image_GetPixelAddress           (const renderer_image* Image, u32 PixelIndexX, u32 PixelIndexY);

function f32            Image_SampleBilinearA8          (const renderer_image* Image, vec2 UV);

function color4         Image_SampleBilinearB8G8R8A8    (const renderer_image* Image, vec2 UV);

//====================================================================================================================//
//------------------------------------------------------- IMAGE ------------------------------------------------------//
//====================================================================================================================//

struct renderer_texture
{
    u32                     SizeX;
    u32                     SizeY;
    renderer_image_format   Format;
    u32                     MaxMipCount;
    u32                     MipCount;
    renderer_image*         Mips;
};

function void           Texture_Create                  (renderer_texture* Texture, memory_arena* Arena,
                                                         const renderer_image* SourceImage, u32 MaxMipCount);

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

function void   Renderer_DispatchClusters   (renderer* Renderer, renderer_image* RenderTarget);
