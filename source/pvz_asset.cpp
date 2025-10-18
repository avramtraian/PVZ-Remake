// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_asset.h"

//====================================================================================================================//
//----------------------------------------------- ASSET DISK STRUCTURES ----------------------------------------------//
//====================================================================================================================//

internal void
Asset_ReadTextureFromStream(asset* Asset, memory_stream* AssetStream, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(Asset);

    const asset_header_texture* TextureHeader = CONSUME(AssetStream, asset_header_texture);
    const memory_size PixelBufferByteCount = (memory_size)TextureHeader->SizeX *
                                             (memory_size)TextureHeader->SizeY *
                                             TextureHeader->BytesPerPixel;
    void* PixelBuffer = MemoryStream_Consume(AssetStream, PixelBufferByteCount);
    
    Asset->Type = ASSET_TYPE_TEXTURE;
    Asset->Texture.RendererTexture.PixelBuffer = PixelBuffer;
    Asset->Texture.RendererTexture.SizeX = TextureHeader->SizeX;
    Asset->Texture.RendererTexture.SizeY = TextureHeader->SizeY;
    Asset->Texture.RendererTexture.BytesPerPixel = TextureHeader->BytesPerPixel;
}

internal void
Asset_ReadFontFromStream(asset* Asset, memory_stream* AssetStream, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(Asset);

    const asset_header_font* FontHeader = CONSUME(AssetStream, asset_header_font);
    asset_font_glyph* Glyphs = PUSH_ARRAY(Arena, asset_font_glyph, FontHeader->GlyphCount);
    for (u32 GlyphIndex = 0; GlyphIndex < FontHeader->GlyphCount; ++GlyphIndex)
    {
        const asset_font_glyph_header* GlyphHeader = CONSUME(AssetStream, asset_font_glyph_header);
        const memory_size GLYPH_TEXTURE_BPP = 1;
        const memory_size PixelBufferByteCount = (memory_size)GlyphHeader->TextureSizeX *
                                                 (memory_size)GlyphHeader->TextureSizeY *
                                                 GLYPH_TEXTURE_BPP;
        void* PixelBuffer = MemoryStream_Consume(AssetStream, PixelBufferByteCount);

        Glyphs[GlyphIndex].Codepoint = GlyphHeader->Codepoint;
        Glyphs[GlyphIndex].RendererTexture.PixelBuffer = PixelBuffer;
        Glyphs[GlyphIndex].RendererTexture.SizeX = GlyphHeader->TextureSizeX;
        Glyphs[GlyphIndex].RendererTexture.SizeY = GlyphHeader->TextureSizeY;
        Glyphs[GlyphIndex].RendererTexture.BytesPerPixel = GLYPH_TEXTURE_BPP;
    }

    Asset->Type = ASSET_TYPE_FONT;
    Asset->Font.Height = FontHeader->Height;
    Asset->Font.GlyphCount = FontHeader->GlyphCount;
    Asset->Font.Glyphs = Glyphs;
}

//====================================================================================================================//
//---------------------------------------------------- GAME ASSETS ---------------------------------------------------//
//====================================================================================================================//

function void
Asset_Initialize(game_assets* GameAssets, memory_arena* TransientArena, platform_file_handle AssetFileHandle)
{
    ZERO_STRUCT_POINTER(GameAssets);
    GameAssets->TransientArena = TransientArena;
    GameAssets->AssetFileHandle = AssetFileHandle;

    // NOTE(Traian): The memory used when reading from the asset file when decoding the asset pack header and the entry
    // headers is not required to be kept alive after the initialization process finishes, and thus we use a temporary
    // arena to allocate it. This temporary arena is 'ended' at the end of the function.
    memory_temporary_arena ReadAssetPackArena = MemoryArena_BeginTemporary(GameAssets->TransientArena);
    memory_stream Stream = {};

    //
    // NOTE(Traian): Read asset pack header.
    //

    platform_read_file_result ReadHeaderResult = Platform_ReadFromFile(GameAssets->AssetFileHandle,
                                                                       0, sizeof(asset_pack_header),
                                                                       ReadAssetPackArena.Arena);
    if (!ReadHeaderResult.IsValid)
    {
        PANIC("Failed to read (asset pack header) from the asset file!");
    }
    Stream.MemoryBlock = ReadHeaderResult.ReadData;
    Stream.ByteCount = ReadHeaderResult.ReadByteCount;
    const asset_pack_header AssetPackHeader = *CONSUME(&Stream, asset_pack_header);

    if (AssetPackHeader.MagicWord != PVZ_ASSET_PACK_MAGIC_WORD)
    {
        PANIC("The provided asset file doesn't start with the magic word and is most likely corrupted!");
    }

    //
    // NOTE(Traian): Read all asset pack entry headers.
    //

    platform_read_file_result ReadEntriesResult = Platform_ReadFromFile(GameAssets->AssetFileHandle,
                                                                        Stream.ByteCount,
                                                                        AssetPackHeader.EntryCount * sizeof(asset_pack_entry_header),
                                                                        ReadAssetPackArena.Arena);
    if (!ReadEntriesResult.IsValid)
    {
        PANIC("Failed to read (asset pack entry headers) from the asset file!");   
    }
    Stream.ByteCount += ReadEntriesResult.ReadByteCount;

    for (u32 EntryIndex = 0; EntryIndex < AssetPackHeader.EntryCount; ++EntryIndex)
    {
        //
        // NOTE(Traian): Read asset pack entry header.
        //

        const asset_pack_entry_header EntryHeader = *CONSUME(&Stream, asset_pack_entry_header);
        if (EntryHeader.Type == ASSET_TYPE_UNKNOWN)
        {
            PANIC("The provided asset file contains an asset of type 'ASSET_TYPE_UNKNOWN' and is most likely corrupted!");
        }
        if (EntryHeader.AssetID == GAME_ASSET_ID_NONE)
        {
            PANIC("The provided asset file contains an asset with ID 'GAME_ASSET_ID_NONE' and is most likely corrupted!");
        }
        if (EntryHeader.AssetID > GAME_ASSET_ID_MAX_COUNT)
        {
            PANIC("The provided asset file contains an asset with ID greater than 'GAME_ASSET_ID_MAX_COUNT' and is most likely corrupted!");
        }

        asset* Asset = &GameAssets->Assets[EntryHeader.AssetID];
        Asset->Type = EntryHeader.Type;
        Asset->AssetFileByteOffset = EntryHeader.ByteOffset;
        Asset->AssetFileByteCount = EntryHeader.ByteCount;
    }

    MemoryArena_EndTemporary(&ReadAssetPackArena);
}

function asset*
Asset_Get(game_assets* GameAssets, game_asset_id AssetID)
{
    ASSERT(AssetID != GAME_ASSET_ID_NONE);
    ASSERT(AssetID < GAME_ASSET_ID_MAX_COUNT);
    asset* Asset = &GameAssets->Assets[AssetID];
    return Asset;
}

function asset*
Asset_GetLoaded(game_assets* GameAssets, game_asset_id AssetID)
{
    asset* Asset = Asset_LoadAssetSync(GameAssets, AssetID);
    return Asset;
}

function asset_state
Asset_GetState(game_assets* GameAssets, game_asset_id AssetID)
{
    asset* Asset = Asset_Get(GameAssets, AssetID);
    const asset_state State = Asset->State;
    return State; 
}

function asset*
Asset_LoadAssetSync(game_assets* GameAssets, game_asset_id AssetID)
{
    asset* Asset = Asset_Get(GameAssets, AssetID);
    if (Asset->State == ASSET_STATE_UNLOADED)
    {
        //
        // NOTE(Traian): Load the asset from the asset file.
        //

        Asset->State = ASSET_STATE_LOADING;
        platform_read_file_result ReadAssetFileResult = Platform_ReadFromFile(GameAssets->AssetFileHandle,
                                                                              Asset->AssetFileByteOffset,
                                                                              Asset->AssetFileByteCount,
                                                                              GameAssets->TransientArena);
        if (ReadAssetFileResult.IsValid)
        {
            memory_stream AssetFileStream = {};
            AssetFileStream.MemoryBlock = ReadAssetFileResult.ReadData;
            AssetFileStream.ByteCount = ReadAssetFileResult.ReadByteCount;

            switch (Asset->Type)
            {
                case ASSET_TYPE_TEXTURE:
                {
                    Asset_ReadTextureFromStream(Asset, &AssetFileStream, GameAssets->TransientArena);
                }
                break;
                case ASSET_TYPE_FONT:
                {
                    Asset_ReadFontFromStream(Asset, &AssetFileStream, GameAssets->TransientArena);
                }
                break;
            }

            ASSERT(AssetFileStream.ByteOffset == AssetFileStream.ByteCount);
            Asset->State = ASSET_STATE_READY;
        }
    }
    else if (Asset->State == ASSET_STATE_LOADING)
    {
        // NOTE(Traian): Wait until the asset is marked as ready by the thread that currently loads it.
        // Unfortunately, without more complex synchronization mechanisms this thread will have to busy-wait.
        while (Asset->State != ASSET_STATE_READY) {}
    }

    return Asset;
}

function asset_state
Asset_LoadAssetAsync(game_assets* GameAssets, game_asset_id AssetID, platform_task_queue* TaskQueue)
{
    return ASSET_STATE_UNLOADED;
}
