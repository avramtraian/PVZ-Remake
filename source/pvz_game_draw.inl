// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

//====================================================================================================================//
//------------------------------------------------------ SHAPES ------------------------------------------------------//
//====================================================================================================================//

internal inline void
GameDraw_RectangleFilled(game_state* GameState, vec2 MinPoint, vec2 MaxPoint, f32 ZOffset, color4 Color)
{
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                           ZOffset, Color);
}

internal inline void
GameDraw_Rectangle(game_state* GameState, vec2 MinPoint, vec2 MaxPoint, f32 BorderThickness, f32 ZOffset, color4 Color)
{
    const f32 SizeX = MaxPoint.X - MinPoint.X;
    const f32 SizeY = MaxPoint.Y - MinPoint.Y;

    const vec2 BMin = MinPoint;
    const vec2 BMax = BMin + Vec2(SizeX, BorderThickness);
    const vec2 RMin = MinPoint + Vec2(SizeX - BorderThickness, BorderThickness);
    const vec2 RMax = RMin + Vec2(BorderThickness, SizeY - 2.0F * BorderThickness);
    const vec2 TMin = MinPoint + Vec2(0.0F, SizeY - BorderThickness);
    const vec2 TMax = TMin + Vec2(SizeX, BorderThickness);
    const vec2 LMin = MinPoint + Vec2(0.0F, BorderThickness);
    const vec2 LMax = LMin + Vec2(BorderThickness, SizeY - 2.0F * BorderThickness);

    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, BMin),
                           Game_TransformGamePointToNDC(&GameState->Camera, BMax),
                           ZOffset, Color);
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, RMin),
                           Game_TransformGamePointToNDC(&GameState->Camera, RMax),
                           ZOffset, Color);
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, TMin),
                           Game_TransformGamePointToNDC(&GameState->Camera, TMax),
                           ZOffset, Color);
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, LMin),
                           Game_TransformGamePointToNDC(&GameState->Camera, LMax),
                           ZOffset, Color);
}

//====================================================================================================================//
//------------------------------------------------------- TEXT -------------------------------------------------------//
//====================================================================================================================//

internal u32
GameDraw_GetFontGlyphIndex(const asset_font* Font, u32 Codepoint)
{
    u32 GlyphIndex;
    for (GlyphIndex = 0; GlyphIndex < Font->GlyphCount; ++GlyphIndex)
    {
        if (Font->Glyphs[GlyphIndex].Codepoint == Codepoint)
        {
            break;
        }
    }
    return GlyphIndex;
}

internal rect2D
GameDraw_GetTextBoundingBox(const game_camera* Camera, const asset* FontAsset,
                            const char* Characters, memory_size CharacterCount,
                            vec2 Position, f32 Height)
{
    const vec2 NDCCursor = Game_TransformGamePointToNDC(Camera, Position);
    s32 CursorX = NDCCursor.X * Camera->ViewportPixelCountX;
    s32 CursorY = NDCCursor.Y * Camera->ViewportPixelCountY;

    s32 BoundingBoxMinX = S32_MAX;
    s32 BoundingBoxMinY = S32_MAX;
    s32 BoundingBoxMaxX = S32_MIN;
    s32 BoundingBoxMaxY = S32_MIN;
    
    const f32 Scale = Height / Game_PixelCountToGameDistanceY(Camera, FontAsset->Font.Height);
    // NOTE(Traian): Descent has a negative value.
    CursorY -= Scale * FontAsset->Font.Descent;

    for (u32 CharacterIndex = 0; CharacterIndex < CharacterCount; ++CharacterIndex)
    {
        const u32 GlyphIndex = GameDraw_GetFontGlyphIndex(&FontAsset->Font, Characters[CharacterIndex]);
        if (GlyphIndex < FontAsset->Font.GlyphCount)
        {
            const asset_font_glyph* Glyph = FontAsset->Font.Glyphs + GlyphIndex;

            const s32 GlyphMinX = CursorX + Scale * Glyph->TextureOffsetX;
            const s32 GlyphMinY = CursorY + Scale * Glyph->TextureOffsetY;
            const s32 GlyphMaxX = GlyphMinX + Scale * Glyph->RendererTexture.SizeX;
            const s32 GlyphMaxY = GlyphMinY + Scale * Glyph->RendererTexture.SizeY;

            BoundingBoxMinX = Min(BoundingBoxMinX, GlyphMinX);
            BoundingBoxMinY = Min(BoundingBoxMinY, GlyphMinY);
            BoundingBoxMaxX = Max(BoundingBoxMaxX, GlyphMaxX);
            BoundingBoxMaxY = Max(BoundingBoxMaxY, GlyphMaxY);

            CursorX += Scale * Glyph->AdvanceWidth;
            if ((CharacterIndex + 1) < CharacterCount)
            {
                const u32 NextGlyphIndex = GameDraw_GetFontGlyphIndex(&FontAsset->Font, Characters[CharacterIndex + 1]);
                if (NextGlyphIndex < FontAsset->Font.GlyphCount)
                {
                    const u32 KerningIndex = (GlyphIndex * FontAsset->Font.GlyphCount) + NextGlyphIndex;
                    CursorX += Scale * FontAsset->Font.KerningTable[KerningIndex];
                }
            }
        }
    }

    rect2D Result = {};
    if ((BoundingBoxMaxX >= BoundingBoxMinX) && (BoundingBoxMaxY >= BoundingBoxMinY))
    {
        const f32 NDCMinX = (f32)BoundingBoxMinX / (f32)Camera->ViewportPixelCountX;
        const f32 NDCMinY = (f32)BoundingBoxMinY / (f32)Camera->ViewportPixelCountY;
        const f32 NDCMaxX = (f32)BoundingBoxMaxX / (f32)Camera->ViewportPixelCountX;
        const f32 NDCMaxY = (f32)BoundingBoxMaxY / (f32)Camera->ViewportPixelCountY;

        Result.Min = Game_TransformNDCPointToGame(Camera, Vec2(NDCMinX, NDCMinY));
        Result.Max = Game_TransformNDCPointToGame(Camera, Vec2(NDCMaxX, NDCMaxY));
    }
    return Result;
}

internal vec2
GameDraw_GetTextRenderPositionFromCenter(const game_camera* Camera, const asset* FontAsset,
                                         const char* Characters, memory_size CharacterCount,
                                         vec2 CenterPosition, f32 Height)
{
    const rect2D BoundingBox = GameDraw_GetTextBoundingBox(Camera, FontAsset, Characters, CharacterCount,
                                                           Vec2(0.0F, 0.0F), Height);
    const vec2 BoundingBoxSize = Rect2D_GetSize(BoundingBox);

    vec2 Result;
    Result.X = CenterPosition.X - (0.5F * BoundingBoxSize.X) - (BoundingBox.Min.X); 
    Result.Y = CenterPosition.Y;
    return Result; 
}

internal void
GameDraw_Text(renderer* Renderer, const game_camera* Camera, const asset* FontAsset,
              const char* Characters, memory_size CharacterCount,
              vec2 Position, f32 ZOffset, f32 Height, color4 Color)
{
    const vec2 NDCPosition = Game_TransformGamePointToNDC(Camera, Position);
    s32 CursorX = NDCPosition.X * Camera->ViewportPixelCountX;
    s32 CursorY = NDCPosition.Y * Camera->ViewportPixelCountY;

    const f32 Scale = Height / Game_PixelCountToGameDistanceY(Camera, FontAsset->Font.Height);
    // NOTE(Traian): Descent has a negative value.
    CursorY -= Scale * FontAsset->Font.Descent;

    for (u32 CharacterIndex = 0; CharacterIndex < CharacterCount; ++CharacterIndex)
    {
        const u32 GlyphIndex = GameDraw_GetFontGlyphIndex(&FontAsset->Font, Characters[CharacterIndex]);
        if (GlyphIndex < FontAsset->Font.GlyphCount)
        {
            const asset_font_glyph* Glyph = FontAsset->Font.Glyphs + GlyphIndex;

            const s32 GlyphMinX = CursorX + Scale * Glyph->TextureOffsetX;
            const s32 GlyphMinY = CursorY + Scale * Glyph->TextureOffsetY;
            const s32 GlyphMaxX = GlyphMinX + Scale * Glyph->RendererTexture.SizeX;
            const s32 GlyphMaxY = GlyphMinY + Scale * Glyph->RendererTexture.SizeY;

            const f32 NDCGlyphMinX = (f32)GlyphMinX / (f32)Camera->ViewportPixelCountX;
            const f32 NDCGlyphMinY = (f32)GlyphMinY / (f32)Camera->ViewportPixelCountY;
            const f32 NDCGlyphMaxX = (f32)GlyphMaxX / (f32)Camera->ViewportPixelCountX;
            const f32 NDCGlyphMaxY = (f32)GlyphMaxY / (f32)Camera->ViewportPixelCountY;
            Renderer_PushPrimitive(Renderer, Vec2(NDCGlyphMinX, NDCGlyphMinY), Vec2(NDCGlyphMaxX, NDCGlyphMaxY),
                                   ZOffset, Color,
                                   Vec2(0.0F), Vec2(1.0F), &Glyph->RendererTexture);

            CursorX += Scale * Glyph->AdvanceWidth;
            if ((CharacterIndex + 1) < CharacterCount)
            {
                const u32 NextGlyphIndex = GameDraw_GetFontGlyphIndex(&FontAsset->Font, Characters[CharacterIndex + 1]);
                if (NextGlyphIndex < FontAsset->Font.GlyphCount)
                {
                    const u32 KerningIndex = (GlyphIndex * FontAsset->Font.GlyphCount) + NextGlyphIndex;
                    CursorX += Scale * FontAsset->Font.KerningTable[KerningIndex];
                }
            }
        }
    }
}

internal void
GameDraw_TextCentered(renderer* Renderer, const game_camera* Camera, const asset* FontAsset,
                      const char* Characters, memory_size CharacterCount,
                      vec2 CenterPosition, f32 ZOffset, f32 Height, color4 Color)
{
    const vec2 Position = GameDraw_GetTextRenderPositionFromCenter(Camera, FontAsset, Characters, CharacterCount,
                                                                   CenterPosition, Height);
    GameDraw_Text(Renderer, Camera, FontAsset, Characters, CharacterCount, Position, ZOffset, Height, Color);
}
