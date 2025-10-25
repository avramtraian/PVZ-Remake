// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_renderer.h"

//====================================================================================================================//
//------------------------------------------------------- IMAGE ------------------------------------------------------//
//====================================================================================================================//

function memory_size
Image_GetBytesPerPixelForFormat(renderer_image_format Format)
{
    memory_size Result = 0;
    switch (Format)
    {
        case RENDERER_IMAGE_FORMAT_A8:          Result = 1; break;
        case RENDERER_IMAGE_FORMAT_B8G8R8A8:    Result = 4; break;
    }
    return Result;
}

function memory_size
Image_GetPixelBufferByteCount(u32 SizeX, u32 SizeY, renderer_image_format Format)
{
    const memory_size Result = (memory_size)SizeX *
                               (memory_size)SizeY *
                               Image_GetBytesPerPixelForFormat(Format);
    return Result;
}

function void
Image_AllocateFromArena(renderer_image* Image, memory_arena* Arena,
                        renderer_image_format Format, u32 SizeX, u32 SizeY)
{
    ASSERT(SizeX > 0 && SizeY > 0);
    ASSERT(Format != RENDERER_IMAGE_FORMAT_UNKNOWN);

    // NOTE(Traian): Initialize the image fields.
    ZERO_STRUCT_POINTER(Image);
    Image->Format = Format;
    Image->SizeX = SizeX;
    Image->SizeY = SizeY;

    // NOTE(Traian): Allocate memory for the pixel buffer.
    const memory_size PixelBufferByteCount = (memory_size)SizeX *
                                             (memory_size)SizeY *
                                             Image_GetBytesPerPixelForFormat(Format);
    Image->PixelBuffer = MemoryArena_Allocate(Arena, PixelBufferByteCount, sizeof(void*));
}

function void*
Image_GetPixelAddress(renderer_image* Image, u32 PixelIndexX, u32 PixelIndexY)
{
    const memory_size BytesPerPixel = Image_GetBytesPerPixelForFormat(Image->Format);
    void* PixelAddress = NULL;
    if (PixelIndexX < Image->SizeX && PixelIndexY < Image->SizeY)
    {
        const usize PixelIndex = ((usize)PixelIndexY * (usize)Image->SizeX) + (usize)PixelIndexX;
        PixelAddress = (u8*)Image->PixelBuffer + (PixelIndex * BytesPerPixel);
    }
    return PixelAddress;
}

function const void*
Image_GetPixelAddress(const renderer_image* Image, u32 PixelIndexX, u32 PixelIndexY)
{
    const memory_size BytesPerPixel = Image_GetBytesPerPixelForFormat(Image->Format);
    const void* PixelAddress = NULL;
    if (PixelIndexX < Image->SizeX && PixelIndexY < Image->SizeY)
    {
        const usize PixelIndex = ((usize)PixelIndexY * (usize)Image->SizeX) + (usize)PixelIndexX;
        PixelAddress = (const u8*)Image->PixelBuffer + (PixelIndex * BytesPerPixel);
    }
    return PixelAddress;
}

function f32
Image_SampleBilinearA8(const renderer_image* Image, vec2 UV)
{
    ASSERT(Image->Format == RENDERER_IMAGE_FORMAT_A8);
    ASSERT(Image->SizeX > 0 && Image->SizeY > 0);
    ASSERT(0.0F <= UV.X && UV.X <= 1.0F && 0.0F <= UV.Y && UV.Y <= 1.0F);

    // NOTE(Traian): Bottom-left pixel.
    const u32 Tex1X = (u32)(UV.X * (f32)Image->SizeX);
    const u32 Tex1Y = (u32)(UV.Y * (f32)Image->SizeY);
    // NOTE(Traian): Bottom-right pixel.
    const u32 Tex2X = Tex1X + 1;
    const u32 Tex2Y = Tex1Y;
    // NOTE(Traian): Top-left pixel.
    const u32 Tex3X = Tex1X;
    const u32 Tex3Y = Tex1Y + 1;
    // NOTE(Traian): Top-right pixel.
    const u32 Tex4X = Tex1X + 1;
    const u32 Tex4Y = Tex1Y + 1;

    const u8* Pixel1Address = (const u8*)Image_GetPixelAddress(Image, Tex1X, Tex1Y);
    const u8* Pixel2Address = (const u8*)Image_GetPixelAddress(Image, Tex2X, Tex2Y);
    const u8* Pixel3Address = (const u8*)Image_GetPixelAddress(Image, Tex3X, Tex3Y);
    const u8* Pixel4Address = (const u8*)Image_GetPixelAddress(Image, Tex4X, Tex4Y);

    const f32 Sample1 = (1.0F / 255.0F) * (f32)(*(Pixel1Address));
    const f32 Sample2 = (1.0F / 255.0F) * (f32)(*(Pixel2Address ? Pixel2Address : Pixel1Address));
    const f32 Sample3 = (1.0F / 255.0F) * (f32)(*(Pixel3Address ? Pixel3Address : Pixel1Address));
    const f32 Sample4 = (1.0F / 255.0F) * (f32)(*(Pixel4Address ? Pixel4Address : Pixel1Address));

    const f32 TX = UV.X - ((f32)Tex1X / (f32)Image->SizeX);
    const f32 TY = UV.Y - ((f32)Tex1Y / (f32)Image->SizeY);

    const f32 InterpolatedBottom = Math_Lerp(Sample1, Sample2, TX);
    const f32 InterpolatedTop    = Math_Lerp(Sample3, Sample4, TX);
    const f32 Interpolated       = Math_Lerp(InterpolatedBottom, InterpolatedTop, TY);

    return Interpolated;
}

function color4
Image_SampleBilinearB8G8R8A8(const renderer_image* Image, vec2 UV)
{
    ASSERT(Image->Format == RENDERER_IMAGE_FORMAT_B8G8R8A8);
    ASSERT(Image->SizeX > 0 && Image->SizeY > 0);
    ASSERT(0.0F <= UV.X && UV.X <= 1.0F && 0.0F <= UV.Y && UV.Y <= 1.0F);

    // NOTE(Traian): Bottom-left pixel.
    const u32 Tex1X = (u32)(UV.X * (f32)Image->SizeX);
    const u32 Tex1Y = (u32)(UV.Y * (f32)Image->SizeY);
    // NOTE(Traian): Bottom-right pixel.
    const u32 Tex2X = Tex1X + 1;
    const u32 Tex2Y = Tex1Y;
    // NOTE(Traian): Top-left pixel.
    const u32 Tex3X = Tex1X;
    const u32 Tex3Y = Tex1Y + 1;
    // NOTE(Traian): Top-right pixel.
    const u32 Tex4X = Tex1X + 1;
    const u32 Tex4Y = Tex1Y + 1;

    const u32* Pixel1Address = (const u32*)Image_GetPixelAddress(Image, Tex1X, Tex1Y);
    const u32* Pixel2Address = (const u32*)Image_GetPixelAddress(Image, Tex2X, Tex2Y);
    const u32* Pixel3Address = (const u32*)Image_GetPixelAddress(Image, Tex3X, Tex3Y);
    const u32* Pixel4Address = (const u32*)Image_GetPixelAddress(Image, Tex4X, Tex4Y);

    const color4 Sample1 = Color4_FromLinear(LinearColor_UnpackFromBGRA(*Pixel1Address));
    const color4 Sample2 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel2Address ? *Pixel2Address : *Pixel1Address));
    const color4 Sample3 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel3Address ? *Pixel3Address : *Pixel1Address));
    const color4 Sample4 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel4Address ? *Pixel4Address : *Pixel1Address));

    const f32 TX = UV.X - ((f32)Tex1X / (f32)Image->SizeX);
    const f32 TY = UV.Y - ((f32)Tex1Y / (f32)Image->SizeY);

    const color4 InterpolatedBottom = Math_LerpColor4(Sample1, Sample2, TX);
    const color4 InterpolatedTop    = Math_LerpColor4(Sample3, Sample4, TX);
    const color4 Interpolated       = Math_LerpColor4(InterpolatedBottom, InterpolatedTop, TY);

    return Interpolated;
}

//====================================================================================================================//
//------------------------------------------------------ TEXTURE -----------------------------------------------------//
//====================================================================================================================//

internal b8
Texture_DownsampleByFactorOf2(renderer_image* DstImage, const renderer_image* SrcImage, memory_arena* Arena)
{
    const u32 DstSizeX = SrcImage->SizeX / 2;
    const u32 DstSizeY = SrcImage->SizeY / 2;
    if (DstSizeX > 0 && DstSizeY > 0)
    {
        ZERO_STRUCT_POINTER(DstImage);
        Image_AllocateFromArena(DstImage, Arena, SrcImage->Format, DstSizeX, DstSizeY);

        if (DstImage->Format == RENDERER_IMAGE_FORMAT_A8)
        {
            for (u32 DstPixelY = 0; DstPixelY < DstImage->SizeY; ++DstPixelY)
            {
                for (u32 DstPixelX = 0; DstPixelX < DstImage->SizeX; ++DstPixelX)
                {
                    const u32 SrcPixel00X = (2 * DstPixelX) + 0;
                    const u32 SrcPixel00Y = (2 * DstPixelY) + 0;

                    const u32 SrcPixel01X = (2 * DstPixelX) + 1;
                    const u32 SrcPixel01Y = (2 * DstPixelY) + 0;
                    
                    const u32 SrcPixel10X = (2 * DstPixelX) + 0;
                    const u32 SrcPixel10Y = (2 * DstPixelY) + 1;
                    
                    const u32 SrcPixel11X = (2 * DstPixelX) + 1;
                    const u32 SrcPixel11Y = (2 * DstPixelY) + 1;

                    const f32 Sample00 = (f32)(*(const u8*)Image_GetPixelAddress(SrcImage, SrcPixel00X, SrcPixel00Y));
                    const f32 Sample01 = (f32)(*(const u8*)Image_GetPixelAddress(SrcImage, SrcPixel01X, SrcPixel01Y));
                    const f32 Sample10 = (f32)(*(const u8*)Image_GetPixelAddress(SrcImage, SrcPixel10X, SrcPixel10Y));
                    const f32 Sample11 = (f32)(*(const u8*)Image_GetPixelAddress(SrcImage, SrcPixel11X, SrcPixel11Y));

                    const f32 Interpolated = 0.25F * (Sample00 + Sample01 + Sample10 + Sample11);
                    const u8 DstPixelValue = (u8)(Interpolated);
                    *(u8*)Image_GetPixelAddress(DstImage, DstPixelX, DstPixelY) = DstPixelValue;
                }
            }
        }
        else if (DstImage->Format == RENDERER_IMAGE_FORMAT_B8G8R8A8)
        {
            for (u32 DstPixelY = 0; DstPixelY < DstImage->SizeY; ++DstPixelY)
            {
                for (u32 DstPixelX = 0; DstPixelX < DstImage->SizeX; ++DstPixelX)
                {
                    const u32 SrcPixel00X = (2 * DstPixelX) + 0;
                    const u32 SrcPixel00Y = (2 * DstPixelY) + 0;

                    const u32 SrcPixel01X = (2 * DstPixelX) + 1;
                    const u32 SrcPixel01Y = (2 * DstPixelY) + 0;
                    
                    const u32 SrcPixel10X = (2 * DstPixelX) + 0;
                    const u32 SrcPixel10Y = (2 * DstPixelY) + 1;
                    
                    const u32 SrcPixel11X = (2 * DstPixelX) + 1;
                    const u32 SrcPixel11Y = (2 * DstPixelY) + 1;

                    const u32 PackedSample00 = *(const u32*)Image_GetPixelAddress(SrcImage, SrcPixel00X, SrcPixel00Y);
                    const u32 PackedSample01 = *(const u32*)Image_GetPixelAddress(SrcImage, SrcPixel01X, SrcPixel01Y);
                    const u32 PackedSample10 = *(const u32*)Image_GetPixelAddress(SrcImage, SrcPixel10X, SrcPixel10Y);
                    const u32 PackedSample11 = *(const u32*)Image_GetPixelAddress(SrcImage, SrcPixel11X, SrcPixel11Y);

                    const color4 Sample00 = Color4_FromLinear(LinearColor_UnpackFromBGRA(PackedSample00));
                    const color4 Sample01 = Color4_FromLinear(LinearColor_UnpackFromBGRA(PackedSample01));
                    const color4 Sample10 = Color4_FromLinear(LinearColor_UnpackFromBGRA(PackedSample10));
                    const color4 Sample11 = Color4_FromLinear(LinearColor_UnpackFromBGRA(PackedSample11));

                    color4 Interpolated;
                    Interpolated.R = 0.25 * (Sample00.R + Sample01.R + Sample10.R + Sample11.R);
                    Interpolated.G = 0.25 * (Sample00.G + Sample01.G + Sample10.G + Sample11.G);
                    Interpolated.B = 0.25 * (Sample00.B + Sample01.B + Sample10.B + Sample11.B);
                    Interpolated.A = 0.25 * (Sample00.A + Sample01.A + Sample10.A + Sample11.A);

                    const u32 DstPixelValue = LinearColor_PackToBGRA(Color4_ToLinear(Interpolated));
                    *(u32*)Image_GetPixelAddress(DstImage, DstPixelX, DstPixelY) = DstPixelValue;
                }
            }
        }
        else
        {
            PANIC("Invalid texture format passed to 'Texture_DownsampleByFactorOf2'!");
        }

        return true;
    }
    else
    {
        return false;
    }
}

function void
Texture_Create(renderer_texture* Texture, memory_arena* Arena,
               const renderer_image* SourceImage, u32 MaxMipCount)
{
    ASSERT(MaxMipCount > 0);

    ZERO_STRUCT_POINTER(Texture);
    Texture->SizeX = SourceImage->SizeX;
    Texture->SizeY = SourceImage->SizeY;
    Texture->Format = SourceImage->Format;
    Texture->MaxMipCount = MaxMipCount;
    Texture->Mips = PUSH_ARRAY(Arena, renderer_image, Texture->MaxMipCount);

    Texture->MipCount = 1;
    Texture->Mips[0] = *SourceImage;

    for (u32 MipLevel = 1; MipLevel < Texture->MaxMipCount; ++MipLevel)
    {
        renderer_image DstMipImage = {};
        renderer_image SrcMipImage = Texture->Mips[MipLevel - 1];

        if (Texture_DownsampleByFactorOf2(&DstMipImage, &SrcMipImage, Arena))
        {
            Texture->Mips[Texture->MipCount++] = DstMipImage;
        }
        else
        {
            break;
        }
    }
}

//====================================================================================================================//
//----------------------------------------------------- RENDERER -----------------------------------------------------//
//====================================================================================================================//

function void
Renderer_Initialize(renderer* Renderer, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(Renderer);
    Renderer->ClusterCount = 12;
    Renderer->MaxPrimitiveCount = 8129;
    Renderer->MaxTextureSlotCount = 64;

    Renderer->Clusters      = PUSH_ARRAY(Arena, renderer_cluster,           Renderer->ClusterCount);
    Renderer->Primitives    = PUSH_ARRAY(Arena, renderer_primitive,         Renderer->MaxPrimitiveCount);
    Renderer->TextureSlots  = PUSH_ARRAY(Arena, const renderer_texture*,    Renderer->MaxTextureSlotCount);

    for (u32 ClusterIndex = 0; ClusterIndex < Renderer->ClusterCount; ++ClusterIndex)
    {
        renderer_cluster* Cluster = Renderer->Clusters + ClusterIndex;
        Cluster->MaxPrimitiveCount = 1024;
        Cluster->Primitives = PUSH_ARRAY(Arena, renderer_primitive, Cluster->MaxPrimitiveCount);
    }
}

internal void
Renderer_GetClusterGridSize(u32 ClusterCount, u32* OutClusterCountX, u32* OutClusterCountY)
{
    *OutClusterCountX = 0;
    *OutClusterCountY = 0;

    if (1 <= ClusterCount && ClusterCount < 2)
    {
        *OutClusterCountX = 1;
        *OutClusterCountY = 1;
    }
    else if (2 <= ClusterCount && ClusterCount < 4)
    {
        *OutClusterCountX = 1;
        *OutClusterCountY = 2;
    }
    else if (4 <= ClusterCount && ClusterCount < 6)
    {
        *OutClusterCountX = 2;
        *OutClusterCountY = 2;
    }
    else if (6 <= ClusterCount && ClusterCount < 8)
    {
        *OutClusterCountX = 2;
        *OutClusterCountY = 3;
    }
    else if (8 <= ClusterCount && ClusterCount < 10)
    {
        *OutClusterCountX = 2;
        *OutClusterCountY = 4;
    }
    else if (10 <= ClusterCount && ClusterCount < 12)
    {
        *OutClusterCountX = 2;
        *OutClusterCountY = 5;
    }
    else if (12 <= ClusterCount && ClusterCount < 16)
    {
        *OutClusterCountX = 3;
        *OutClusterCountY = 4;
    }
    else if (16 <= ClusterCount)
    {
        *OutClusterCountX = 4;
        *OutClusterCountY = 4;
    }
}

function void
Renderer_BeginFrame(renderer* Renderer, u32 ViewportSizeX, u32 ViewportSizeY)
{
    Renderer->ViewportSizeX = ViewportSizeX;
    Renderer->ViewportSizeY = ViewportSizeY;

    //
    // NOTE(Traian): Reset the renderer primitive and texture slot buffers.
    //

    ZERO_STRUCT_ARRAY(Renderer->Primitives, Renderer->CurrentPrimitiveIndex);
    ZERO_STRUCT_ARRAY(Renderer->TextureSlots, Renderer->CurrentTextureSlotIndex);
    Renderer->CurrentPrimitiveIndex = 0;
    Renderer->CurrentTextureSlotIndex = 0;

    //
    // NOTE(Traian): Partition the viewport into multiple clusters.
    //

    u32 ClusterCountX = 0;
    u32 ClusterCountY = 0;
    Renderer_GetClusterGridSize(Renderer->ClusterCount, &ClusterCountX, &ClusterCountY);

    for (u32 ClusterIndexY = 0; ClusterIndexY < ClusterCountY; ++ClusterIndexY)
    {
        for (u32 ClusterIndexX = 0; ClusterIndexX < ClusterCountX; ++ClusterIndexX)
        {
            const u32 ClusterIndex = (ClusterIndexY * ClusterCountX) + ClusterIndexX;
            renderer_cluster* Cluster = Renderer->Clusters + ClusterIndex;

            Cluster->DrawRegionOffsetX = 0;
            if (ClusterIndexX > 0)
            {
                const u32 NeighbourClusterIndex = ClusterIndex - 1;
                const renderer_cluster* NeighbourCluster = Renderer->Clusters + NeighbourClusterIndex;
                Cluster->DrawRegionOffsetX = NeighbourCluster->DrawRegionOffsetX + NeighbourCluster->DrawRegionSizeX;
            }

            Cluster->DrawRegionOffsetY = 0;
            if (ClusterIndexY > 0)
            {
                const u32 NeighbourClusterIndex = ClusterIndex - ClusterCountX;
                const renderer_cluster* NeighbourCluster = Renderer->Clusters + NeighbourClusterIndex;
                Cluster->DrawRegionOffsetY = NeighbourCluster->DrawRegionOffsetY + NeighbourCluster->DrawRegionSizeY;
            }

            Cluster->DrawRegionSizeX = ViewportSizeX / ClusterCountX;
            Cluster->DrawRegionSizeY = ViewportSizeY / ClusterCountY;

            if (ClusterIndexX < (ViewportSizeX % ClusterCountX))
            {
                Cluster->DrawRegionSizeX++;
            }
            if (ClusterIndexY < (ViewportSizeY % ClusterCountY))
            {
                Cluster->DrawRegionSizeY++;
            }
        }
    }

    //
    // NOTE(Traian): Reset the primitive buffer of each cluster.
    //

    for (u32 ClusterIndex = 0; ClusterIndex < Renderer->ClusterCount; ++ClusterIndex)
    {
        renderer_cluster* Cluster = Renderer->Clusters + ClusterIndex;
        ZERO_STRUCT_ARRAY(Cluster->Primitives, Cluster->CurrentPrimitiveIndex);
        Cluster->CurrentPrimitiveIndex = 0;
    }
}

function void
Renderer_EndFrame(renderer* Renderer)
{
}

function void
Renderer_PushPrimitive(renderer* Renderer, vec2 MinPoint, vec2 MaxPoint, f32 ZOffset, color4 Color,
                       vec2 MinUV /*= {}*/, vec2 MaxUV /*= {}*/, const renderer_texture* Texture /*= NULL*/)
{
    if (Renderer->CurrentPrimitiveIndex >= Renderer->MaxPrimitiveCount)
    {
        PANIC("Renderer primitive buffer overflown!");
    }

    u32 TextureSlotIndex = -1;
    if (Texture != NULL)
    {
        TextureSlotIndex = Renderer->CurrentTextureSlotIndex;
        for (u32 SlotIndex = 0; SlotIndex < Renderer->CurrentTextureSlotIndex; ++SlotIndex)
        {
            if (Renderer->TextureSlots[SlotIndex] == Texture)
            {
                TextureSlotIndex = SlotIndex;
                break;
            }
        }

        if (TextureSlotIndex == Renderer->CurrentTextureSlotIndex)
        {
            if (Renderer->CurrentTextureSlotIndex >= Renderer->MaxTextureSlotCount)
            {
                PANIC("Renderer texture slot buffer overflown!");
            }

            Renderer->CurrentTextureSlotIndex++;
            Renderer->TextureSlots[TextureSlotIndex] = Texture;
        }
    }

    renderer_primitive* Primitive = Renderer->Primitives + Renderer->CurrentPrimitiveIndex;
    Primitive->Index = Renderer->CurrentPrimitiveIndex;
    Primitive->MinPoint = MinPoint;
    Primitive->MaxPoint = MaxPoint;
    Primitive->ZOffset = ZOffset;
    Primitive->Color = Color;
    Primitive->MinUV = MinUV;
    Primitive->MaxUV = MaxUV;
    Primitive->TextureSlotIndex = TextureSlotIndex;

    Renderer->CurrentPrimitiveIndex++;
}

internal inline b8
Renderer_ShouldPrimitivesBeSwapped(f32 ZOffsetA, u32 IndexA, f32 ZOffsetB, u32 IndexB)
{
    b8 Result;
    if (ZOffsetA > ZOffsetB)
    {
        Result = true;
    }
    else if ((ZOffsetA == ZOffsetB) && (IndexA > IndexB))
    {
        Result = true;
    }
    else
    {
        Result = false;
    }
    return Result;
}

internal void
Renderer_SortPrimitiveBuffer(renderer_primitive* Primitives, u32 PrimitiveCount)
{
    // TODO(Traian): Implement a better sorting algorithm!

    for (u32 I = 1; I <= PrimitiveCount; ++I)
    {
        for (u32 J = 0; J < PrimitiveCount - I; ++J)
        {
            renderer_primitive* A = Primitives + (J + 0);
            renderer_primitive* B = Primitives + (J + 1);
            if (Renderer_ShouldPrimitivesBeSwapped(A->ZOffset, A->Index, B->ZOffset, B->Index))
            {
                renderer_primitive Temp = *A;
                *A = *B;
                *B = Temp;
            }
        }
    }
}

struct renderer_rasterization_area
{
    u32 PixelOffsetX;
    u32 PixelOffsetY;
    u32 PixelCountX;
    u32 PixelCountY;
};

internal renderer_rasterization_area
Renderer_GetRasterizationArea(f32 ViewportSizeX, f32 ViewportSizeY, rect2D QuadRegion)
{
    ASSERT(ViewportSizeX >= 0.0F && ViewportSizeY >= 0.0F);
    ASSERT(QuadRegion.Min.X >= 0.0F && QuadRegion.Min.Y >= 0.0F && !Rect2D_IsDegenerated(QuadRegion));

    vec2 MinSamplePoint;
    MinSamplePoint.X = QuadRegion.Min.X * ViewportSizeX;
    MinSamplePoint.Y = QuadRegion.Min.Y * ViewportSizeY;
    vec2 MaxSamplePoint;
    MaxSamplePoint.X = QuadRegion.Max.X * ViewportSizeX;
    MaxSamplePoint.Y = QuadRegion.Max.Y * ViewportSizeY;

    s32 MinSampleIndexX = (s32)(MinSamplePoint.X + 0.5F);
    s32 MinSampleIndexY = (s32)(MinSamplePoint.Y + 0.5F);

    // NOTE(Traian): In order to avoid overdraw, if the sample point on one of the axis is *exactly* on the center of
    // the pixel on the right/bottom side of the quad, we discard that pixel. However, floating point math is
    // imprecise! In order to ensure that dividing a quad into multiple quads and rendering them separately doesn't
    // produce any gaps between them, we must introduce a small error tolerance.
    // But this is more of a hack rather than a fix! Error tolerances are dependent on the resolution at which we
    // render, so inconsistencies from one resolution to another might appear! The proper solution to this issue is to
    // use fixed-point math - which is outside of the scope for this game!
    const f32 ERROR_TOLERANCE = 0.0001F;
    
    s32 MaxSampleIndexX = (s32)(MaxSamplePoint.X - 0.5F);
    if (((f32)MaxSampleIndexX + 0.5F) - MaxSamplePoint.X > ERROR_TOLERANCE)
    {
        --MaxSampleIndexX;
    }
    s32 MaxSampleIndexY = (s32)(MaxSamplePoint.Y - 0.5F);
    if (((f32)MaxSampleIndexY + 0.5F) - MaxSamplePoint.Y > ERROR_TOLERANCE)
    {
        --MaxSampleIndexY;
    }

    renderer_rasterization_area RasterizationArea = {};
    RasterizationArea.PixelOffsetX = MinSampleIndexX;
    RasterizationArea.PixelOffsetY = MinSampleIndexY;
    RasterizationArea.PixelCountX = MaxSampleIndexX - MinSampleIndexX + 1;
    RasterizationArea.PixelCountY = MaxSampleIndexY - MinSampleIndexY + 1;
    return RasterizationArea;
}

internal void
Renderer_DrawFilledPrimitive(renderer* Renderer, renderer_image* RenderTarget, renderer_cluster* Cluster,
                             u32 PrimitiveIndex, rect2D ClusterDrawRegion)
{
    const renderer_primitive* Primitive = Cluster->Primitives + PrimitiveIndex;
    const rect2D PrimitiveRegion = Rect2D_Intersect({ Primitive->MinPoint, Primitive->MaxPoint }, ClusterDrawRegion);

    renderer_rasterization_area RasterizationArea = Renderer_GetRasterizationArea(RenderTarget->SizeX,
                                                                                  RenderTarget->SizeY,
                                                                                  PrimitiveRegion);

    if (RenderTarget->Format == RENDERER_IMAGE_FORMAT_B8G8R8A8)
    {
        u32* CurrentRowAddress = (u32*)Image_GetPixelAddress(RenderTarget,
                                                             RasterizationArea.PixelOffsetX,
                                                             RasterizationArea.PixelOffsetY);

        for (u32 PixelIndexY = 0; PixelIndexY < RasterizationArea.PixelCountY; ++PixelIndexY)
        {
            u32* CurrentPixelAddress = CurrentRowAddress;
            for (u32 PixelIndexX = 0; PixelIndexX < RasterizationArea.PixelCountX; ++PixelIndexX)
            {
                // NOTE(Traian): Output the color to the render target buffer.
                const color4 CurrentColor = Color4_FromLinear(LinearColor_UnpackFromBGRA(*CurrentPixelAddress));
                const color4 BlendedColor = Color4(Math_Lerp(CurrentColor.R, Primitive->Color.R, Primitive->Color.A),
                                                   Math_Lerp(CurrentColor.G, Primitive->Color.G, Primitive->Color.A),
                                                   Math_Lerp(CurrentColor.B, Primitive->Color.B, Primitive->Color.A));

                *CurrentPixelAddress = LinearColor_PackToBGRA(Color4_ToLinear(BlendedColor));
                ++CurrentPixelAddress;
            }
            CurrentRowAddress += RenderTarget->SizeX;
        }
    }
    else
    {
        // TODO(Traian): Eventually support more texture formats?
        PANIC("Image with non supported fomat was used as render target!");
    }
}

struct renderer_find_mip_levels_info
{
    f32 NDCPrimitiveSizeX;
    f32 NDCPrimitiveSizeY;
    f32 ViewportSizeX;
    f32 ViewportSizeY;
    f32 UVDeltaX;
    f32 UVDeltaY;
};

struct renderer_find_mip_levels_result
{
    b8  BlendBetweenMips;
    u32 MipLevelA;
    u32 MipLevelB;
    f32 InterpolationFactorAB;
};

internal inline renderer_find_mip_levels_result
Renderer_FindMipLevels(const renderer_texture* Texture, renderer_find_mip_levels_info FindInfo)
{
    const u32 PrimitivePixelCountX = (FindInfo.NDCPrimitiveSizeX * FindInfo.ViewportSizeX) + 1;
    const u32 PrimitivePixelCountY = (FindInfo.NDCPrimitiveSizeY * FindInfo.ViewportSizeY) + 1;
    const f32 UVDeltaX = FindInfo.UVDeltaX;
    const f32 UVDeltaY = FindInfo.UVDeltaY;

    u32 MipLevel;
    b8 FoundMipLevel = false;

    for (u32 MipIndex = 0; MipIndex < Texture->MipCount; ++MipIndex)
    {
        const renderer_image* MipImage = Texture->Mips + MipIndex;
        if ((UVDeltaX * MipImage->SizeX) <= PrimitivePixelCountX && (UVDeltaY * MipImage->SizeY) <= PrimitivePixelCountY)
        {
            MipLevel = MipIndex;
            FoundMipLevel = true;
            break;
        }
    }

    renderer_find_mip_levels_result Result = {};
    if (FoundMipLevel)
    {
        if (MipLevel == 0)
        {
            // NOTE(Traian): We have to just "upsample" the first mip.
            Result.BlendBetweenMips = false;
            Result.MipLevelA = MipLevel;
        }
        else
        {
            // NOTE(Traian): We should blend between the current mip level and the previous mip level.
            Result.BlendBetweenMips = true;
            Result.MipLevelA = MipLevel;
            Result.MipLevelB = MipLevel - 1;
            const renderer_image* MipImageA = Texture->Mips + Result.MipLevelA;
            const renderer_image* MipImageB = Texture->Mips + Result.MipLevelB;

            if ((UVDeltaX * MipImageB->SizeX) > PrimitivePixelCountX && (UVDeltaY * MipImageB->SizeY) > PrimitivePixelCountY)
            {
                // NOTE(Traian): Determine the interpolation factor by calculating the interpolation factors on the
                // X-axis (width) and Y-axis (height) and take their average value.
                const f32 InterpolationFactorX = Math_InverseLerp(UVDeltaX * MipImageA->SizeX, UVDeltaX * MipImageB->SizeX,
                                                                  PrimitivePixelCountX);
                const f32 InterpolationFactorY = Math_InverseLerp(UVDeltaY * MipImageA->SizeY, UVDeltaY * MipImageB->SizeY,
                                                                  PrimitivePixelCountY);
                Result.InterpolationFactorAB = 0.5F * (InterpolationFactorX + InterpolationFactorY);
            }
            else if ((UVDeltaY * MipImageB->SizeX) > PrimitivePixelCountX)
            {
                // NOTE(Traian): Determine the interpolation factor based on the size on the X-axis (width).
                Result.InterpolationFactorAB = Math_InverseLerp(UVDeltaX * MipImageA->SizeX, UVDeltaX * MipImageB->SizeX,
                                                                PrimitivePixelCountX);
            }
            else if ((UVDeltaY * MipImageB->SizeY) > PrimitivePixelCountY)
            {
                // NOTE(Traian): Determine the interpolation factor based on the size on the Y-axis (height).
                Result.InterpolationFactorAB = Math_InverseLerp(UVDeltaY * MipImageA->SizeY, UVDeltaY * MipImageB->SizeY,
                                                                PrimitivePixelCountY);
            }
        }
    }
    else
    {
        // NOTE(Traian): We are forced to "downsample" the last mip - sad!
        Result.BlendBetweenMips = false;
        Result.MipLevelA = Texture->MipCount - 1;
    }

    return Result;
}

internal void
Renderer_DrawTexturedPrimitive(renderer* Renderer, renderer_image* RenderTarget, renderer_cluster* Cluster,
                               u32 PrimitiveIndex, rect2D ClusterDrawRegion)
{
    const renderer_primitive* Primitive = Cluster->Primitives + PrimitiveIndex;
    const rect2D PrimitiveRegion = Rect2D_Intersect({ Primitive->MinPoint, Primitive->MaxPoint }, ClusterDrawRegion);
    const renderer_texture* PrimitiveTexture = Renderer->TextureSlots[Primitive->TextureSlotIndex];

    renderer_rasterization_area RasterizationArea = Renderer_GetRasterizationArea(RenderTarget->SizeX,
                                                                                  RenderTarget->SizeY,
                                                                                  PrimitiveRegion);

    renderer_find_mip_levels_info FindMipsInfo = {};
    FindMipsInfo.NDCPrimitiveSizeX = Primitive->MaxPoint.X - Primitive->MinPoint.X;
    FindMipsInfo.NDCPrimitiveSizeY = Primitive->MaxPoint.Y - Primitive->MinPoint.Y;
    FindMipsInfo.ViewportSizeX = (f32)Renderer->ViewportSizeX;
    FindMipsInfo.ViewportSizeY = (f32)Renderer->ViewportSizeY;
    FindMipsInfo.UVDeltaX = Primitive->MaxUV.X - Primitive->MinUV.X;
    FindMipsInfo.UVDeltaY = Primitive->MaxUV.Y - Primitive->MinUV.Y;
    const renderer_find_mip_levels_result FindMipsResult = Renderer_FindMipLevels(PrimitiveTexture, FindMipsInfo);

    const vec2 MinGeometricPrimitive = Vec2(Primitive->MinPoint.X * RenderTarget->SizeX,
                                            Primitive->MinPoint.Y * RenderTarget->SizeY);
    const vec2 MaxGeometricPrimitive = Vec2(Primitive->MaxPoint.X * RenderTarget->SizeX,
                                            Primitive->MaxPoint.Y * RenderTarget->SizeY);

    if (RenderTarget->Format == RENDERER_IMAGE_FORMAT_B8G8R8A8)
    {
        u32* CurrentRowAddress = (u32*)Image_GetPixelAddress(RenderTarget,
                                                               RasterizationArea.PixelOffsetX,
                                                               RasterizationArea.PixelOffsetY);

        for (u32 PixelPositionY = RasterizationArea.PixelOffsetY;
             PixelPositionY < RasterizationArea.PixelOffsetY + RasterizationArea.PixelCountY;
             ++PixelPositionY)
        {
            u32* CurrentPixelAddress = CurrentRowAddress;
            for (u32 PixelPositionX = RasterizationArea.PixelOffsetX;
                 PixelPositionX < RasterizationArea.PixelOffsetX + RasterizationArea.PixelCountX;
                 ++PixelPositionX)
            {
                // NOTE(Traian): Unfortunately, this shenanigan is a caused by the error tolerance in the rasterization
                // function (otherwise, an out-of-bounds texture read might occur) - no easy fix!
                const f32 ERROR_TOLERANCE = 0.001F;
                const f32 PercentageX = Math_InverseLerp(MinGeometricPrimitive.X, MaxGeometricPrimitive.X + ERROR_TOLERANCE,
                                                         (f32)PixelPositionX + 0.5F);
                const f32 PercentageY = Math_InverseLerp(MinGeometricPrimitive.Y, MaxGeometricPrimitive.Y + ERROR_TOLERANCE,
                                                         (f32)PixelPositionY + 0.5F);

                // NOTE(Traian): Calculate the UV coordinate of this fragment.
                vec2 UV;
                UV.X = Math_Lerp(Primitive->MinUV.X, Primitive->MaxUV.X, PercentageX);
                UV.Y = Math_Lerp(Primitive->MinUV.Y, Primitive->MaxUV.Y, PercentageY);

                // NOTE(Traian): Sample from the mips.
                color4 SampledColor;
                if (PrimitiveTexture->Format == RENDERER_IMAGE_FORMAT_B8G8R8A8)
                {
                    if (FindMipsResult.BlendBetweenMips)
                    {
                        const renderer_image* MipImageA = PrimitiveTexture->Mips + FindMipsResult.MipLevelA;
                        const renderer_image* MipImageB = PrimitiveTexture->Mips + FindMipsResult.MipLevelB;
                        const color4 SampledA = Image_SampleBilinearB8G8R8A8(MipImageA, UV);
                        const color4 SampledB = Image_SampleBilinearB8G8R8A8(MipImageB, UV);
                        SampledColor = Math_LerpColor4(SampledA, SampledB, FindMipsResult.InterpolationFactorAB);
                    }
                    else
                    {
                        const renderer_image* MipImage = PrimitiveTexture->Mips + FindMipsResult.MipLevelA;
                        SampledColor = Image_SampleBilinearB8G8R8A8(MipImage, UV);
                    }

                    SampledColor.R *= Primitive->Color.R;
                    SampledColor.G *= Primitive->Color.G;
                    SampledColor.B *= Primitive->Color.B;
                    SampledColor.A *= Primitive->Color.A;
                }
                else
                {
                    f32 SampledAlpha;
                    if (FindMipsResult.BlendBetweenMips)
                    {
                        const renderer_image* MipImageA = PrimitiveTexture->Mips + FindMipsResult.MipLevelA;
                        const renderer_image* MipImageB = PrimitiveTexture->Mips + FindMipsResult.MipLevelB;
                        const f32 SampledA = Image_SampleBilinearA8(MipImageA, UV);
                        const f32 SampledB = Image_SampleBilinearA8(MipImageB, UV);
                        SampledAlpha = Math_Lerp(SampledA, SampledB, FindMipsResult.InterpolationFactorAB);
                    }
                    else
                    {
                        const renderer_image* MipImage = PrimitiveTexture->Mips + FindMipsResult.MipLevelA;
                        SampledAlpha = Image_SampleBilinearA8(MipImage, UV);
                    }
                    
                    SampledColor = Primitive->Color;
                    SampledColor.A *= SampledAlpha;
                }

                // NOTE(Traian): Output the color to the render target buffer.
                const color4 CurrentColor = Color4_FromLinear(LinearColor_UnpackFromBGRA(*CurrentPixelAddress));
                const color4 BlendedColor = Color4(Math_Lerp(CurrentColor.R, SampledColor.R, SampledColor.A),
                                                   Math_Lerp(CurrentColor.G, SampledColor.G, SampledColor.A),
                                                   Math_Lerp(CurrentColor.B, SampledColor.B, SampledColor.A));

                *CurrentPixelAddress = LinearColor_PackToBGRA(Color4_ToLinear(BlendedColor));
                ++CurrentPixelAddress;
            }
            CurrentRowAddress += RenderTarget->SizeX;
        }
    }
    else
    {
        // TODO(Traian): Eventually support more texture formats?
        PANIC("Image with non supported fomat was used as render target!");
    }
}

internal void
Renderer_ExecuteCluster(renderer* Renderer, renderer_image* RenderTarget, u32 ClusterIndex)
{
    renderer_cluster* Cluster = Renderer->Clusters + ClusterIndex;

    //
    // NOTE(Traian): Push all primitives that intersect the cluster draw region to the primitives buffer.
    //

    rect2D ClusterDrawRegion;
    const f32 InvViewportSizeX = 1.0F / (f32)Renderer->ViewportSizeX;
    const f32 InvViewportSizeY = 1.0F / (f32)Renderer->ViewportSizeY;
    ClusterDrawRegion.Min.X = (f32)Cluster->DrawRegionOffsetX * InvViewportSizeX;
    ClusterDrawRegion.Min.Y = (f32)Cluster->DrawRegionOffsetY * InvViewportSizeY;
    ClusterDrawRegion.Max.X = (f32)(Cluster->DrawRegionOffsetX + Cluster->DrawRegionSizeX) * InvViewportSizeX;
    ClusterDrawRegion.Max.Y = (f32)(Cluster->DrawRegionOffsetY + Cluster->DrawRegionSizeY) * InvViewportSizeY;

    for (u32 PrimitiveIndex = 0; PrimitiveIndex < Renderer->CurrentPrimitiveIndex; ++PrimitiveIndex)
    {
        const renderer_primitive* Primitive = Renderer->Primitives + PrimitiveIndex;
        const rect2D PrimitiveRegion = Rect2D_Intersect({ Primitive->MinPoint, Primitive->MaxPoint },
                                                        ClusterDrawRegion);
        if (!Rect2D_IsDegenerated(PrimitiveRegion))
        {
            if (Cluster->CurrentPrimitiveIndex >= Cluster->MaxPrimitiveCount)
            {
                PANIC("Renderer cluster primitive buffer overflown!");
            }

            Cluster->Primitives[Cluster->CurrentPrimitiveIndex] = *Primitive;
            Cluster->CurrentPrimitiveIndex++;
        }
    }

    //
    // NOTE(Traian): Sort the cluster primitive buffer.
    //

    Renderer_SortPrimitiveBuffer(Cluster->Primitives, Cluster->CurrentPrimitiveIndex);

    //
    // NOTE(Traian): Draw the primitives.
    //

    for (u32 PrimitiveIndex = 0; PrimitiveIndex < Cluster->CurrentPrimitiveIndex; ++PrimitiveIndex)
    {
        const renderer_primitive* Primitive = Cluster->Primitives + PrimitiveIndex;
        if (Primitive->TextureSlotIndex == -1)
        {
            Renderer_DrawFilledPrimitive(Renderer, RenderTarget, Cluster, PrimitiveIndex, ClusterDrawRegion);
        }
        else
        {
            Renderer_DrawTexturedPrimitive(Renderer, RenderTarget, Cluster, PrimitiveIndex, ClusterDrawRegion);
        }
    }
}

struct renderer_cluster_task_info
{
    renderer*       Renderer;
    renderer_image* RenderTarget;
    u32             ClusterIndex;
};

internal void
Renderer_RunClusterTask(s32 LogicalThreadIndex, void* OpaqueTaskInfo)
{
    const renderer_cluster_task_info* TaskInfo = (const renderer_cluster_task_info*)OpaqueTaskInfo;
    Renderer_ExecuteCluster(TaskInfo->Renderer, TaskInfo->RenderTarget, TaskInfo->ClusterIndex);
}

function void
Renderer_DispatchClusters(renderer* Renderer, renderer_image* RenderTarget, platform_task_queue* TaskQueue)
{
    ASSERT(RenderTarget->SizeX == Renderer->ViewportSizeX);
    ASSERT(RenderTarget->SizeY == Renderer->ViewportSizeY);

    const u32 MAX_TASK_INFO_COUNT = 64;
    renderer_cluster_task_info TaskInfos[MAX_TASK_INFO_COUNT] = {};
    u32 CurrentTaskInfoIndex = 0;

    for (u32 ClusterIndex = 0; ClusterIndex < Renderer->ClusterCount; ++ClusterIndex)
    {
        renderer_cluster_task_info* TaskInfo = &TaskInfos[CurrentTaskInfoIndex++];
        TaskInfo->Renderer = Renderer;
        TaskInfo->RenderTarget = RenderTarget;
        TaskInfo->ClusterIndex = ClusterIndex;

        PlatformTaskQueue_Push(TaskQueue, Renderer_RunClusterTask, TaskInfo);
    }

    PlatformTaskQueue_WaitForAll(TaskQueue);
}
