// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_renderer.h"

//====================================================================================================================//
//------------------------------------------------------ TEXTURE -----------------------------------------------------//
//====================================================================================================================//

function void
Texture_AllocateFromArena(renderer_texture* Texture, memory_arena* Arena,
                          memory_size BytesPerPixel, u32 SizeX, u32 SizeY)
{
    ZERO_STRUCT_POINTER(Texture);
    if (SizeX > 0 && SizeY > 0)
    {
        ASSERT(BytesPerPixel > 0);
        const memory_size PixelBufferSize = (memory_size)SizeX * (memory_size)SizeY * BytesPerPixel;
        Texture->PixelBuffer = MemoryArena_Allocate(Arena, PixelBufferSize, sizeof(void*));
        Texture->BytesPerPixel = BytesPerPixel;
        Texture->SizeX = SizeX;
        Texture->SizeY = SizeY;
    }
}

function void*
Texture_GetPixelAddress(renderer_texture* Texture, u32 PixelIndexX, u32 PixelIndexY)
{
    if (PixelIndexX < Texture->SizeX && PixelIndexY < Texture->SizeY)
    {
        const usize PixelIndex = ((usize)PixelIndexY * (usize)Texture->SizeX) + (usize)PixelIndexX;
        void* PixelAddress = (u8*)Texture->PixelBuffer + (PixelIndex * Texture->BytesPerPixel);
        return PixelAddress;
    }
    else
    {
        return NULL;
    }
}

function const void*
Texture_GetPixelAddress(const renderer_texture* Texture, u32 PixelIndexX, u32 PixelIndexY)
{
    if (PixelIndexX < Texture->SizeX && PixelIndexX < Texture->SizeY)
    {
        const usize PixelIndex = ((usize)PixelIndexY * (usize)Texture->SizeX) + (usize)PixelIndexX;
        const void* PixelAddress = (const u8*)Texture->PixelBuffer + (PixelIndex * Texture->BytesPerPixel);
        return PixelAddress;
    }
    else
    {
        return NULL;
    }
}

//====================================================================================================================//
//----------------------------------------------------- RENDERER -----------------------------------------------------//
//====================================================================================================================//

function void
Renderer_Initialize(renderer* Renderer, memory_arena* Arena)
{
    ZERO_STRUCT_POINTER(Renderer);
    Renderer->ClusterCount = 8;
    Renderer->MaxPrimitiveCount = 1024;
    Renderer->MaxTextureSlotCount = 64;

    Renderer->Clusters      = PUSH_ARRAY(Arena, renderer_cluster,   Renderer->ClusterCount);
    Renderer->Primitives    = PUSH_ARRAY(Arena, renderer_primitive, Renderer->MaxPrimitiveCount);
    Renderer->TextureSlots  = PUSH_ARRAY(Arena, renderer_texture*,  Renderer->MaxTextureSlotCount);

    for (u32 ClusterIndex = 0; ClusterIndex < Renderer->ClusterCount; ++ClusterIndex)
    {
        renderer_cluster* Cluster = Renderer->Clusters + ClusterIndex;
        Cluster->MaxPrimitiveCount = 128;
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
                       vec2 MinUV /*= {}*/, vec2 MaxUV /*= {}*/, renderer_texture* Texture /*= NULL*/)
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

struct renderer_quad_rasterization_area
{
    u32 PixelOffsetX;
    u32 PixelOffsetY;
    u32 PixelCountX;
    u32 PixelCountY;
};

internal renderer_quad_rasterization_area
Renderer_GetQuadRasterizationArea(f32 ViewportSizeX, f32 ViewportSizeY, rect2D QuadRegion)
{
    vec2 MinSamplePoint;
    MinSamplePoint.X = QuadRegion.Min.X * ViewportSizeX;
    MinSamplePoint.Y = QuadRegion.Min.Y * ViewportSizeY;
    vec2 MaxSamplePoint;
    MaxSamplePoint.X = QuadRegion.Max.X * ViewportSizeX;
    MaxSamplePoint.Y = QuadRegion.Max.Y * ViewportSizeY;

    s32 MinSampleIndexX = (s32)(MinSamplePoint.X + 0.5F);
    s32 MinSampleIndexY = (s32)(MinSamplePoint.Y + 0.5F);
    
    s32 MaxSampleIndexX = (s32)(MaxSamplePoint.X - 0.5F);
    if (MaxSamplePoint.X == (f32)MaxSampleIndexX + 0.5F)
    {
        --MaxSampleIndexX;
    }
    s32 MaxSampleIndexY = (s32)(MaxSamplePoint.Y - 0.5F);
    if (MaxSamplePoint.Y == (f32)MaxSampleIndexY + 0.5F)
    {
        --MaxSampleIndexY;
    }

    renderer_quad_rasterization_area RasterizationArea = {};
    RasterizationArea.PixelOffsetX = MinSampleIndexX;
    RasterizationArea.PixelOffsetY = MinSampleIndexY;
    RasterizationArea.PixelCountX = MaxSampleIndexX - MinSampleIndexX + 1;
    RasterizationArea.PixelCountY = MaxSampleIndexY - MinSampleIndexY + 1;
    return RasterizationArea;
}

internal void
Renderer_DrawFilledPrimitive(renderer* Renderer, renderer_texture* RenderTarget, renderer_cluster* Cluster,
                             u32 PrimitiveIndex, rect2D ClusterDrawRegion)
{
    const renderer_primitive* Primitive = Cluster->Primitives + PrimitiveIndex;
    const rect2D PrimitiveRegion = Rect2D_Intersect({ Primitive->MinPoint, Primitive->MaxPoint }, ClusterDrawRegion);

    const u32 BasePixelAddressX = PrimitiveRegion.Min.X * Renderer->ViewportSizeX;
    const u32 BasePixelAddressY = PrimitiveRegion.Min.Y * Renderer->ViewportSizeY;
    // NOTE(Traian): In order to avoid off-by-one errors in the number of pixels to draw, it is important to calculate
    // these counts by subtracting one integer from another, instead of calculating the size of the 'PrimitiveRegion'
    // rectangle (which is in the [0,1] range) and multiplying the result by the size of the viewport!
    const u32 PrimitivePixelCountX = (u32)(PrimitiveRegion.Max.X * Renderer->ViewportSizeX) - BasePixelAddressX;
    const u32 PrimitivePixelCountY = (u32)(PrimitiveRegion.Max.Y * Renderer->ViewportSizeY) - BasePixelAddressY;

    if (RenderTarget->BytesPerPixel == 4)
    {
        u32* CurrentRowAddress = (u32*)Texture_GetPixelAddress(RenderTarget, BasePixelAddressX, BasePixelAddressY);
        for (u32 PixelIndexY = 0; PixelIndexY < PrimitivePixelCountY; ++PixelIndexY)
        {
            u32* CurrentPixelAddress = CurrentRowAddress;
            for (u32 PixelIndexX = 0; PixelIndexX < PrimitivePixelCountX; ++PixelIndexX)
            {
                *CurrentPixelAddress = LinearColor_PackToBGRA(Color4_ToLinear(Primitive->Color));
                ++CurrentPixelAddress;
            }
            CurrentRowAddress += RenderTarget->SizeX;
        }
    }
    else
    {
        // TODO(Traian): Eventually support more texture formats?
        PANIC("Texture with non supported bytes-per-pixel was used as render target!");
    }
}

internal color4
Renderer_SampleTextureBPP4(const renderer_texture* Texture, vec2 UV)
{
    // NOTE(Traian): Bottom-left pixel.
    const u32 Tex1X = (u32)(UV.X * (f32)(Texture->SizeX - 1));
    const u32 Tex1Y = (u32)(UV.Y * (f32)(Texture->SizeY - 1));
    // NOTE(Traian): Bottom-right pixel.
    const u32 Tex2X = Tex1X + 1;
    const u32 Tex2Y = Tex1Y;
    // NOTE(Traian): Top-left pixel.
    const u32 Tex3X = Tex1X;
    const u32 Tex3Y = Tex1Y + 1;
    // NOTE(Traian): Top-right pixel.
    const u32 Tex4X = Tex1X + 1;
    const u32 Tex4Y = Tex1Y + 1;

    const u32* Pixel1Address = (const u32*)Texture_GetPixelAddress(Texture, Tex1X, Tex1Y);
    const u32* Pixel2Address = (const u32*)Texture_GetPixelAddress(Texture, Tex2X, Tex2Y);
    const u32* Pixel3Address = (const u32*)Texture_GetPixelAddress(Texture, Tex3X, Tex3Y);
    const u32* Pixel4Address = (const u32*)Texture_GetPixelAddress(Texture, Tex4X, Tex4Y);

    const color4 Sample1 = Color4_FromLinear(LinearColor_UnpackFromBGRA(*Pixel1Address));
    const color4 Sample2 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel2Address ? *Pixel2Address : *Pixel1Address));
    const color4 Sample3 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel3Address ? *Pixel3Address : *Pixel1Address));
    const color4 Sample4 = Color4_FromLinear(LinearColor_UnpackFromBGRA(Pixel4Address ? *Pixel4Address : *Pixel1Address));

    const f32 TX = UV.X - ((f32)Tex1X / (f32)Texture->SizeX);
    const f32 TY = UV.Y - ((f32)Tex1Y / (f32)Texture->SizeY);

    const color4 InterpolatedBottom = Math_LerpColor4(Sample1, Sample2, TX);
    const color4 InterpolatedTop    = Math_LerpColor4(Sample3, Sample4, TX);
    const color4 Interpolated       = Math_LerpColor4(InterpolatedBottom, InterpolatedTop, TY);

    return Interpolated;
}

internal void
Renderer_DrawTexturedPrimitive(renderer* Renderer, renderer_texture* RenderTarget, renderer_cluster* Cluster,
                               u32 PrimitiveIndex, rect2D ClusterDrawRegion)
{
    const renderer_primitive* Primitive = Cluster->Primitives + PrimitiveIndex;
    const rect2D PrimitiveRegion = Rect2D_Intersect({ Primitive->MinPoint, Primitive->MaxPoint }, ClusterDrawRegion);

    renderer_quad_rasterization_area RasterizationArea = Renderer_GetQuadRasterizationArea(RenderTarget->SizeX,
                                                                                           RenderTarget->SizeY,
                                                                                           PrimitiveRegion);
    
    const vec2 MinGeometricPrimitive = Vec2(Primitive->MinPoint.X * RenderTarget->SizeX,
                                            Primitive->MinPoint.Y * RenderTarget->SizeY);
    const vec2 MaxGeometricPrimitive = Vec2(Primitive->MaxPoint.X * RenderTarget->SizeX,
                                            Primitive->MaxPoint.Y * RenderTarget->SizeY);

    if (RenderTarget->BytesPerPixel == 4)
    {
        u32* CurrentRowAddress = (u32*)Texture_GetPixelAddress(RenderTarget,
                                                               RasterizationArea.PixelOffsetX,
                                                               RasterizationArea.PixelOffsetY);
        for (u32 PixelOffsetY = RasterizationArea.PixelOffsetY;
             PixelOffsetY < RasterizationArea.PixelOffsetY + RasterizationArea.PixelCountY;
             ++PixelOffsetY)
        {
            u32* CurrentPixelAddress = CurrentRowAddress;
            for (u32 PixelOffsetX = RasterizationArea.PixelOffsetX;
                 PixelOffsetX < RasterizationArea.PixelOffsetX + RasterizationArea.PixelCountX;
                 ++PixelOffsetX)
            {
                const f32 PercentageX = Math_InverseLerp(MinGeometricPrimitive.X, MaxGeometricPrimitive.X,
                                                         (f32)PixelOffsetX + 0.5F);
                const f32 PercentageY = Math_InverseLerp(MinGeometricPrimitive.Y, MaxGeometricPrimitive.Y,
                                                         (f32)PixelOffsetY + 0.5F);

                // NOTE(Traian): Calculate the UV coordinate of this fragment.
                vec2 UV;
                UV.X = Math_Lerp(Primitive->MinUV.X, Primitive->MaxUV.X, PercentageX);
                UV.Y = Math_Lerp(Primitive->MinUV.Y, Primitive->MaxUV.Y, PercentageY);

                // NOTE(Traian): Output the color to the render target buffer.
                const color4 SampledColor = Renderer_SampleTextureBPP4(Renderer->TextureSlots[Primitive->TextureSlotIndex],
                                                                       UV);
                const color4 CurrentColor = Color4_FromLinear(LinearColor_UnpackFromBGRA(*CurrentPixelAddress));
                const color4 BlendedColor = Color4(Math_Lerp(CurrentColor.R, SampledColor.R, SampledColor.A),
                                                   Math_Lerp(CurrentColor.G, SampledColor.G, SampledColor.A),
                                                   Math_Lerp(CurrentColor.B, SampledColor.B, SampledColor.A));

                *CurrentPixelAddress = LinearColor_PackToBGRA(Color4_ToLinear(BlendedColor));
                // *CurrentPixelAddress = LinearColor_PackToBGRA(Color4_ToLinear({ UV.X, UV.Y, 0.0F, 1.0F }));
                ++CurrentPixelAddress;
            }
            CurrentRowAddress += RenderTarget->SizeX;
        }
    }
    else
    {
        // TODO(Traian): Eventually support more texture formats?
        PANIC("Texture with non supported bytes-per-pixel was used as render target!");
    }
}

internal void
Renderer_ExecuteCluster(renderer* Renderer, renderer_texture* RenderTarget, u32 ClusterIndex)
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

function void
Renderer_DispatchClusters(renderer* Renderer, renderer_texture* RenderTarget)
{
    ASSERT(RenderTarget->SizeX == Renderer->ViewportSizeX);
    ASSERT(RenderTarget->SizeY == Renderer->ViewportSizeY);

    for (u32 ClusterIndex = 0; ClusterIndex < Renderer->ClusterCount; ++ClusterIndex)
    {
        Renderer_ExecuteCluster(Renderer, RenderTarget, ClusterIndex);
    }
}
