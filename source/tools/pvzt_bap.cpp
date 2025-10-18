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

struct bap_asset_texture
{
    game_asset_id               AssetID;
    bap_texture_buffer          TextureBuffer;
    asset_pack_entry_header*    EntryHeader;
};

struct bap_asset_pack
{
    u32                 TextureCount;
    bap_asset_texture*  Textures;
};

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

internal void
BAP_GenerateAssetPack(bap_asset_pack* AssetPack, const char* AssetRootDirectoryPath)
{
    ZERO_STRUCT_POINTER(AssetPack);

    AssetPack->TextureCount = 5;
    AssetPack->Textures = (bap_asset_texture*)malloc(AssetPack->TextureCount * sizeof(bap_asset_texture));
    ZERO_STRUCT_ARRAY(AssetPack->Textures, AssetPack->TextureCount);

    {
        const char* FileName = "plant_sunflower.png";
        const memory_size BytesPerPixel = 4;
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);
        AssetPack->Textures[0].AssetID = GAME_ASSET_ID_PLANT_SUNFLOWER;
        AssetPack->Textures[0].TextureBuffer = TextureBuffer;
    }
    {
        const char* FileName = "plant_peashooter.png";
        const memory_size BytesPerPixel = 4;
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);
        AssetPack->Textures[1].AssetID = GAME_ASSET_ID_PLANT_PEASHOOTER;
        AssetPack->Textures[1].TextureBuffer = TextureBuffer;
    }
    {
        const char* FileName = "projectile_sun.png";
        const memory_size BytesPerPixel = 4;
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);
        AssetPack->Textures[2].AssetID = GAME_ASSET_ID_PROJECTILE_SUN;
        AssetPack->Textures[2].TextureBuffer = TextureBuffer;
    }
    {
        const char* FileName = "projectile_pea.png";
        const memory_size BytesPerPixel = 4;
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);
        AssetPack->Textures[3].AssetID = GAME_ASSET_ID_PROJECTILE_PEA;
        AssetPack->Textures[3].TextureBuffer = TextureBuffer;
    }    
    {
        const char* FileName = "zombie_normal.png";
        const memory_size BytesPerPixel = 4;
        bap_texture_buffer TextureBuffer = BAP_LoadTextureFromFile(AssetRootDirectoryPath, FileName, BytesPerPixel);
        AssetPack->Textures[4].AssetID = GAME_ASSET_ID_ZOMBIE_NORMAL;
        AssetPack->Textures[4].TextureBuffer = TextureBuffer;
    }
}

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
    EMIT_ARRAY(Stream, (u8*)Texture->TextureBuffer.PixelBuffer, PixelBufferByteCount);

    // NOTE(Traian): Finalize the entry header.
    EntryHeader->ByteCount = Stream->ByteOffset - EntryHeader->ByteOffset;
}

internal void
BAP_WriteAssetPack(memory_stream* Stream, const bap_asset_pack* AssetPack)
{
    // NOTE(Traian): Emit the asset pack header to the stream.
    asset_pack_header Header = {};
    Header.MagicWord = PVZ_ASSET_PACK_MAGIC_WORD;
    Header.EntryCount = AssetPack->TextureCount;
    EMIT(Stream, Header);

    // NOTE(Traian): Emit all entry headers, without filling any information. They will be finalized
    // when each asset is written to the stream and thus the byte offset and byte count is known.
    for (u32 TextureIndex = 0; TextureIndex < AssetPack->TextureCount; ++TextureIndex)
    {
        asset_pack_entry_header EntryHeader = {};
        AssetPack->Textures[TextureIndex].EntryHeader = PEEK(Stream, asset_pack_entry_header);
        EMIT(Stream, EntryHeader);
    }

    // NOTE(Traian): Write texture assets to the stream.
    for (u32 TextureIndex = 0; TextureIndex < AssetPack->TextureCount; ++TextureIndex)
    {
        BAP_WriteTexture(Stream, AssetPack->Textures + TextureIndex);
    }
}

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
