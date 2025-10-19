// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#pragma once

#include "pvz_memory.h"
#include "pvz_platform.h"
#include "pvz_renderer.h"

//====================================================================================================================//
//---------------------------------------------- LOADED ASSET STRUCTURES ---------------------------------------------//
//====================================================================================================================//

enum asset_type : u8
{
    ASSET_TYPE_UNKNOWN = 0,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_FONT,
    ASSET_TYPE_MAX_COUNT,
};

enum asset_state : u8
{
    ASSET_STATE_UNLOADED = 0,
    ASSET_STATE_LOADING,
    ASSET_STATE_READY,
};

struct asset_texture
{
    renderer_texture RendererTexture;
};

struct asset_font_glyph
{
    u32                 Codepoint;
    renderer_texture    RendererTexture;
};

struct asset_font
{
    f32                 Height;
    u32                 GlyphCount;
    asset_font_glyph*   Glyphs;
};

struct asset
{
    asset_type          Type;
    asset_state         State;
    memory_size         AssetFileByteOffset;
    memory_size         AssetFileByteCount;
    union
    {
        asset_texture   Texture;
        asset_font      Font;
    };
};

//====================================================================================================================//
//----------------------------------------------- ASSET DISK STRUCTURES ----------------------------------------------//
//====================================================================================================================//

#define PVZ_MAKE_MAGIC_WORD(A, B, C, D) (u32)(((u8)(A) << 0) | ((u8)(B) << 8) | ((u8)(C) << 16) | ((u8)(D) << 24))
#define PVZ_ASSET_PACK_MAGIC_WORD PVZ_MAKE_MAGIC_WORD('P', 'Z', 'A', 'P')

struct asset_pack_header
{
    u32 MagicWord;
    u32 EntryCount;
};

struct asset_pack_entry_header
{
    u32         AssetID;
    asset_type  Type;
    memory_size ByteOffset;
    memory_size ByteCount;
};

struct asset_header_texture
{
    u32         SizeX;
    u32         SizeY;
    memory_size BytesPerPixel;
};

struct asset_header_font
{
    f32 Height;
    u32 GlyphCount;
};

struct asset_font_glyph_header
{
    u32 Codepoint;
    u32 TextureSizeX;
    u32 TextureSizeY;
};

//====================================================================================================================//
//---------------------------------------------------- GAME ASSETS ---------------------------------------------------//
//====================================================================================================================//

enum game_asset_id : u32
{
    GAME_ASSET_ID_NONE = 0,
    GAME_ASSET_ID_PLANT_SUNFLOWER,
    GAME_ASSET_ID_PLANT_PEASHOOTER,
    GAME_ASSET_ID_PROJECTILE_SUN,
    GAME_ASSET_ID_PROJECTILE_PEA,
    GAME_ASSET_ID_ZOMBIE_NORMAL,
    GAME_ASSET_ID_MAX_COUNT,
};

struct game_assets
{
    asset                   Assets[GAME_ASSET_ID_MAX_COUNT];
    memory_arena*           TransientArena;
    platform_file_handle    AssetFileHandle;
};

function void           Asset_Initialize    (game_assets* GameAssets, memory_arena* TransientArena,
                                             platform_file_handle AssetFileHandle);

function asset_state    Asset_GetState      (game_assets* GameAssets, game_asset_id AssetID);

function asset*         Asset_Get           (game_assets* GameAssets, game_asset_id AssetID);

function asset_state    Asset_LoadSync      (game_assets* GameAssets, game_asset_id AssetID);

function asset_state    Asset_LoadAsync     (game_assets* GameAssets, game_asset_id AssetID,
                                             platform_task_queue* TaskQueue);
