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
    Asset->Type = ASSET_TYPE_TEXTURE;

    const asset_header_texture* TextureHeader = CONSUME(AssetStream, asset_header_texture);
    renderer_image Image = {};
    Image.SizeX = TextureHeader->SizeX;
    Image.SizeY = TextureHeader->SizeY;
    // TODO(Traian): Read the texture format from the asset file!
    Image.Format = RENDERER_IMAGE_FORMAT_B8G8R8A8;

    const memory_size PixelBufferByteCount = (memory_size)TextureHeader->SizeX *
                                             (memory_size)TextureHeader->SizeY *
                                             TextureHeader->BytesPerPixel;
    if (TextureHeader->BytesPerPixel == 4)
    {
        Image.PixelBuffer = CONSUME_ARRAY(AssetStream, u32, PixelBufferByteCount / sizeof(u32));
    }
    else
    {
        PANIC("Invalid texture BPP read from the asset file!");
    }

    Texture_Create(&Asset->Texture.RendererTexture, Arena, &Image, 6);
}

internal void
Asset_ReadFontFromStream(asset* Asset, memory_stream* AssetStream, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(Asset);
    Asset->Type = ASSET_TYPE_FONT;

    const asset_header_font* FontHeader = CONSUME(AssetStream, asset_header_font);
    Asset->Font.Height = FontHeader->Height;
    Asset->Font.Ascent = FontHeader->Ascent;
    Asset->Font.Descent = FontHeader->Descent;
    Asset->Font.LineGap = FontHeader->LineGap;
    Asset->Font.GlyphCount = FontHeader->GlyphCount;

    Asset->Font.Glyphs = PUSH_ARRAY(Arena, asset_font_glyph, Asset->Font.GlyphCount);
    Asset->Font.KerningTable = PUSH_ARRAY(Arena, s32, Asset->Font.GlyphCount * Asset->Font.GlyphCount);

    for (u32 GlyphIndex = 0; GlyphIndex < FontHeader->GlyphCount; ++GlyphIndex)
    {
        asset_font_glyph* Glyph = Asset->Font.Glyphs + GlyphIndex;
        const asset_font_glyph_header* GlyphHeader = CONSUME(AssetStream, asset_font_glyph_header);

        Glyph->Codepoint = GlyphHeader->Codepoint;
        Glyph->AdvanceWidth = GlyphHeader->AdvanceWidth;
        Glyph->LeftSideBearing = GlyphHeader->LeftSideBearing;
        Glyph->TextureOffsetX = GlyphHeader->TextureOffsetX;
        Glyph->TextureOffsetY = GlyphHeader->TextureOffsetY;

        const renderer_image_format GLYPH_IMAGE_FORMAT = RENDERER_IMAGE_FORMAT_A8;        
        renderer_image GlyphImage = {};
        GlyphImage.SizeX = GlyphHeader->TextureSizeX;
        GlyphImage.SizeY = GlyphHeader->TextureSizeY;
        GlyphImage.Format = GLYPH_IMAGE_FORMAT;

        const memory_size PixelBufferByteCount = (memory_size)GlyphHeader->TextureSizeX *
                                                 (memory_size)GlyphHeader->TextureSizeY *
                                                 Image_GetBytesPerPixelForFormat(GLYPH_IMAGE_FORMAT);
        GlyphImage.PixelBuffer = CONSUME_ARRAY(AssetStream, u8, PixelBufferByteCount / sizeof(u8));

        Texture_Create(&Glyph->RendererTexture, Arena, &GlyphImage, 4);
    }

    Asset->Font.KerningTable = CONSUME_ARRAY(AssetStream, s32, Asset->Font.GlyphCount * Asset->Font.GlyphCount);
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

function asset_state
Asset_GetState(game_assets* GameAssets, game_asset_id AssetID)
{
    ASSERT(AssetID != GAME_ASSET_ID_NONE);
    ASSERT(AssetID < GAME_ASSET_ID_MAX_COUNT);
    asset* Asset = &GameAssets->Assets[AssetID];
    const asset_state State = Asset->State;
    return State; 
}

function asset*
Asset_Get(game_assets* GameAssets, game_asset_id AssetID)
{
    Asset_LoadSync(GameAssets, AssetID);
    asset* Asset = &GameAssets->Assets[AssetID];
    return Asset;
}

function asset_state
Asset_LoadSync(game_assets* GameAssets, game_asset_id AssetID)
{
    ASSERT(AssetID != GAME_ASSET_ID_NONE);
    ASSERT(AssetID < GAME_ASSET_ID_MAX_COUNT);
    asset* Asset = &GameAssets->Assets[AssetID];
    const asset_state InitialAssetState = Asset->State;

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
            // NOTE(Traian): Emulate a single asset file stream that contains the entire asset file data at once.
            // This ensures that writing and reading from the asset file are equivalent from an alignment perspective.
            memory_stream AssetFileStream = {};
            AssetFileStream.MemoryBlock = (u8*)ReadAssetFileResult.ReadData - Asset->AssetFileByteOffset;
            AssetFileStream.ByteCount = Asset->AssetFileByteOffset + ReadAssetFileResult.ReadByteCount;
            AssetFileStream.ByteOffset = Asset->AssetFileByteOffset;

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

            if (AssetFileStream.ByteOffset != AssetFileStream.ByteCount)
            {
                PANIC("Loading an asset from the asset file didn't consume the entire memory block!");
            }
        }

        Asset->State = ASSET_STATE_READY;
    }
    else if (Asset->State == ASSET_STATE_LOADING)
    {
        // NOTE(Traian): Wait until the asset is marked as ready by the thread that currently loads it.
        // Unfortunately, without more complex synchronization mechanisms this thread will have to busy-wait.
        while (Asset->State != ASSET_STATE_READY) {}
    }

    return InitialAssetState;
}

function asset_state
Asset_LoadAsync(game_assets* GameAssets, game_asset_id AssetID, platform_task_queue* TaskQueue)
{
    return ASSET_STATE_UNLOADED;
}
