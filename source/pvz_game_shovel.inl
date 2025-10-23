// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

//====================================================================================================================//
//---------------------------------------------------- INITIALIZE ----------------------------------------------------//
//====================================================================================================================//

internal void
GameShovel_Initialize(game_state* GameState)
{
    game_shovel* Shovel = &GameState->Shovel;
    Shovel->BorderThickness = 0.05F;
    Shovel->ThumbnailCenterPercentage = Vec2(0.5F, 0.5F);
    Shovel->ThumbnailDimensionsPercentage = Vec2(0.8F, 0.7F);
}

//====================================================================================================================//
//------------------------------------------------------ UPDATE ------------------------------------------------------//
//====================================================================================================================//

internal void
GameShovel_RemovePlant(game_state* GameState)
{
    game_shovel* Shovel = &GameState->Shovel;
    game_garden_grid* GardenGrid = &GameState->GardenGrid;

    // NOTE(Traian): Sanity check.
    ASSERT(Shovel->IsSelected);

    const s32 GardenGridCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, Shovel->ToolCenterPosition.X);
    const s32 GardenGridCellIndexY = GameGardenGrid_GetCellIndexY(GardenGrid, Shovel->ToolCenterPosition.Y);
    if ((0 <= GardenGridCellIndexX && GardenGridCellIndexX < GardenGrid->CellCountX) &&
        (0 <= GardenGridCellIndexY && GardenGridCellIndexY < GardenGrid->CellCountY))
    {
        const u32 PlantEntityIndex = (GardenGridCellIndexY * GardenGrid->CellCountX) + GardenGridCellIndexX;
        plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;
        if (PlantEntity->Type != PLANT_TYPE_NONE && !PlantEntity->IsPendingDestroy)
        {
            PlantEntity->IsPendingDestroy = true;
        }
    }
}

internal void
GameShovel_Update(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    game_shovel* Shovel = &GameState->Shovel;

    const vec2 SHOVEL_MIN_POINT_PERCENTAGE = Vec2(0.88F, 0.81F);
    const vec2 SHOVEL_MAX_POINT_PERCENTAGE = Vec2(0.99F, 0.99F);

    Shovel->MinPoint.X = GameState->Camera.UnitCountX * SHOVEL_MIN_POINT_PERCENTAGE.X;
    Shovel->MinPoint.Y = GameState->Camera.UnitCountY * SHOVEL_MIN_POINT_PERCENTAGE.Y;
    Shovel->MaxPoint.X = GameState->Camera.UnitCountX * SHOVEL_MAX_POINT_PERCENTAGE.X;
    Shovel->MaxPoint.Y = GameState->Camera.UnitCountY * SHOVEL_MAX_POINT_PERCENTAGE.Y;

    Shovel->ThumbnailCenterPosition.X = Math_Lerp(Shovel->MinPoint.X,Shovel->MaxPoint.X, Shovel->ThumbnailCenterPercentage.X);
    Shovel->ThumbnailCenterPosition.Y = Math_Lerp(Shovel->MinPoint.Y,Shovel->MaxPoint.Y, Shovel->ThumbnailCenterPercentage.Y);

    Shovel->ThumbnailDimensions.X = Shovel->ThumbnailDimensionsPercentage.X * (Shovel->MaxPoint.X - Shovel->MinPoint.X);
    Shovel->ThumbnailDimensions.Y = Shovel->ThumbnailDimensionsPercentage.Y * (Shovel->MaxPoint.Y - Shovel->MinPoint.Y);

    const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                Vec2(PlatformState->Input->MousePositionX,
                                                                     PlatformState->Input->MousePositionY));
    //
    // NOTE(Traian): Begin drag-and-drop action.
    //

    if (PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].WasPressedThisFrame)
    {
        if ((Shovel->ThumbnailCenterPosition.X - 0.5F * Shovel->ThumbnailDimensions.X <= GameMousePosition.X) &&
            (Shovel->ThumbnailCenterPosition.X + 0.5F * Shovel->ThumbnailDimensions.X > GameMousePosition.X) &&
            (Shovel->ThumbnailCenterPosition.Y - 0.5F * Shovel->ThumbnailDimensions.Y <= GameMousePosition.Y) &&
            (Shovel->ThumbnailCenterPosition.Y + 0.5F * Shovel->ThumbnailDimensions.Y > GameMousePosition.Y))
        {
            Shovel->IsSelected = true;
            Shovel->ToolCenterPosition = GameMousePosition;
        }
    }

    //
    // NOTE(Traian): Determine where is the tool located.
    //

    if (Shovel->IsSelected)
    {
        b8 SnapToGardenGrid = false;

        const game_garden_grid* GardenGrid = &GameState->GardenGrid;
        const s32 GardenGridCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, GameMousePosition.X);
        const s32 GardenGridCellIndexY = GameGardenGrid_GetCellIndexY(GardenGrid, GameMousePosition.Y);
        if ((0 <= GardenGridCellIndexX && GardenGridCellIndexX < GardenGrid->CellCountX) &&
            (0 <= GardenGridCellIndexY && GardenGridCellIndexY < GardenGrid->CellCountY))
        {
            const u32 PlantEntityIndex = (GardenGridCellIndexY * GardenGrid->CellCountX) + GardenGridCellIndexX;
            plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;
            if (PlantEntity->Type != PLANT_TYPE_NONE)
            {
                SnapToGardenGrid = true;
            }
        }

        if (SnapToGardenGrid)
        {
            vec2 CellPoint;
            CellPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, ((f32)GardenGridCellIndexX + 0.5F) / (f32)GardenGrid->CellCountX);
            CellPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, ((f32)GardenGridCellIndexY + 0.5F) / (f32)GardenGrid->CellCountY);
            Shovel->ToolCenterPosition = CellPoint;
        }
        else
        {
            Shovel->ToolCenterPosition = GameMousePosition;
        }
    }

    //
    // NOTE(Traian): End (and execute) the drag-and-drop action.
    //

    if (PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].WasReleasedThisFrame)
    {
        if (Shovel->IsSelected)
        {
            GameShovel_RemovePlant(GameState);
            Shovel->IsSelected = false;
        }
    }
    else if (!PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].IsDown)
    {
        Shovel->IsSelected = false;
        Shovel->ToolCenterPosition = {};
    }
}

//====================================================================================================================//
//------------------------------------------------------ RENDER ------------------------------------------------------//
//====================================================================================================================//

internal void
GameShovel_Render(game_state* GameState, game_platform_state* PlatformState)
{
    game_shovel* Shovel = &GameState->Shovel;

    const f32 FRAME_OFFSET_Z            = 1.0F;
    const f32 THUMBNAIL_OFFSET_Z        = 2.0F;
    const f32 TOOL_OFFSET_Z             = 10.0F;

    const color4 FRAME_BORDER_COLOR     = Color4_FromLinear(LinearColor(80, 50, 10));
    const color4 FRAME_BACKGROUND_COLOR = Color4_FromLinear(LinearColor(110, 80, 40));

    //
    // NOTE(Traian): Render the shovel frame.
    //

    GameDraw_Rectangle(GameState, Shovel->MinPoint, Shovel->MaxPoint, Shovel->BorderThickness,
                       FRAME_OFFSET_Z, Color4_FromLinear(LinearColor(80, 50, 10)));
    
    GameDraw_RectangleFilled(GameState,
                             Shovel->MinPoint + Vec2(Shovel->BorderThickness),
                             Shovel->MaxPoint - Vec2(Shovel->BorderThickness),
                             FRAME_OFFSET_Z, FRAME_BACKGROUND_COLOR);

    //
    // NOTE(Traian): Render the shovel thumbnail.
    //

    if (!Shovel->IsSelected)
    {
        asset* ThumbnailTexture = Asset_Get(&GameState->Assets, GAME_ASSET_ID_UI_SHOVEL);
        Renderer_PushPrimitive(&GameState->Renderer,
                               Game_TransformGamePointToNDC(&GameState->Camera, Shovel->ThumbnailCenterPosition - 0.5F * Shovel->ThumbnailDimensions),
                               Game_TransformGamePointToNDC(&GameState->Camera, Shovel->ThumbnailCenterPosition + 0.5F * Shovel->ThumbnailDimensions),
                               THUMBNAIL_OFFSET_Z, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                               &ThumbnailTexture->Texture.RendererTexture);
    }

    //
    // NOTE(Traian): Render the tool.
    //

    if (Shovel->IsSelected)
    {
        asset* ThumbnailTexture = Asset_Get(&GameState->Assets, GAME_ASSET_ID_UI_SHOVEL);
        Renderer_PushPrimitive(&GameState->Renderer,
                               Game_TransformGamePointToNDC(&GameState->Camera, Shovel->ToolCenterPosition - 0.5F * Shovel->ThumbnailDimensions),
                               Game_TransformGamePointToNDC(&GameState->Camera, Shovel->ToolCenterPosition + 0.5F * Shovel->ThumbnailDimensions),
                               TOOL_OFFSET_Z, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                               &ThumbnailTexture->Texture.RendererTexture);
    }
}
