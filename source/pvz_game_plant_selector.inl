// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

//====================================================================================================================//
//---------------------------------------------------- INITIALIZE ----------------------------------------------------//
//====================================================================================================================//

internal void
GamePlantSelector_Initialize(game_state* GameState)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;

    PlantSelector->BorderThickness = 0.05F;
    PlantSelector->SeedPacketBorderPadding = 0.03F;
    PlantSelector->SeedPacketSpace = 0.03F;
    PlantSelector->SeedPacketAspectRatio = 1.0F / 1.4F;

    PlantSelector->SeedPacketCount = 8;
    PlantSelector->SeedPackets = PUSH_ARRAY(GameState->PermanentArena, game_seed_packet,
                                            PlantSelector->SeedPacketCount);
    for (u32 SeedPacketIndex = 0; SeedPacketIndex < PlantSelector->SeedPacketCount; ++SeedPacketIndex)
    {
        game_seed_packet* SeedPacket = PlantSelector->SeedPackets + SeedPacketIndex;
        SeedPacket->SunCost = 25;
        SeedPacket->SunCostCenterPercentage = Vec2(0.4F, 0.03F);
        SeedPacket->SunCostHeightPercentage = 0.24F;
        SeedPacket->ThumbnailCenterPercentage = Vec2(0.5F, 0.55F);
        SeedPacket->ThumbnailSizePercentage = Vec2(0.7F, 0.45F);
    }

    u32 SeedPacketIndex = 0;
    for (u16 PlantType = PLANT_TYPE_NONE + 1; PlantType < PLANT_TYPE_MAX_COUNT; ++PlantType)
    {
        const game_plant_config PlantConfig = GameState->Config.Plants[PlantType];
        PlantSelector->SeedPackets[SeedPacketIndex].PlantType = (plant_type)PlantType;
        PlantSelector->SeedPackets[SeedPacketIndex].SunCost = PlantConfig.SunCost;
        PlantSelector->SeedPackets[SeedPacketIndex].CooldownDelay = PlantConfig.PlantCooldownDelay;
        ++SeedPacketIndex;
    }
}

//====================================================================================================================//
//------------------------------------------------------ UPDATE ------------------------------------------------------//
//====================================================================================================================//

internal inline u32
GamePlantSelector_GetSeedPacketIndex(const game_plant_selector* PlantSelector, vec2 Position)
{
    u32 Result = PlantSelector->SeedPacketCount + 1;
    if ((PlantSelector->MinPoint.X <= Position.X && Position.X < PlantSelector->MaxPoint.X) &&
        (PlantSelector->MinPoint.Y <= Position.Y && Position.Y < PlantSelector->MaxPoint.Y))
    {
        vec2 SeedPacketOffset;
        SeedPacketOffset.X = PlantSelector->MinPoint.X + PlantSelector->BorderThickness + PlantSelector->SeedPacketBorderPadding;
        SeedPacketOffset.Y = PlantSelector->MinPoint.Y + PlantSelector->BorderThickness + PlantSelector->SeedPacketBorderPadding;

        for (u32 SeedPacketIndex = 0; SeedPacketIndex < PlantSelector->SeedPacketCount; ++SeedPacketIndex)
        {
            const game_seed_packet* SeedPacket = PlantSelector->SeedPackets + SeedPacketIndex;
            const vec2 SeedPacketMinPoint = SeedPacketOffset;
            const vec2 SeedPacketMaxPoint = SeedPacketOffset + PlantSelector->SeedPacketSize;
            const f32 SeedPacketSizeX = SeedPacketMaxPoint.X - SeedPacketMinPoint.X;
            const f32 SeedPacketSizeY = SeedPacketMaxPoint.Y - SeedPacketMinPoint.Y;

            if ((SeedPacketMinPoint.X <= Position.X && Position.X < SeedPacketMaxPoint.X) &&
                (SeedPacketMinPoint.Y <= Position.Y && Position.Y < SeedPacketMaxPoint.Y))
            {
                Result = SeedPacketIndex;
                break;
            }

            SeedPacketOffset.X += PlantSelector->SeedPacketSize.X;
            SeedPacketOffset.X += PlantSelector->SeedPacketSpace;
        }
    }

    return Result;
}

internal inline rect2D
GamePlantSelector_GetSeedPacketRectangle(const game_plant_selector* PlantSelector, u32 SeedPacketIndex)
{
    ASSERT(SeedPacketIndex < PlantSelector->SeedPacketCount);

    vec2 SeedPacketOffset;
    SeedPacketOffset.X = PlantSelector->MinPoint.X + PlantSelector->BorderThickness + PlantSelector->SeedPacketBorderPadding;
    SeedPacketOffset.Y = PlantSelector->MinPoint.Y + PlantSelector->BorderThickness + PlantSelector->SeedPacketBorderPadding;
    SeedPacketOffset.X += SeedPacketIndex * (PlantSelector->SeedPacketSize.X + PlantSelector->SeedPacketSpace);

    rect2D Result;
    Result.Min = SeedPacketOffset;
    Result.Max = SeedPacketOffset + PlantSelector->SeedPacketSize;
    return Result;
}

internal void
GamePLantSelector_PlantSeedPacket(game_state* GameState, vec2 GameMousePosition)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;
    game_garden_grid* GardenGrid = &GameState->GardenGrid;

    // NOTE(Traian): Sanity checks.
    ASSERT(PlantSelector->HasSeedPacketSelected);
    ASSERT(PlantSelector->SelectedSeedPacketIndex < PlantSelector->SeedPacketCount);

    const s32 GardenGridCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, GameMousePosition.X);
    const s32 GardenGridCellIndexY = GameGardenGrid_GetCellIndexY(GardenGrid, GameMousePosition.Y);
    if ((0 <= GardenGridCellIndexX && GardenGridCellIndexX < GardenGrid->CellCountX) &&
        (0 <= GardenGridCellIndexY && GardenGridCellIndexY < GardenGrid->CellCountY))
    {
        game_seed_packet* SeedPacket = PlantSelector->SeedPackets + PlantSelector->SelectedSeedPacketIndex;
        const u32 PlantEntityIndex = (GardenGridCellIndexY * GardenGrid->CellCountX) + GardenGridCellIndexX;
        plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;

        if (PlantEntity->Type == PLANT_TYPE_NONE)
        {
            // NOTE(Traian): Configure the plant entity's "generic" settings.
            if (SeedPacket->PlantType != PLANT_TYPE_NONE && SeedPacket->PlantType < PLANT_TYPE_MAX_COUNT)
            {
                const game_plant_config* PlantConfig = &GameState->Config.Plants[SeedPacket->PlantType];
                PlantEntity->Type = SeedPacket->PlantType;
                PlantEntity->Health = PlantConfig->Health;
            }

            switch (SeedPacket->PlantType)
            {
                case PLANT_TYPE_SUNFLOWER:
                {
                    // NOTE(Traian): "Plant" a sunflower.
                    PlantEntity->Sunflower.GenerateDelayBase = PLANT_SUNFLOWER_GENERATE_SUN_DELAY_BASE;
                    PlantEntity->Sunflower.GenerateDelayRandomOffset = PLANT_SUNFLOWER_GENERATE_SUN_DELAY_RANDOM_OFFSET;
                    PlantEntity->Sunflower.SunRadius = PLANT_SUNFLOWER_SUN_RADIUS;
                    PlantEntity->Sunflower.SunAmount = PLANT_SUNFLOWER_SUN_AMOUNT;
                    PlantEntity->Sunflower.SunDecayDelay = PLANT_SUNFLOWER_SUN_DECAY;
                }
                break;

                case PLANT_TYPE_PEASHOOTER:
                {
                    // NOTE(Traian): "Plant" a peashooter.
                    PlantEntity->Peashooter.ShootDelay = PLANT_PEASHOOTER_SHOOT_DELAY;
                    PlantEntity->Peashooter.ProjectileDamage = PLANT_PEASHOOTER_PROJECTILE_DAMAGE;
                    PlantEntity->Peashooter.ProjectileVelocity = PLANT_PEASHOOTER_PROJECTILE_VELOCITY;
                    PlantEntity->Peashooter.ProjectileRadius = PLANT_PEASHOOTER_PROJECTILE_RADIUS;
                }
                break;

                case PLANT_TYPE_REPEATER:
                {
                    // NOTE(Traian): "Plant" a repeater.
                    PlantEntity->Repeater.ShootSequenceDelay = PLANT_REPEATER_SHOOT_SEQUENCE_DELAY;
                    PlantEntity->Repeater.ShootSequenceDeltaDelay = PLANT_REPEATER_SHOOT_SEQUENCE_DELTA_DELAY;
                    PlantEntity->Repeater.ProjectileDamage = PLANT_REPEATER_PROJECTILE_DAMAGE;
                    PlantEntity->Repeater.ProjectileVelocity = PLANT_REPEATER_PROJECTILE_VELOCITY;
                    PlantEntity->Repeater.ProjectileRadius = PLANT_REPEATER_PROJECTILE_RADIUS;                    
                }
                break;

                case PLANT_TYPE_TORCHWOOD:
                {
                    // NOTE(Traian): "Plant" a torchwood.
                    PlantEntity->Torchwood.DamageMultiplier = PLANT_TORCHWOOD_DAMAGE_MULTIPLIER;
                }
                break;

                case PLANT_TYPE_MELONPULT:
                {
                    // NOTE(Traian): "Plant" a melonpult.
                    PlantEntity->Melonpult.LaunchDelay = PLANT_MELONPULT_LAUNCH_DELAY;
                    PlantEntity->Melonpult.ProjectileDamage = PLANT_MELONPULT_PROJECTILE_DAMAGE;
                    PlantEntity->Melonpult.ProjectileRadius = PLANT_MELONPULT_PROJECTILE_RADIUS;
                    PlantEntity->Melonpult.ProjectileVelocity = PLANT_MELONPULT_PROJECTILE_VELOCITY;
                    PlantEntity->Melonpult.ProjectileSplashDamageRadius = PLANT_MELONPULT_PROJECTILE_SPLASH_DAMAGE_RADIUS;
                    PlantEntity->Melonpult.ProjectileSplashDamageMultiplier = PLANT_MELONPULT_PROJECTILE_SPLASH_DAMAGE_MULTIPLIER;
                }
                break;
            }

            if (SeedPacket->PlantType != PLANT_TYPE_NONE && SeedPacket->CooldownDelay > 0.0F)
            {
                SeedPacket->IsInCooldown = true;
                SeedPacket->CooldownTimer = 0.0F;
                
                ASSERT(GameState->SunCounter.SunAmount >= SeedPacket->SunCost);
                GameState->SunCounter.SunAmount -= SeedPacket->SunCost;
            }
        }
        else
        {
            // TODO(Traian): Notify the user that the plant operation has failed - for example,
            // play a sound effect.
        }
    }
}

internal void
GamePlantSelector_Update(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;

    const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                Vec2(PlatformState->Input->MousePositionX,
                                                                     PlatformState->Input->MousePositionY));

    //
    // NOTE(Traian): Calculate the plant selector dimensions.
    //

    const vec2  PLANT_SELECTOR_MIN_POINT_PERCENTAGE = Vec2(0.13F, 0.81F);
    const f32   PLANT_SELECTOR_HEIGHT_PERCENTAGE    = 0.18F;

    vec2 MinPoint;
    vec2 MaxPoint;
    MinPoint.X = GameState->Camera.UnitCountX * PLANT_SELECTOR_MIN_POINT_PERCENTAGE.X;
    MinPoint.Y = GameState->Camera.UnitCountY * PLANT_SELECTOR_MIN_POINT_PERCENTAGE.Y;
    MaxPoint.Y = GameState->Camera.UnitCountY * (PLANT_SELECTOR_MIN_POINT_PERCENTAGE.Y + PLANT_SELECTOR_HEIGHT_PERCENTAGE);

    PlantSelector->SeedPacketSize.Y = (MaxPoint.Y - MinPoint.Y) -
                                      2.0F * PlantSelector->BorderThickness -
                                      2.0F * PlantSelector->SeedPacketBorderPadding;
    PlantSelector->SeedPacketSize.X = PlantSelector->SeedPacketSize.Y * PlantSelector->SeedPacketAspectRatio;

    f32 PlantSelectorSizeX = 0.0F;
    PlantSelectorSizeX += 2.0F * (PlantSelector->BorderThickness + PlantSelector->SeedPacketBorderPadding);
    PlantSelectorSizeX += PlantSelector->SeedPacketCount * PlantSelector->SeedPacketSize.X;
    PlantSelectorSizeX += (PlantSelector->SeedPacketCount - 1) * PlantSelector->SeedPacketSpace;

    MaxPoint.X = MinPoint.X + PlantSelectorSizeX;
    PlantSelector->MinPoint = MinPoint;
    PlantSelector->MaxPoint = MaxPoint;

    //
    // NOTE(Traian): Update the plant cooldowns.
    //

    for (u32 SeedPacketIndex = 0; SeedPacketIndex < PlantSelector->SeedPacketCount; ++SeedPacketIndex)
    {
        game_seed_packet* SeedPacket = PlantSelector->SeedPackets + SeedPacketIndex;
        if (SeedPacket->IsInCooldown)
        {
            if (SeedPacket->CooldownTimer >= SeedPacket->CooldownDelay)
            {
                SeedPacket->CooldownTimer = 0.0F;
                SeedPacket->IsInCooldown = false;
            }
            else
            {
                SeedPacket->CooldownTimer += DeltaTime;
            }
        }
    }

    //
    // NOTE(Traian): Implement drag-and-drop for seed packets into the grid.
    //

    if (PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].WasPressedThisFrame)
    {
        const u32 SelectedSeedPacketIndex = GamePlantSelector_GetSeedPacketIndex(PlantSelector, GameMousePosition);
        if (SelectedSeedPacketIndex < PlantSelector->SeedPacketCount)
        {
            const game_seed_packet* SeedPacket = PlantSelector->SeedPackets + SelectedSeedPacketIndex;
            if (!SeedPacket->IsInCooldown && (GameState->SunCounter.SunAmount >= SeedPacket->SunCost))
            {
                PlantSelector->HasSeedPacketSelected = true;
                PlantSelector->SelectedSeedPacketIndex = SelectedSeedPacketIndex;
            }
        }
    }

    if (PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].WasReleasedThisFrame)
    {
        if (PlantSelector->HasSeedPacketSelected)
        {
            GamePLantSelector_PlantSeedPacket(GameState, GameMousePosition);
            PlantSelector->HasSeedPacketSelected = false;
            PlantSelector->SelectedSeedPacketIndex = 0;
            PlantSelector->PlantPreviewCenterPosition = {};
        }
    }
    else if (!PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].IsDown)
    {
        PlantSelector->HasSeedPacketSelected = false;
        PlantSelector->SelectedSeedPacketIndex = 0;
        PlantSelector->PlantPreviewCenterPosition = {};
    }

    //
    // NOTE(Traian): Determine where to render the plant preview.
    //

    if (PlantSelector->HasSeedPacketSelected)
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
            if (PlantEntity->Type == PLANT_TYPE_NONE)
            {
                vec2 CellPoint;
                CellPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, ((f32)GardenGridCellIndexX + 0.5F) / (f32)GardenGrid->CellCountX);
                CellPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, ((f32)GardenGridCellIndexY + 0.5F) / (f32)GardenGrid->CellCountY);
                PlantSelector->PlantPreviewCenterPosition = CellPoint;
                SnapToGardenGrid = true;
            }
        }

        if (!SnapToGardenGrid)
        {
            PlantSelector->PlantPreviewCenterPosition = GameMousePosition;
        }
    }
}

//====================================================================================================================//
//------------------------------------------------------ RENDER ------------------------------------------------------//
//====================================================================================================================//

internal const f32 PLANT_SELECTOR_FRAME_OFFSET_Z                            = 1.0F;
internal const f32 PLANT_SELECTOR_SEED_PACKET_BACKGROUND_OFFSET_Z           = 1.0F;
internal const f32 PLANT_SELECTOR_SEED_PACKET_THUMBNAIL_OFFSET_Z            = 2.0F;
internal const f32 PLANT_SELECTOR_SEED_PACKET_COOLDOWN_COVER_OFFSET_Z       = 3.0F;
internal const f32 PLANT_SELECTOR_SEED_PACKET_COST_OFFSET_Z                 = 4.0F;
internal const f32 PLANT_SELECTOR_PLANT_PREVIEW_OFFSET_Z                    = 10.0F;

internal const color4 PLANT_SELECTOR_FRAME_BORDER_COLOR                     = Color4_FromLinear(LinearColor(80, 50, 10));
internal const color4 PLANT_SELECTOR_FRAME_BACKGROUND_COLOR                 = Color4_FromLinear(LinearColor(110, 80, 40));
internal const color4 PLANT_SELECTOR_SEED_PACKET_COST_TEXT_COLOR            = Color4_FromLinear(LinearColor(15, 10, 5));
internal const color4 PLANT_SELECTOR_SEED_PACKET_SELECTED_TINT_COLOR        = Color4(0.5F, 0.5F, 0.5F, 1.0F);
internal const color4 PLANT_SELECTOR_SEED_PACKET_TOO_EXPENSIVE_TINT_COLOR   = Color4(0.5F, 0.5F, 0.5F, 1.0F);
internal const color4 PLANT_SELECTOR_SEED_PACKET_IN_COOLDOWN_TINT_COLOR     = Color4(0.7F, 0.7F, 0.7F, 1.0F);
internal const color4 PLANT_SELECTOR_SEED_PACKET_COOLDOWN_COVER_COLOR       = Color4(0.0F, 0.0F, 0.0F, 0.3F);

internal void
GamePlantSelector_RenderSeedPacket(game_state* GameState, u32 SeedPacketIndex)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;

    const game_seed_packet* SeedPacket = PlantSelector->SeedPackets + SeedPacketIndex;
    const rect2D SeedPacketRectangle = GamePlantSelector_GetSeedPacketRectangle(PlantSelector, SeedPacketIndex);

    //
    // NOTE(Traian): Render the seed packet background.
    //

    color4 TintColor = Color4(1.0F);
    if (PlantSelector->HasSeedPacketSelected && (PlantSelector->SelectedSeedPacketIndex == SeedPacketIndex))
    {
        TintColor = PLANT_SELECTOR_SEED_PACKET_SELECTED_TINT_COLOR;
    }
    else if (GameState->SunCounter.SunAmount < SeedPacket->SunCost)
    {
        TintColor = PLANT_SELECTOR_SEED_PACKET_TOO_EXPENSIVE_TINT_COLOR;
    }
    else if (SeedPacket->IsInCooldown && SeedPacket->CooldownDelay > 0.0F)
    {
        TintColor = PLANT_SELECTOR_SEED_PACKET_IN_COOLDOWN_TINT_COLOR;
    }

    asset* SeedPacketTexture = Asset_Get(&GameState->Assets, GAME_ASSET_ID_UI_SEED_PACKET);
    Renderer_PushPrimitive(&GameState->Renderer,
                           Game_TransformGamePointToNDC(&GameState->Camera, SeedPacketRectangle.Min),
                           Game_TransformGamePointToNDC(&GameState->Camera, SeedPacketRectangle.Max),
                           PLANT_SELECTOR_SEED_PACKET_BACKGROUND_OFFSET_Z, TintColor, Vec2(0.0F), Vec2(1.0F),
                           &SeedPacketTexture->Texture.RendererTexture);

    //
    // NOTE(Traian): Render the seed packet thumbnail.
    //

    vec2 ThumbnailCenter;
    ThumbnailCenter.X = Math_Lerp(SeedPacketRectangle.Min.X, SeedPacketRectangle.Max.X, SeedPacket->ThumbnailCenterPercentage.X);
    ThumbnailCenter.Y = Math_Lerp(SeedPacketRectangle.Min.Y, SeedPacketRectangle.Max.Y, SeedPacket->ThumbnailCenterPercentage.Y);

    vec2 ThumbnailSize;
    ThumbnailSize.X = SeedPacket->ThumbnailSizePercentage.X * PlantSelector->SeedPacketSize.X;
    ThumbnailSize.Y = SeedPacket->ThumbnailSizePercentage.Y * PlantSelector->SeedPacketSize.Y;

    const game_asset_id ThumbnailTextureAssetID = GameState->Config.Plants[SeedPacket->PlantType].AssetID;
    if (ThumbnailTextureAssetID != GAME_ASSET_ID_NONE)
    {
        asset* ThumbnailTextureAsset = Asset_Get(&GameState->Assets, ThumbnailTextureAssetID);
        Renderer_PushPrimitive(&GameState->Renderer,
                               Game_TransformGamePointToNDC(&GameState->Camera, ThumbnailCenter - 0.5F * ThumbnailSize),
                               Game_TransformGamePointToNDC(&GameState->Camera, ThumbnailCenter + 0.5F * ThumbnailSize),
                               PLANT_SELECTOR_SEED_PACKET_THUMBNAIL_OFFSET_Z, TintColor, Vec2(0.0F), Vec2(1.0F),
                               &ThumbnailTextureAsset->Texture.RendererTexture);
    }

    //
    // NOTE(Traian): Render the seed packet sun cost text.
    //

    char SunCostCharacters[8] = {};
    const memory_size SunCostCharacterCount = String_FromUnsignedInteger(SunCostCharacters, sizeof(SunCostCharacters),
                                                                         SeedPacket->SunCost, STRING_NUMBER_BASE_DEC);

    vec2 SunCostCenter;
    SunCostCenter.X = Math_Lerp(SeedPacketRectangle.Min.X, SeedPacketRectangle.Max.X, SeedPacket->SunCostCenterPercentage.X);
    SunCostCenter.Y = Math_Lerp(SeedPacketRectangle.Min.Y, SeedPacketRectangle.Max.Y, SeedPacket->SunCostCenterPercentage.Y);

    asset* FontAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_FONT_COMIC_SANS);
    GameDraw_TextCentered(&GameState->Renderer, &GameState->Camera, FontAsset, SunCostCharacters, SunCostCharacterCount,
                          SunCostCenter, PLANT_SELECTOR_SEED_PACKET_COST_OFFSET_Z,
                          SeedPacket->SunCostHeightPercentage * PlantSelector->SeedPacketSize.Y,
                          PLANT_SELECTOR_SEED_PACKET_COST_TEXT_COLOR);

    //
    // NOTE(Traian): Render the seed packet cooldown cover.
    //

    if (SeedPacket->IsInCooldown && SeedPacket->CooldownDelay > 0.0F)
    {
        const f32 CooldownPercentage = 1.0F - (SeedPacket->CooldownTimer / SeedPacket->CooldownDelay);
        vec2 CooldownCoverSize;
        CooldownCoverSize.X = PlantSelector->SeedPacketSize.X;
        CooldownCoverSize.Y = CooldownPercentage * PlantSelector->SeedPacketSize.Y;

        vec2 CooldownCoverMinPoint;
        CooldownCoverMinPoint.X = SeedPacketRectangle.Min.X;
        CooldownCoverMinPoint.Y = SeedPacketRectangle.Max.Y - CooldownCoverSize.Y;
        vec2 CooldownCoverMaxPoint = CooldownCoverMinPoint + CooldownCoverSize;

        GameDraw_RectangleFilled(GameState, CooldownCoverMinPoint, CooldownCoverMaxPoint,
                                 PLANT_SELECTOR_SEED_PACKET_COOLDOWN_COVER_OFFSET_Z,
                                 PLANT_SELECTOR_SEED_PACKET_COOLDOWN_COVER_COLOR);
    }
}

internal void
GamePlantSelector_RenderPlantPreview(game_state* GameState, vec2 GameMousePosition)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;
    game_garden_grid* GardenGrid = &GameState->GardenGrid;

    // NOTE(Traian): Sanity checks.
    ASSERT(PlantSelector->HasSeedPacketSelected);
    ASSERT(PlantSelector->SelectedSeedPacketIndex < PlantSelector->SeedPacketCount);
    game_seed_packet* SeedPacket = PlantSelector->SeedPackets + PlantSelector->SelectedSeedPacketIndex;

    if (SeedPacket->PlantType != PLANT_TYPE_NONE && SeedPacket->PlantType < PLANT_TYPE_MAX_COUNT)
    {
        const game_plant_config* PlantConfig = &GameState->Config.Plants[SeedPacket->PlantType];
        const vec2 Dimensions = PlantConfig->Dimensions;
        const vec2 RenderOffset = PlantConfig->RenderOffset;
        const vec2 RenderScale = PlantConfig->RenderScale;
        const vec2 RenderDimensions = Vec2(Dimensions.X * RenderScale.X, Dimensions.Y * RenderScale.Y);

        const vec2 PreviewMinPoint = PlantSelector->PlantPreviewCenterPosition - 0.5F * RenderDimensions + RenderOffset;
        const vec2 PreviewMaxPoint = PreviewMinPoint + RenderDimensions;
        
        asset* PreviewTextureAsset = Asset_Get(&GameState->Assets, PlantConfig->AssetID);
        if (PreviewTextureAsset)
        {
            Renderer_PushPrimitive(&GameState->Renderer,
                                   Game_TransformGamePointToNDC(&GameState->Camera, PreviewMinPoint),
                                   Game_TransformGamePointToNDC(&GameState->Camera, PreviewMaxPoint),
                                   PLANT_SELECTOR_PLANT_PREVIEW_OFFSET_Z, Color4(1.0F, 1.0F, 1.0F, 0.7F),
                                   Vec2(0.0F), Vec2(1.0F),
                                   &PreviewTextureAsset->Texture.RendererTexture);
        }
    }
}

internal void
GamePlantSelector_Render(game_state* GameState, game_platform_state* PlatformState)
{
    game_plant_selector* PlantSelector = &GameState->PlantSelector;

    //
    // NOTE(Traian): Render the plant selector frame.
    //

    GameDraw_Rectangle(GameState, PlantSelector->MinPoint, PlantSelector->MaxPoint, PlantSelector->BorderThickness,
                       PLANT_SELECTOR_FRAME_OFFSET_Z, PLANT_SELECTOR_FRAME_BORDER_COLOR);

    GameDraw_RectangleFilled(GameState,
                             PlantSelector->MinPoint + Vec2(PlantSelector->BorderThickness),
                             PlantSelector->MaxPoint - Vec2(PlantSelector->BorderThickness),
                             PLANT_SELECTOR_FRAME_OFFSET_Z, PLANT_SELECTOR_FRAME_BACKGROUND_COLOR);

    //
    // NOTE(Traian): Render the plant selector seed packets.
    //

    for (u32 SeedPacketIndex = 0; SeedPacketIndex < PlantSelector->SeedPacketCount; ++SeedPacketIndex)
    {
        GamePlantSelector_RenderSeedPacket(GameState, SeedPacketIndex);
    }

    //
    // NOTE(Traian): Render the plant preview.
    //

    if (PlantSelector->HasSeedPacketSelected)
    {
        const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                    Vec2(PlatformState->Input->MousePositionX,
                                                                         PlatformState->Input->MousePositionY));
        GamePlantSelector_RenderPlantPreview(GameState, GameMousePosition);
    }
}
