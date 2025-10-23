// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

//====================================================================================================================//
//---------------------------------------------------- INITIALIZE ----------------------------------------------------//
//====================================================================================================================//

internal void
GameSunCounter_Initialize(game_state* GameState)
{
    game_sun_counter* SunCounter = &GameState->SunCounter;

    SunCounter->BorderThickness = 0.05F;
    SunCounter->SunAmountCenterPercentage = Vec2(0.5F, 0.08F);
    SunCounter->SunAmountHeightPercentage = 0.27F;
    SunCounter->SunThumbnailCenterPercentage = Vec2(0.5F, 0.65F);
    SunCounter->SunThumbnailSizePercentage = Vec2(0.9F, 0.78F);
    SunCounter->SunCostShelfCenterPercentage = Vec2(0.5F, 0.2F);
    SunCounter->SunCostShelfSizePercentage = Vec2(0.8F, 0.22F);
}

//====================================================================================================================//
//------------------------------------------------------ UPDATE ------------------------------------------------------//
//====================================================================================================================//

internal void
GameSunCounter_Update(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    game_sun_counter* SunCounter = &GameState->SunCounter;

    const vec2 SUN_COUNTER_MIN_POINT_PERCENTAGE = Vec2(0.01F, 0.81F);
    const vec2 SUN_COUNTER_MAX_POINT_PERCENTAGE = Vec2(0.12F, 0.99F);

    SunCounter->MinPoint.X = GameState->Camera.UnitCountX * SUN_COUNTER_MIN_POINT_PERCENTAGE.X;
    SunCounter->MinPoint.Y = GameState->Camera.UnitCountY * SUN_COUNTER_MIN_POINT_PERCENTAGE.Y;
    SunCounter->MaxPoint.X = GameState->Camera.UnitCountX * SUN_COUNTER_MAX_POINT_PERCENTAGE.X;
    SunCounter->MaxPoint.Y = GameState->Camera.UnitCountY * SUN_COUNTER_MAX_POINT_PERCENTAGE.Y;

#ifdef PVZ_INTERNAL
    // NOTE(Traian): For debugging purposes, it is useful to have infinte sun amount.
    if (PlatformState->Input->Keys[GAME_INPUT_KEY_F1].WasPressedThisFrame)
    {
        SunCounter->SunAmount += 25;
    }
#endif // PVZ_INTERNAL
}

//====================================================================================================================//
//------------------------------------------------------ RENDER ------------------------------------------------------//
//====================================================================================================================//

internal void
GameSunCounter_Render(game_state* GameState, game_platform_state* PlatformState)
{
    game_sun_counter* SunCounter = &GameState->SunCounter;

    const f32 FRAME_OFFSET_Z                = 1.0F;
    const f32 SUN_THUMBNAIL_OFFSET_Z        = 2.0F;
    const f32 SUN_AMOUNT_SHELF_OFFSET_Z     = 3.0F;
    const f32 SUN_AMOUNT_TEXT_OFFSET_Z      = 4.0F;

    const color4 FRAME_BORDER_COLOR         = Color4_FromLinear(LinearColor(80, 50, 10));
    const color4 FRAME_BACKGROUND_COLOR     = Color4_FromLinear(LinearColor(110, 80, 40));
    const color4 SUN_AMOUNT_SHELF_COLOR     = Color4_FromLinear(LinearColor(210, 230, 190));
    const color4 SUN_AMOUNT_TEXT_COLOR      = Color4_FromLinear(LinearColor(15, 10, 5));

    //
    // NOTE(Traian): Render the sun counter frame.
    //

    GameDraw_Rectangle(GameState, SunCounter->MinPoint, SunCounter->MaxPoint, SunCounter->BorderThickness,
                       FRAME_OFFSET_Z, Color4_FromLinear(LinearColor(80, 50, 10)));
    
    GameDraw_RectangleFilled(GameState,
                             SunCounter->MinPoint + Vec2(SunCounter->BorderThickness),
                             SunCounter->MaxPoint - Vec2(SunCounter->BorderThickness),
                             FRAME_OFFSET_Z, FRAME_BACKGROUND_COLOR);
    
    //
    // NOTE(Traian): Render the sun thumbnail.
    //

    vec2 ThumbnailCenter;
    ThumbnailCenter.X = Math_Lerp(SunCounter->MinPoint.X, SunCounter->MaxPoint.X, SunCounter->SunThumbnailCenterPercentage.X);
    ThumbnailCenter.Y = Math_Lerp(SunCounter->MinPoint.Y, SunCounter->MaxPoint.Y, SunCounter->SunThumbnailCenterPercentage.Y);

    vec2 ThumbnailSize;
    ThumbnailSize.X = SunCounter->SunThumbnailSizePercentage.X * (SunCounter->MaxPoint.X - SunCounter->MinPoint.X);
    ThumbnailSize.Y = SunCounter->SunThumbnailSizePercentage.Y * (SunCounter->MaxPoint.Y - SunCounter->MinPoint.Y);

    asset* SunThumbnailTexture = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PROJECTILE_SUN);
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, ThumbnailCenter - 0.5F * ThumbnailSize),
                           Game_TransformGamePointToNDC(&GameState->Camera, ThumbnailCenter + 0.5F * ThumbnailSize),
                           SUN_THUMBNAIL_OFFSET_Z, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                           &SunThumbnailTexture->Texture.RendererTexture);
    
    //
    // NOTE(Traian): Render the sun amount shelf.
    //

    vec2 ShelfCenter;
    ShelfCenter.X = Math_Lerp(SunCounter->MinPoint.X, SunCounter->MaxPoint.X, SunCounter->SunCostShelfCenterPercentage.X);
    ShelfCenter.Y = Math_Lerp(SunCounter->MinPoint.Y, SunCounter->MaxPoint.Y, SunCounter->SunCostShelfCenterPercentage.Y);

    vec2 ShelfSize;
    ShelfSize.X = SunCounter->SunCostShelfSizePercentage.X * (SunCounter->MaxPoint.X - SunCounter->MinPoint.X);
    ShelfSize.Y = SunCounter->SunCostShelfSizePercentage.Y * (SunCounter->MaxPoint.Y - SunCounter->MinPoint.Y);

    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, ShelfCenter - 0.5F * ShelfSize),
                           Game_TransformGamePointToNDC(&GameState->Camera, ShelfCenter + 0.5F * ShelfSize),
                           SUN_AMOUNT_SHELF_OFFSET_Z, SUN_AMOUNT_SHELF_COLOR);

    //
    // NOTE(Traian): Render the sun amount text.
    //

    char SunAmountCharacters[8] = {};
    const memory_size SunAmountCharacterCount = String_FromUnsignedInteger(SunAmountCharacters, sizeof(SunAmountCharacters),
                                                                           SunCounter->SunAmount, STRING_NUMBER_BASE_DEC);

    const f32 TextHeight = SunCounter->SunAmountHeightPercentage * (SunCounter->MaxPoint.Y - SunCounter->MinPoint.Y);
    vec2 SunAmountCenter;
    SunAmountCenter.X = Math_Lerp(SunCounter->MinPoint.X, SunCounter->MaxPoint.X, SunCounter->SunAmountCenterPercentage.X);
    SunAmountCenter.Y = Math_Lerp(SunCounter->MinPoint.Y, SunCounter->MaxPoint.Y, SunCounter->SunAmountCenterPercentage.Y);

    asset* FontAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_FONT_COMIC_SANS);
    GameDraw_TextCentered(&GameState->Renderer, &GameState->Camera, FontAsset, SunAmountCharacters, SunAmountCharacterCount,
                          SunAmountCenter, SUN_AMOUNT_TEXT_OFFSET_Z, TextHeight, SUN_AMOUNT_TEXT_COLOR);
}
