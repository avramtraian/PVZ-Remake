// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

#include "pvz_asset.h"
#include "pvz_game_config.h"
#include "pvz_memory.h"
#include "pvz_platform.h"
#include "pvz_renderer.h"

//====================================================================================================================//
//-------------------------------------------------- STATE STRUCTURE -------------------------------------------------//
//====================================================================================================================//

struct game_camera
{
    f32     UnitCountX;
    f32     UnitCountY;
    vec2    NDCViewportMin;
    vec2    NDCViewportMax;
    u32     ViewportPixelCountX;
    u32     ViewportPixelCountY;
};

enum plant_type : u16
{
    PLANT_TYPE_NONE = 0,
    PLANT_TYPE_SUNFLOWER,
    PLANT_TYPE_PEASHOOTER,
};

struct plant_entity_sunflower
{
    f32 GenerateDelayBase;
    f32 GenerateDelayRandomOffset;
    f32 GenerateTimer;
    u32 SunAmount;
    f32 SunRadius;
    f32 SunDecayDelay;
};

struct plant_entity_peashooter
{
    f32 ShootDelay;
    f32 ShootTimer;
    f32 ProjectileDamage;
    f32 ProjectileVelocity;
    f32 ProjectileRadius;
};

struct plant_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    plant_type                  Type;
    b8                          IsPendingDestroy;
    u8                          Reserved_;
    f32                         Health;
    union
    {
        plant_entity_sunflower  Sunflower;
        plant_entity_peashooter Peashooter;
    };
};

enum zombie_type : u16
{
    ZOMBIE_TYPE_NONE = 0,
    ZOMBIE_TYPE_NORMAL,
};

struct zombie_entity_normal
{
    f32 Velocity;
    f32 AttackDamage;
    f32 AttackDelay;
    f32 AttackTimer;
};

struct zombie_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    zombie_type                 Type;
    b8                          IsPendingDestroy;
    u8                          Reserved_;
    u32                         CellIndexY;
    vec2                        Position;
    vec2                        Dimensions;
    f32                         Health;
    union
    {
        zombie_entity_normal    Normal;
    };
};

enum projectile_type : u16
{
    PROJECTILE_TYPE_NONE = 0,
    PROJECTILE_TYPE_PEA,
    PROJECTILE_TYPE_SUN,
};

struct projectile_entity_sun
{
    f32     Radius;
    u32     SunAmount;
    f32     DecayDelay;
    f32     DecayTimer;
};

struct projectile_entity_pea
{
    f32 Radius;
    f32 Velocity;
    f32 Damage;
};

struct projectile_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    projectile_type             Type;
    b8                          IsPendingDestroy;
    u8                          Reserved_;
    u32                         CellIndexY;
    vec2                        Position;
    vec2                        Dimensions;
    union
    {
        projectile_entity_sun   Sun;
        projectile_entity_pea   Pea;
    };
};

struct game_garden_grid
{
    vec2                    MinPoint;
    vec2                    MaxPoint;
    
    u32                     CellCountX;
    u32                     CellCountY;
    plant_entity*           PlantEntities;
    
    u32                     MaxZombieCount;
    u32                     CurrentZombieCount;
    zombie_entity*          ZombieEntities;

    u32                     MaxProjectileCount;
    u32                     CurrentProjectileCount;
    projectile_entity*      ProjectileEntities;

    f32                     SpawnZombieMinDelay;
    f32                     SpawnZombieMaxDelay;
    f32                     SpawnNextZombieDelay;
    f32                     SpawnNextZombieTimer;

    random_series           RandomSeries;
};

struct game_sun_counter
{
    vec2    MinPoint;
    vec2    MaxPoint;

    f32     BorderThickness;
    vec2    SunAmountCenterPercentage;
    f32     SunAmountHeightPercentage;
    vec2    SunThumbnailCenterPercentage;
    vec2    SunThumbnailSizePercentage;
    vec2    SunCostShelfCenterPercentage;
    vec2    SunCostShelfSizePercentage;

    u32     SunAmount;
};

struct game_seed_packet
{
    vec2        SunCostCenterPercentage;
    f32         SunCostHeightPercentage;
    vec2        ThumbnailCenterPercentage;
    vec2        ThumbnailSizePercentage;

    plant_type  PlantType;
    u32         SunCost;
    f32         CooldownDelay;
    b8          IsInCooldown;
    f32         CooldownTimer;
};

struct game_plant_selector
{
    vec2                MinPoint;
    vec2                MaxPoint;

    f32                 BorderThickness;
    f32                 SeedPacketBorderPadding;
    f32                 SeedPacketSpace;
    f32                 SeedPacketAspectRatio;

    u32                 SeedPacketCount;
    game_seed_packet*   SeedPackets;
    vec2                SeedPacketSize;

    b8                  HasSeedPacketSelected;
    u32                 SelectedSeedPacketIndex;
    vec2                PlantPreviewCenterPosition;
};

struct game_state
{
    memory_arena*       PermanentArena;
    memory_arena*       TransientArena;
    game_assets         Assets;
    renderer            Renderer;
    game_camera         Camera;
    game_garden_grid    GardenGrid;
    game_sun_counter    SunCounter;
    game_plant_selector PlantSelector;
};

//====================================================================================================================//
//------------------------------------------------------ CAMERA ------------------------------------------------------//
//====================================================================================================================//

internal inline f32
Game_TransformGameCoordToNDCX(const game_camera* Camera, f32 GameCoordX)
{
    const f32 NDCCoord = Math_Lerp(Camera->NDCViewportMin.X, Camera->NDCViewportMax.X, GameCoordX / Camera->UnitCountX);
    return NDCCoord;
}

internal inline f32
Game_TransformGameCoordToNDCY(const game_camera* Camera, f32 GameCoordY)
{
    const f32 NDCCoord = Math_Lerp(Camera->NDCViewportMin.Y, Camera->NDCViewportMax.Y, GameCoordY / Camera->UnitCountY);
    return NDCCoord;
}

internal inline vec2
Game_TransformGamePointToNDC(const game_camera* Camera, vec2 GamePoint)
{
    vec2 NDCPoint;
    NDCPoint.X = Game_TransformGameCoordToNDCX(Camera, GamePoint.X);
    NDCPoint.Y = Game_TransformGameCoordToNDCY(Camera, GamePoint.Y);
    return NDCPoint;
}

internal inline f32
Game_TransformNDCCoordToGameX(const game_camera* Camera, f32 NDCCoordX)
{
    const f32 GameCoord = Math_InverseLerp(Camera->NDCViewportMin.X, Camera->NDCViewportMax.X, NDCCoordX) *
                          Camera->UnitCountX;
    return GameCoord;
}

internal inline f32
Game_TransformNDCCoordToGameY(const game_camera* Camera, f32 NDCCoordY)
{
    const f32 GameCoord = Math_InverseLerp(Camera->NDCViewportMin.Y, Camera->NDCViewportMax.Y, NDCCoordY) *
                          Camera->UnitCountY;
    return GameCoord;
}

internal inline vec2
Game_TransformNDCPointToGame(const game_camera* Camera, vec2 NDCPoint)
{
    vec2 GamePoint;
    GamePoint.X = Game_TransformNDCCoordToGameX(Camera, NDCPoint.X);
    GamePoint.Y = Game_TransformNDCCoordToGameY(Camera, NDCPoint.Y);
    return GamePoint;
}

internal f32
Game_PixelCountToGameDistanceX(const game_camera* Camera, u32 PixelCountX)
{
    const vec2 NDCMinPoint = Vec2(0.0F, 0.0F);
    const vec2 NDCMaxPoint = Vec2((f32)PixelCountX / (f32)Camera->ViewportPixelCountX, 0.0F);
    const vec2 GameMinPoint = Game_TransformNDCPointToGame(Camera, NDCMinPoint);
    const vec2 GameMaxPoint = Game_TransformNDCPointToGame(Camera, NDCMaxPoint);
    return GameMaxPoint.X - GameMinPoint.X;
}

internal f32
Game_PixelCountToGameDistanceY(const game_camera* Camera, u32 PixelCountY)
{
    const vec2 NDCMinPoint = Vec2(0.0F, 0.0F);
    const vec2 NDCMaxPoint = Vec2(0.0F, (f32)PixelCountY / (f32)Camera->ViewportPixelCountY);
    const vec2 GameMinPoint = Game_TransformNDCPointToGame(Camera, NDCMinPoint);
    const vec2 GameMaxPoint = Game_TransformNDCPointToGame(Camera, NDCMaxPoint);
    return GameMaxPoint.Y - GameMinPoint.Y;
}

internal void
Game_UpdateCamera(game_state* GameState, renderer_image* RenderTarget)
{
    game_camera* Camera = &GameState->Camera;
    Camera->UnitCountX = 8.0F;
    Camera->UnitCountY = 6.0F;
    Camera->ViewportPixelCountX = RenderTarget->SizeX;
    Camera->ViewportPixelCountY = RenderTarget->SizeY;

    const f32 CameraAspectRatio = Camera->UnitCountX / Camera->UnitCountY;
    const u32 RequiredRenderTargetSizeX = RenderTarget->SizeY * CameraAspectRatio;
    const u32 RequiredRenderTargetSizeY = RenderTarget->SizeX / CameraAspectRatio;

    if (RequiredRenderTargetSizeX <= RenderTarget->SizeX)
    {
        const f32 Padding = (f32)(RenderTarget->SizeX - RequiredRenderTargetSizeX) / (f32)RenderTarget->SizeX;
        Camera->NDCViewportMin.X = 0.5F * Padding;
        Camera->NDCViewportMax.X = 1.0F - (0.5F * Padding);
        Camera->NDCViewportMin.Y = 0.0F;
        Camera->NDCViewportMax.Y = 1.0F;
    }
    else if (RequiredRenderTargetSizeY <= RenderTarget->SizeY)
    {
        const f32 Padding = (f32)(RenderTarget->SizeY - RequiredRenderTargetSizeY) / (f32)RenderTarget->SizeY;
        Camera->NDCViewportMin.X = 0.0F;
        Camera->NDCViewportMax.X = 1.0F;
        Camera->NDCViewportMin.Y = 0.5F * Padding;
        Camera->NDCViewportMax.Y = 1.0F - (0.5F * Padding);
    }
    else
    {
        PANIC("Failed to calculate the required padding for the camera viewport!");
    }
}

//====================================================================================================================//
//-------------------------------------------- GAME LAYERS IMPLEMENTATIONS -------------------------------------------//
//====================================================================================================================//

#include "pvz_game_draw.inl"

#include "pvz_game_garden_grid.inl"
#include "pvz_game_sun_counter.inl"
#include "pvz_game_plant_selector.inl"

//====================================================================================================================//
//-------------------------------------------------- INITIALIZATION --------------------------------------------------//
//====================================================================================================================//

function struct game_state*
Game_Initialize(platform_game_memory* GameMemory)
{
    game_state* GameState = PUSH(GameMemory->PermanentArena, game_state);
    GameState->PermanentArena = GameMemory->PermanentArena;
    GameState->TransientArena = GameMemory->TransientArena;

    //
    // NOTE(Traian): Initialize the game assets.
    //

    platform_file_handle AssetFileHandle = Platform_OpenFile("PVZ-Remake-Assets.data",
                                                             PLATFORM_FILE_ACCESS_READ,
                                                             false, false);
    if (!Platform_IsFileHandleValid(AssetFileHandle))
    {
        PANIC("Failed to open the asset file!");
    }
    Asset_Initialize(&GameState->Assets, GameState->TransientArena, AssetFileHandle);
    Asset_LoadSync(&GameState->Assets, GAME_ASSET_ID_FONT_COMIC_SANS);

    //
    // NOTE(Traian): Initialize the renderer.
    //

    Renderer_Initialize(&GameState->Renderer, GameState->PermanentArena);

    //
    // NOTE(Traian): Initialize the game layers.
    //

    GameGardenGrid_Initialize(GameState);
    GameSunCounter_Initialize(GameState);
    GamePlantSelector_Initialize(GameState);

    return GameState;
}

//====================================================================================================================//
//----------------------------------------------------- UPDATING -----------------------------------------------------//
//====================================================================================================================//

function void
Game_UpdateAndRender(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    Game_UpdateCamera(GameState, PlatformState->RenderTarget);
    Renderer_BeginFrame(&GameState->Renderer, PlatformState->RenderTarget->SizeX, PlatformState->RenderTarget->SizeY);
    Renderer_PushPrimitive(&GameState->Renderer, Vec2(0, 0), Vec2(1, 1), -1.0F, Color4(0.1F, 0.1F, 0.1F));

    GameGardenGrid_Update(GameState, PlatformState, DeltaTime);
    GameSunCounter_Update(GameState, PlatformState, DeltaTime);
    GamePlantSelector_Update(GameState, PlatformState, DeltaTime);

    GameGardenGrid_Render(GameState, PlatformState);
    GameSunCounter_Render(GameState, PlatformState);
    GamePlantSelector_Render(GameState, PlatformState);

    Renderer_EndFrame(&GameState->Renderer);
    Renderer_DispatchClusters(&GameState->Renderer, PlatformState->RenderTarget);
}
