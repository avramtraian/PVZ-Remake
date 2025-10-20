// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "../pvz_asset.h"
#include "../pvz_platform.h"

[[noreturn]] internal inline void
BAP_InternalPanicWithMessage(const char* Message)
{
#ifdef PVZ_WINDOWS
    OutputDebugStringA(Message);
#endif // PVZ_WINDOWS
    printf("%s", Message);
    fflush(stdout);
}

#ifdef PANIC
    #undef PANIC
#endif // PANIC

#define PANIC(...)                                                      \
    {                                                                   \
        char MessageBuffer[128] = {};                                   \
        snprintf(MessageBuffer, sizeof(MessageBuffer), __VA_ARGS__);    \
        BAP_InternalPanicWithMessage(MessageBuffer);                    \
        __debugbreak();                                                 \
    }

// TODO(Traian): Maybe it is not the best idea to unit-build the 'pvz_memory.cpp', and we should instead create a
// static library that contains all of the code that is not specific to the game itself. What we do now only works
// because 'pvz_memory.cpp' doesn't pull any other functions - which might be a fine contraint to implement!
#include "../pvz_memory.h"
#include "../pvz_memory.cpp"

// NOTE(Traian): Used for reading from raw data files and writing the asset pack from/to disk, as well as informing
// the user about the command status by logging in the console.
// TODO(Traian): Replace the 'fopen' and 'fwrite' calls with a custom file write stream!
#include <stdio.h>
#include <stdlib.h>

// NOTE(Traian): Currently, image and fonts are loaded from the raw data files using the STB libraries. Because this
// tool only runs offline (when the game is packaged for distribution), there is no need for high-performant/custom
// implementations for these utilities (not that STB libraries are not fast!).

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

struct bap_read_file_result
{
    b8          IsValid;
    memory_size FileSize;
    void*       FileData;
};

internal inline bap_read_file_result
BAP_ReadEntireFile(const char* FileName)
{
    bap_read_file_result Result = {};
    Result.IsValid = false;

    // TODO(Traian): Correctly handling file errors would actually be easier to implement if
    // we just use the Win32 API directly instead of the wrapper inside the C runtime library!
    // TODO(Traian): Currently, the game doesn't provide any platform API for opening arbitrary
    // files on disk - if in the future such API will be available, use it instead!

    FILE* FileHandle = fopen(FileName, "rb");
    if (FileHandle)
    {
        fseek(FileHandle, 0, SEEK_END);
        Result.FileSize = ftell(FileHandle);
        rewind(FileHandle);
        Result.FileData = malloc(Result.FileSize);
        fread(Result.FileData, 1, Result.FileSize, FileHandle);
        fclose(FileHandle);
        Result.IsValid = true;
    }

    return Result;
}

//====================================================================================================================//
//-------------------------------------------------- TEXTURE LOADING -------------------------------------------------//
//====================================================================================================================//

internal inline memory_size
BAP_GetPixelBufferByteCount(u32 SizeX, u32 SizeY, memory_size BytesPerPixel)
{
    const memory_size Result = (memory_size)SizeX *
                               (memory_size)SizeY *
                               BytesPerPixel;
    return Result;
}

struct bap_texture_buffer
{
    u32         SizeX;
    u32         SizeY;
    memory_size BytesPerPixel;
    void*       PixelBuffer;
};

internal bap_texture_buffer
BAP_LoadTextureFromFile(const char* AssetRootDirectoryPath, const char* FileName, memory_size ExpectedBytesPerPixel)
{
    char FullFileName[512] = {};
    snprintf(FullFileName, sizeof(FullFileName), "%s/%s", AssetRootDirectoryPath, FileName);

    bap_read_file_result ReadResult = BAP_ReadEntireFile(FullFileName);
    if (ReadResult.IsValid)
    {
        stbi_set_flip_vertically_on_load(1);
        int Width, Height, Channels;
        u8* ImageData = stbi_load_from_memory((const u8*)ReadResult.FileData, ReadResult.FileSize,
                                              &Width, &Height, &Channels, ExpectedBytesPerPixel);
        
        bap_texture_buffer TextureBuffer = {};
        TextureBuffer.SizeX = Width;
        TextureBuffer.SizeY = Height;
        TextureBuffer.BytesPerPixel = ExpectedBytesPerPixel;
        
        const memory_size PixelBufferByteCount = BAP_GetPixelBufferByteCount(TextureBuffer.SizeX,
                                                                             TextureBuffer.SizeY,
                                                                             TextureBuffer.BytesPerPixel);
        TextureBuffer.PixelBuffer = malloc(PixelBufferByteCount);

        if (TextureBuffer.BytesPerPixel == 4)
        {
            u32* DstPixels = (u32*)TextureBuffer.PixelBuffer;
            const u32* SrcPixels = (const u32*)ImageData;
            const memory_size PixelCount = PixelBufferByteCount / TextureBuffer.BytesPerPixel;

            for (memory_size PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
            {
                // NOTE(Traian): STBI outputs the pixel data in RGBA format, but our renderer assumes all
                // texture data is in the BGRA format. Convert the byte-ordering before packaging the asset.

                const u32 RGBAPixel = SrcPixels[PixelIndex];
                const u32 BGRAPixel = ((RGBAPixel & 0x000000FF) << 16) |
                                      ((RGBAPixel & 0x0000FF00) <<  0) |
                                      ((RGBAPixel & 0x00FF0000) >> 16) |
                                      ((RGBAPixel & 0xFF000000) >>  0);
                DstPixels[PixelIndex] = BGRAPixel;
            }
        }
        else
        {
            PANIC("Invalid/unsupported texture bytes per pixel value!");
        }

        stbi_image_free(ImageData);
        free(ReadResult.FileData);
        return TextureBuffer;
    }
    else
    {
        PANIC("Failed to open texture file '%s' for reading!", FileName);
    }
}

//====================================================================================================================//
//--------------------------------------------------- FONT LOADING ---------------------------------------------------//
//====================================================================================================================//

struct bap_font_glyph
{
    u32                 Codepoint;
    s32                 AdvanceWidth;
    s32                 LeftSideBearing;
    s32                 TextureOffsetX;
    s32                 TextureOffsetY;
    bap_texture_buffer  Texture;
};

struct bap_font_buffer
{
    f32             Height;
    s32             Ascent;
    s32             Descent;
    s32             LineGap;
    u32             GlyphCount;
    bap_font_glyph* Glyphs;
    s32*            KerningTable;
};

internal bap_font_buffer
BAP_LoadFontFromFile(const char* AssetRootDirectoryPath, const char* FileName, float FontHeight,
                     const char* Codepoints, u32 CodepointCount)
{
    char FullFileName[512] = {};
    snprintf(FullFileName, sizeof(FullFileName), "%s/%s", AssetRootDirectoryPath, FileName);

    bap_read_file_result ReadResult = BAP_ReadEntireFile(FullFileName);
    if (ReadResult.IsValid)
    {
        stbtt_fontinfo FontInfo = {};
        if (stbtt_InitFont(&FontInfo, (const u8*)ReadResult.FileData,
                           stbtt_GetFontOffsetForIndex((const u8*)ReadResult.FileData, 0)))
        {
            const f32 Scale = stbtt_ScaleForPixelHeight(&FontInfo, FontHeight);
            int FontAscent, FontDescent, FontLineGap;
            stbtt_GetFontVMetrics(&FontInfo, &FontAscent, &FontDescent, &FontLineGap);

            bap_font_buffer FontBuffer = {};
            FontBuffer.Height = FontHeight;
            FontBuffer.Ascent = Scale * FontAscent;
            FontBuffer.Descent = Scale * FontDescent;
            FontBuffer.LineGap = Scale * FontLineGap;
            FontBuffer.GlyphCount = CodepointCount;
            FontBuffer.Glyphs = (bap_font_glyph*)malloc(CodepointCount * sizeof(bap_font_glyph));
            ZERO_STRUCT_ARRAY(FontBuffer.Glyphs, CodepointCount);
            FontBuffer.KerningTable = (s32*)malloc(CodepointCount * CodepointCount * sizeof(s32));
            ZERO_STRUCT_ARRAY(FontBuffer.KerningTable, CodepointCount * CodepointCount);

            //
            // NOTE(Traian): Generate the kerning table.
            //

            for (u32 FirstCodepointIndex = 0; FirstCodepointIndex < CodepointCount; ++FirstCodepointIndex)
            {
                for (u32 SecondCodepointIndex = 0; SecondCodepointIndex < CodepointCount; ++SecondCodepointIndex)
                {
                    const u32 FirstCodepoint = Codepoints[FirstCodepointIndex];
                    const u32 SecondCodepoint = Codepoints[SecondCodepointIndex];
                    const int KernAdvance = stbtt_GetCodepointKernAdvance(&FontInfo, FirstCodepoint, SecondCodepoint);

                    const u32 KerningTableIndex = (FirstCodepointIndex * CodepointCount) + SecondCodepointIndex;
                    FontBuffer.KerningTable[KerningTableIndex] = Scale * KernAdvance;
                }
            }

            //
            // NOTE(Traian): Generate glyph metrics and textures.
            //

            for (u32 CodepointIndex = 0; CodepointIndex < CodepointCount; ++CodepointIndex)
            {
                const int GlyphCodepoint = Codepoints[CodepointIndex];
                const int GlyphIndex = stbtt_FindGlyphIndex(&FontInfo, GlyphCodepoint);
                if (GlyphIndex != 0)
                {
                    int GlyphAdvanceWidth, GlyphLeftSideBearing;
                    stbtt_GetGlyphHMetrics(&FontInfo, GlyphIndex, &GlyphAdvanceWidth, &GlyphLeftSideBearing);

                    int GlyphWidth, GlyphHeight, GlyphOffsetX, GlyphOffsetY;
                    u8* GlyphTextureData = stbtt_GetGlyphBitmap(&FontInfo, Scale, Scale, GlyphIndex,
                                                                &GlyphWidth, &GlyphHeight,
                                                                &GlyphOffsetX, &GlyphOffsetY);
                    const memory_size GLYPH_TEXTURE_BPP = 1;
                    
                    bap_font_glyph* Glyph = FontBuffer.Glyphs + CodepointIndex;
                    Glyph->Codepoint = GlyphCodepoint;
                    Glyph->AdvanceWidth = Scale * GlyphAdvanceWidth;
                    Glyph->LeftSideBearing = Scale * GlyphLeftSideBearing;
                    Glyph->TextureOffsetX = GlyphOffsetX;
                    // NOTE(traian): The stbtt generated bitmap is Y-down (top-left origin).
                    Glyph->TextureOffsetY = -(GlyphHeight + GlyphOffsetY);
                    Glyph->Texture.SizeX = GlyphWidth;
                    Glyph->Texture.SizeY = GlyphHeight;
                    Glyph->Texture.BytesPerPixel = GLYPH_TEXTURE_BPP;
                    Glyph->Texture.PixelBuffer = malloc(BAP_GetPixelBufferByteCount(Glyph->Texture.SizeX,
                                                                                    Glyph->Texture.SizeY,
                                                                                    GLYPH_TEXTURE_BPP));

                    // NOTE(Traian): The stb_truetype library generates the bitmap Y-down, while the engine assumes all
                    // textures are Y-up, so flip the glyph texture before packaging it into the asset pack.
                    u8* DstPixels = (u8*)Glyph->Texture.PixelBuffer;
                    const u8* SrcPixels = GlyphTextureData + ((Glyph->Texture.SizeY - 1) * Glyph->Texture.SizeX);
                    const memory_size RowByteCount = Glyph->Texture.SizeX * GLYPH_TEXTURE_BPP;
                    for (u32 RowIndex = 0; RowIndex < Glyph->Texture.SizeY; ++RowIndex)
                    {
                        CopyMemory(DstPixels, SrcPixels, RowByteCount);
                        DstPixels += Glyph->Texture.SizeX;
                        SrcPixels -= Glyph->Texture.SizeX;
                    }

                    stbtt_FreeBitmap(GlyphTextureData, NULL);
                }
            }

            free(ReadResult.FileData);
            return FontBuffer;  
        }
        else
        {
            PANIC("Failed to read a font from the file '%s' using the stb_truetype library!", FileName);
        }
    }
    else
    {
        PANIC("Failed to open font file '%s' for reading!", FileName);
    }
}

//====================================================================================================================//
//----------------------------------------------- ASSET PACK DEFINITION ----------------------------------------------//
//====================================================================================================================//

struct bap_asset_texture
{
    game_asset_id               AssetID;
    asset_pack_entry_header*    EntryHeader;
    bap_texture_buffer          TextureBuffer;
};

struct bap_asset_font
{
    game_asset_id               AssetID;
    asset_pack_entry_header*    EntryHeader;
    bap_font_buffer             FontBuffer;
};

struct bap_asset_pack
{
    u32                 TextureCount;
    bap_asset_texture*  Textures;
    u32                 FontCount;
    bap_asset_font*     Fonts;
};

internal void
BAP_GenerateAssetPack(bap_asset_pack* AssetPack, const char* AssetRootDirectoryPath)
{
    ZERO_STRUCT_POINTER(AssetPack);

    AssetPack->TextureCount = 9;
    AssetPack->Textures = (bap_asset_texture*)malloc(AssetPack->TextureCount * sizeof(bap_asset_texture));
    ZERO_STRUCT_ARRAY(AssetPack->Textures, AssetPack->TextureCount);
    u32 CurrentTextureIndex = 0;

#define BAP_ADD_TEXTURE(FileName, AssetIDValue)                                                                         \
    {                                                                                                                   \
        const memory_size BytesPerPixel = 4;                                                                            \
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);    \
        AssetPack->Textures[CurrentTextureIndex].AssetID = AssetIDValue;                                                \
        AssetPack->Textures[CurrentTextureIndex].TextureBuffer = TextureBuffer;                                         \
        ++CurrentTextureIndex;                                                                                          \
    }

    BAP_ADD_TEXTURE("plant_sunflower.png", GAME_ASSET_ID_PLANT_SUNFLOWER);
    BAP_ADD_TEXTURE("plant_peashooter.png", GAME_ASSET_ID_PLANT_PEASHOOTER);
    BAP_ADD_TEXTURE("plant_repeater.png", GAME_ASSET_ID_PLANT_REPEATER);
    BAP_ADD_TEXTURE("plant_torchwood.png", GAME_ASSET_ID_PLANT_TORCHWOOD);
    BAP_ADD_TEXTURE("projectile_sun.png", GAME_ASSET_ID_PROJECTILE_SUN);
    BAP_ADD_TEXTURE("projectile_pea.png", GAME_ASSET_ID_PROJECTILE_PEA);
    BAP_ADD_TEXTURE("projectile_pea_fire.png", GAME_ASSET_ID_PROJECTILE_PEA_FIRE);
    BAP_ADD_TEXTURE("zombie_normal.png", GAME_ASSET_ID_ZOMBIE_NORMAL);
    BAP_ADD_TEXTURE("ui_seed_packet.png", GAME_ASSET_ID_UI_SEED_PACKET);

#undef BAP_ADD_TEXTURE

    AssetPack->FontCount = 1;
    AssetPack->Fonts = (bap_asset_font*)malloc(AssetPack->FontCount * sizeof(bap_asset_font));
    ZERO_STRUCT_ARRAY(AssetPack->Fonts, AssetPack->FontCount);
    u32 CurrentFontIndex = 0;

    {
        const char* FileName = "comic.ttf";
        const f32 Height = 64.0F;
        const char Codepoints[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
        bap_font_buffer FontBuffer = BAP_LoadFontFromFile(AssetRootDirectoryPath, FileName, Height,
                                                          Codepoints, sizeof(Codepoints));
        AssetPack->Fonts[CurrentFontIndex].AssetID = GAME_ASSET_ID_FONT_COMIC_SANS;
        AssetPack->Fonts[CurrentFontIndex].FontBuffer = FontBuffer;
        ++CurrentFontIndex;
    }

    if (CurrentTextureIndex != AssetPack->TextureCount)
    {
        PANIC("CurrentTextureIndex != AssetPack->TextureCount");
    }

    if (CurrentFontIndex != AssetPack->FontCount)
    {
        PANIC("CurrentFontIndex != AssetPack->FontCount");
    }
}

//====================================================================================================================//
//------------------------------------------------ ASSET PACK WRITING ------------------------------------------------//
//====================================================================================================================//

internal void
BAP_WriteTexture(memory_stream* Stream, const bap_asset_texture* Texture)
{
    // NOTE(Traian): Fill available information in the entry header.
    asset_pack_entry_header* EntryHeader = Texture->EntryHeader;
    EntryHeader->AssetID = Texture->AssetID;
    EntryHeader->Type = ASSET_TYPE_TEXTURE;
    EntryHeader->ByteOffset = Stream->ByteOffset;

    // NOTE(Traian): Emit the header.
    asset_header_texture TextureHeader = {};
    TextureHeader.SizeX = Texture->TextureBuffer.SizeX;
    TextureHeader.SizeY = Texture->TextureBuffer.SizeY;
    TextureHeader.BytesPerPixel = Texture->TextureBuffer.BytesPerPixel;
    EMIT(Stream, TextureHeader);

    // NOTE(Traian): Emit the pixel buffer.
    const memory_size PixelBufferByteCount = BAP_GetPixelBufferByteCount(TextureHeader.SizeX,
                                                                         TextureHeader.SizeY,
                                                                         TextureHeader.BytesPerPixel);
    if (TextureHeader.BytesPerPixel == 4)
    {
        EMIT_ARRAY(Stream, (u32*)Texture->TextureBuffer.PixelBuffer, PixelBufferByteCount / sizeof(u32));
    }
    else
    {
        PANIC("Invalid texture BPP when trying to write it to the asset pack!");
    }

    // NOTE(Traian): Finalize the entry header.
    EntryHeader->ByteCount = Stream->ByteOffset - EntryHeader->ByteOffset;
}

internal void
BAP_WriteFont(memory_stream* Stream, const bap_asset_font* Font)
{
    // NOTE(Traian): Fill available information in the entry header.
    asset_pack_entry_header* EntryHeader = Font->EntryHeader;
    EntryHeader->AssetID = Font->AssetID;
    EntryHeader->Type = ASSET_TYPE_FONT;
    EntryHeader->ByteOffset = Stream->ByteOffset;

    // NOTE(Traian): Emit the header.
    asset_header_font FontHeader = {};
    FontHeader.Height = Font->FontBuffer.Height;
    FontHeader.Ascent = Font->FontBuffer.Ascent;
    FontHeader.Descent = Font->FontBuffer.Descent;
    FontHeader.LineGap = Font->FontBuffer.LineGap;
    FontHeader.GlyphCount = Font->FontBuffer.GlyphCount;
    EMIT(Stream, FontHeader);

    // NOTE(Traian): Emit the glyphs.
    for (u32 GlyphIndex = 0; GlyphIndex < Font->FontBuffer.GlyphCount; ++GlyphIndex)
    {
        const bap_font_glyph* Glyph = Font->FontBuffer.Glyphs + GlyphIndex;

        // NOTE(Traian): Emit the glyph header.
        asset_font_glyph_header GlyphHeader = {};
        GlyphHeader.Codepoint = Glyph->Codepoint;
        GlyphHeader.AdvanceWidth = Glyph->AdvanceWidth;
        GlyphHeader.LeftSideBearing = Glyph->LeftSideBearing;
        GlyphHeader.TextureOffsetX = Glyph->TextureOffsetX;
        GlyphHeader.TextureOffsetY = Glyph->TextureOffsetY;
        GlyphHeader.TextureSizeX = Glyph->Texture.SizeX;
        GlyphHeader.TextureSizeY = Glyph->Texture.SizeY;
        EMIT(Stream, GlyphHeader);

        // NOTE(Traian): Emit the glyph texture pixel buffer.
        const memory_size PixelBufferByteCount = BAP_GetPixelBufferByteCount(Glyph->Texture.SizeX,
                                                                             Glyph->Texture.SizeY,
                                                                             Glyph->Texture.BytesPerPixel);
        EMIT_ARRAY(Stream, (u8*)Glyph->Texture.PixelBuffer, PixelBufferByteCount / sizeof(u8));
    }

    // NOTE(Traian): Emit the kerning table.
    EMIT_ARRAY(Stream, Font->FontBuffer.KerningTable, Font->FontBuffer.GlyphCount * Font->FontBuffer.GlyphCount);

    // NOTE(Traian): Finalize the entry header.
    EntryHeader->ByteCount = Stream->ByteOffset - EntryHeader->ByteOffset;
}

internal void
BAP_WriteAssetPack(memory_stream* Stream, const bap_asset_pack* AssetPack)
{
    // NOTE(Traian): Emit the asset pack header to the stream.
    asset_pack_header Header = {};
    Header.MagicWord = PVZ_ASSET_PACK_MAGIC_WORD;
    Header.EntryCount = AssetPack->TextureCount + AssetPack->FontCount;
    EMIT(Stream, Header);

    // NOTE(Traian): Emit all entry headers, without filling any information. They will be finalized
    // when each asset is written to the stream and thus the byte offset and byte count is known.
    for (u32 TextureIndex = 0; TextureIndex < AssetPack->TextureCount; ++TextureIndex)
    {
        asset_pack_entry_header EntryHeader = {};
        AssetPack->Textures[TextureIndex].EntryHeader = PEEK(Stream, asset_pack_entry_header);
        EMIT(Stream, EntryHeader);
    }
    for (u32 FontIndex = 0; FontIndex < AssetPack->FontCount; ++FontIndex)
    {
        asset_pack_entry_header EntryHeader = {};
        AssetPack->Fonts[FontIndex].EntryHeader = PEEK(Stream, asset_pack_entry_header);
        EMIT(Stream, EntryHeader);
    }

    // NOTE(Traian): Write texture assets to the stream.
    for (u32 TextureIndex = 0; TextureIndex < AssetPack->TextureCount; ++TextureIndex)
    {
        BAP_WriteTexture(Stream, AssetPack->Textures + TextureIndex);
    }

    // NOTE(Traian): Write font assets to the stream.
    for (u32 FontIndex = 0; FontIndex < AssetPack->FontCount; ++FontIndex)
    {
        BAP_WriteFont(Stream, AssetPack->Fonts + FontIndex);
    }
}

//====================================================================================================================//
//------------------------------------------------- TOOL ENTRY POINT -------------------------------------------------//
//====================================================================================================================//

function int
main(int ArgumentCount, char** Arguments)
{
    if (ArgumentCount != 3)
    {
        printf("Incorrect number of arguments provided!\n");
        return 1;
    }
    const char* AssetRootDirectoryPath = Arguments[1];
    const char* OutputFileName = Arguments[2];

    // NOTE(Traian): Generate the asset pack from the raw data files.
    bap_asset_pack AssetPack = {};
    BAP_GenerateAssetPack(&AssetPack, AssetRootDirectoryPath);

    // NOTE(Traian): Serialize the asset pack to a memory stream.
    memory_stream OutputStream = {};
    OutputStream.ByteCount = MEGABYTES(32);
    OutputStream.MemoryBlock = malloc(OutputStream.ByteCount);
    ZeroMemory(OutputStream.MemoryBlock, OutputStream.ByteCount);
    BAP_WriteAssetPack(&OutputStream, &AssetPack);

    // NOTE(Traian): Write the contents of the memory stream to the output file.
    FILE* OutputFile = fopen(OutputFileName, "wb");
    if (OutputFile)
    {
        fwrite(OutputStream.MemoryBlock, OutputStream.ByteOffset, 1, OutputFile);
        fclose(OutputFile);
        OutputFile = NULL;
    }
    else
    {
        printf("Failed to open the output file '%s' for writing!", OutputFileName);
        return 2;
    }

    return 0;
}
