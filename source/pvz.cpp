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

//
// NOTE(Traian): ADDING A NEW PLANT TYPE!
//
// - 'Game_SetDefaultConfiguration', where you *must* define the plant configuration structure.
// - 'GameGardenGrid_UpdatePlants', which (optionally) updates the logic and the plant.
// - 'GamePLantSelector_PlantSeedPacket', which (optionally) configures all paramteres specific to that type.
//

enum plant_type : u16
{
#define PVZ_ENUMERATE_PLANT_TYPES(X)        \
    X(PLANT_TYPE_SUNFLOWER, Sunflower)      \
    X(PLANT_TYPE_PEASHOOTER, Peashooter)    \
    X(PLANT_TYPE_REPEATER, Repeater)        \
    X(PLANT_TYPE_TORCHWOOD, Torchwood)      \
    X(PLANT_TYPE_MELONPULT, Melonpult)      \
    X(PLANT_TYPE_WALLNUT, Wallnut)

    PLANT_TYPE_NONE = 0,
#define _PVZ_ENUM_MEMBER(X, N) X,
    PVZ_ENUMERATE_PLANT_TYPES(_PVZ_ENUM_MEMBER)
#undef _PVZ_ENUM_MEMBER
    PLANT_TYPE_MAX_COUNT,
};

//
// NOTE(Traian): ADDING A NEW ZOMBIE TYPE!
//
// - 'Game_SetDefaultConfiguration', where you *must* define the zombie configuration structure.
// - 'GameGardenGrid_UpdateZombies', which (optionally) updates the zombie logic.
// - 'GameGardenGrid_SpawnZombie', which (optionally) configures all parameters specific to that type.
//

enum zombie_type : u16
{
    ZOMBIE_TYPE_NONE = 0,
    ZOMBIE_TYPE_NORMAL,
    ZOMBIE_TYPE_MAX_COUNT,
};

//
// NOTE(Traian): ADDING A NEW PROJECTILE TYPE!
//
// - 'Game_SetDefaultConfiguration', where you *must* define the projectile configuration structure.
// - 'GameGardenGrid_UpdateProjectiles', which (optionally) updates the projectile logic.
//

enum projectile_type : u16
{
#define PVZ_ENUMERATE_PROJECTILE_TYPES(X)   \
    X(PROJECTILE_TYPE_SUN, Sun)             \
    X(PROJECTILE_TYPE_PEA, Pea)             \
    X(PROJECTILE_TYPE_FIRE_PEA, FirePea)    \
    X(PROJECTILE_TYPE_MELON, Melon)

    PROJECTILE_TYPE_NONE = 0,
#define _PVZ_ENUM_MEMBER(X, N) X,
    PVZ_ENUMERATE_PROJECTILE_TYPES(_PVZ_ENUM_MEMBER)
#undef _PVZ_ENUM_MEMBER
    PROJECTILE_TYPE_MAX_COUNT,
};

struct game_camera
{
    f32                                 UnitCountX;
    f32                                 UnitCountY;
    vec2                                NDCViewportMin;
    vec2                                NDCViewportMax;
    u32                                 ViewportPixelCountX;
    u32                                 ViewportPixelCountY;
};

struct plant_entity_sunflower
{
    f32                                 GenerateDelayBase;
    f32                                 GenerateDelayRandomOffset;
    f32                                 GenerateTimer;
    u32                                 SunAmount;
    f32                                 SunRadius;
    f32                                 SunDecayDelay;
};

struct plant_entity_peashooter
{
    f32                                 ShootDelay;
    f32                                 ShootTimer;
    f32                                 ProjectileDamage;
    f32                                 ProjectileVelocity;
    f32                                 ProjectileRadius;
};

struct plant_entity_repeater
{
    f32                                 ShootSequenceDelay;
    f32                                 ShootSequenceDeltaDelay;
    f32                                 ShootTimer;
    b8                                  IsInShootSequence;
    f32                                 ProjectileDamage;
    f32                                 ProjectileVelocity;
    f32                                 ProjectileRadius;
};

struct plant_entity_torchwood
{
    f32                                 DamageMultiplier;
};

struct plant_entity_melonpult
{
    f32                                 LaunchDelay;
    f32                                 LaunchTimer;
    f32                                 ProjectileDamage;
    f32                                 ProjectileRadius;
    f32                                 ProjectileVelocity;
    f32                                 ProjectileSplashDamageRadius;
    f32                                 ProjectileSplashDamageMultiplier;
};

struct plant_entity_wallnut
{
    f32                                 MaxHealth;
    f32                                 CrackStage1HealthPercentage;
    f32                                 CrackStage2HealthPercentage;
    u8                                  CrackIndex;
};

struct plant_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    plant_type                          Type;
    b8                                  IsPendingDestroy;
    u8                                  Reserved_;
    f32                                 Health;
    union
    {
        plant_entity_sunflower          Sunflower;
        plant_entity_peashooter         Peashooter;
        plant_entity_repeater           Repeater;
        plant_entity_torchwood          Torchwood;
        plant_entity_melonpult          Melonpult;
        plant_entity_wallnut            Wallnut;
    };
};

struct zombie_entity_normal
{
    f32                                 Velocity;
    f32                                 AttackDamage;
    f32                                 AttackDelay;
    f32                                 AttackTimer;
};

struct zombie_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    zombie_type                         Type;
    b8                                  IsPendingDestroy;
    u8                                  Reserved_;
    u32                                 CellIndexY;
    vec2                                Position;
    f32                                 Health;
    union
    {
        zombie_entity_normal            Normal;
    };
};

struct projectile_entity_sun
{
    u32                                 SunAmount;
    f32                                 DecayDelay;
    f32                                 DecayTimer;
};

struct projectile_entity_pea
{
    f32                                 Velocity;
    f32                                 Damage;
};

struct projectile_entity_fire_pea
{
    f32                                 Velocity;
    f32                                 Damage;
};

struct projectile_entity_melon
{
    f32                                 Damage;
    f32                                 SplashDamageRadius;
    f32                                 SplashDamageMultiplier;
    vec2                                StartPosition;
    vec2                                TargetPosition;
    f32                                 Velocity;
    zombie_entity*                      TargetZombie;
};

struct projectile_entity
{
    // NOTE(Traian): All entity types must have the same 32-bit header, which contained the entity type (16-bit),
    // the 'is-pending-destroy' flag (8-bit) and a currently unused reserved flag (8-bit).
    projectile_type                     Type;
    b8                                  IsPendingDestroy;
    u8                                  Reserved_;
    u32                                 CellIndexY;
    vec2                                Position;
    f32                                 Radius;
    union
    {
        projectile_entity_sun           Sun;
        projectile_entity_pea           Pea;
        projectile_entity_fire_pea      FirePea;
        projectile_entity_melon         Melon;
    };
};

struct game_garden_grid
{
    vec2                                MinPoint;
    vec2                                MaxPoint;
    
    u32                                 CellCountX;
    u32                                 CellCountY;
    plant_entity*                       PlantEntities;
    
    u32                                 MaxZombieCount;
    u32                                 CurrentZombieCount;
    zombie_entity*                      ZombieEntities;

    u32                                 MaxProjectileCount;
    u32                                 CurrentProjectileCount;
    projectile_entity*                  ProjectileEntities;

    f32                                 SpawnZombieMinDelay;
    f32                                 SpawnZombieMaxDelay;
    f32                                 SpawnNextZombieDelay;
    f32                                 SpawnNextZombieTimer;

    f32                                 SpawnNaturalSunMinDelay;
    f32                                 SpawnNaturalSunMaxDelay;
    f32                                 SpawnNextNaturalSunDelay;
    f32                                 SpawnNextNaturalSunTimer;

    random_series                       RandomSeries;
};

struct game_sun_counter
{
    vec2                                MinPoint;
    vec2                                MaxPoint;

    f32                                 BorderThickness;
    vec2                                SunAmountCenterPercentage;
    f32                                 SunAmountHeightPercentage;
    vec2                                SunThumbnailCenterPercentage;
    vec2                                SunThumbnailSizePercentage;
    vec2                                SunCostShelfCenterPercentage;
    vec2                                SunCostShelfSizePercentage;

    u32                                 SunAmount;
};

struct game_seed_packet
{
    vec2                                SunCostCenterPercentage;
    f32                                 SunCostHeightPercentage;
    vec2                                ThumbnailCenterPercentage;
    vec2                                ThumbnailSizePercentage;

    plant_type                          PlantType;
    u32                                 SunCost;
    f32                                 CooldownDelay;
    b8                                  IsInCooldown;
    f32                                 CooldownTimer;
};

struct game_plant_selector
{
    vec2                                MinPoint;
    vec2                                MaxPoint;

    f32                                 BorderThickness;
    f32                                 SeedPacketBorderPadding;
    f32                                 SeedPacketSpace;
    f32                                 SeedPacketAspectRatio;

    u32                                 SeedPacketCount;
    game_seed_packet*                   SeedPackets;
    vec2                                SeedPacketSize;

    b8                                  HasSeedPacketSelected;
    u32                                 SelectedSeedPacketIndex;
    vec2                                PlantPreviewCenterPosition;
};

struct game_shovel
{
    vec2                                MinPoint;
    vec2                                MaxPoint;
    vec2                                ThumbnailCenterPosition;
    vec2                                ThumbnailDimensions;

    f32                                 BorderThickness;
    vec2                                ThumbnailCenterPercentage;
    vec2                                ThumbnailDimensionsPercentage;

    b8                                  IsSelected;
    vec2                                ToolCenterPosition;
};

struct game_plant_config
{
    u32                                 SunCost;
    f32                                 PlantCooldownDelay;
    f32                                 Health;
    vec2                                Dimensions;
    vec2                                RenderScale;
    vec2                                RenderOffset;
    game_asset_id                       AssetID;
    b8                                  UseCustomRenderProcedure;
};

struct game_zombie_config
{
    f32                                 Health;
    vec2                                Dimensions;
    vec2                                RenderScale;
    vec2                                RenderOffset;
    game_asset_id                       AssetID;
};

struct game_projectile_config
{
    vec2                                RenderScale;
    vec2                                RenderOffset;
    game_asset_id                       AssetID;
};

struct game_config
{
    game_plant_config                   Plants      [PLANT_TYPE_MAX_COUNT];
    game_zombie_config                  Zombies     [ZOMBIE_TYPE_MAX_COUNT];
    game_projectile_config              Projectiles [PROJECTILE_TYPE_MAX_COUNT];
};

struct game_state
{
    memory_arena*                       PermanentArena;
    memory_arena*                       TransientArena;
    game_assets                         Assets;
    renderer                            Renderer;
    game_camera                         Camera;
    game_garden_grid                    GardenGrid;
    game_sun_counter                    SunCounter;
    game_plant_selector                 PlantSelector;
    game_shovel                         Shovel;
    game_config                         Config;
};

//====================================================================================================================//
//------------------------------------------------ STRING MANIPULATION -----------------------------------------------//
//====================================================================================================================//

enum string_number_base : u8
{
    STRING_NUMBER_BASE_BIN = 2,
    STRING_NUMBER_BASE_OCT = 8,
    STRING_NUMBER_BASE_DEC = 10,
    STRING_NUMBER_BASE_HEX = 16,
};

internal inline memory_size
String_FromUnsignedInteger(char* DstBuffer, memory_size DstBufferByteCount, u64 Value, string_number_base NumberBase)
{
    if (Value == 0)
    {
        if (DstBufferByteCount > 0)
        {
            DstBuffer[0] = '0';
            return sizeof(char);
        }
        else
        {
            // NOTE(Traian): The buffer is not sufficient to store the number.
            return 0;
        }
    }

    u8 DigitCount = 0;
    u64 WorkValue = Value;
    while (WorkValue > 0)
    {
        ++DigitCount;
        WorkValue /= (u64)NumberBase;
    }

    if (DigitCount * sizeof(char) <= DstBufferByteCount)
    {
        
        const memory_size WrittenByteCount = DigitCount * sizeof(char);
        memory_size ByteOffset = WrittenByteCount - 1;
        
        WorkValue = Value;
        while (WorkValue > 0)
        {
            const char* DIGITS = "0123456789ABCDEF";
            const u8 Digit = WorkValue % NumberBase;
            DstBuffer[ByteOffset--] = DIGITS[Digit];
            WorkValue /= (u64)NumberBase;
        }

        return WrittenByteCount;
    }
    else
    {
        // NOTE(Traian): The buffer is not sufficient to store the number.
        return 0;
    }
}

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
#include "pvz_game_shovel.inl"

//====================================================================================================================//
//-------------------------------------------------- INITIALIZATION --------------------------------------------------//
//====================================================================================================================//

internal void
Game_SetDefaultConfiguration(game_state* GameState)
{
    game_config* Config = &GameState->Config;

#define CONFIGURE_DEFAULT_PLANT(PLANT_NAME, AssetIDValue)                                                                             \
    {                                                                                                                   \
        const plant_type PlantType = PLANT_TYPE_##PLANT_NAME;                                                           \
        game_plant_config* PlantConfig = &Config->Plants[PlantType];                                                    \
        PlantConfig->SunCost = PLANT_##PLANT_NAME##_SUN_COST;                                                           \
        PlantConfig->PlantCooldownDelay = PLANT_##PLANT_NAME##_PLANT_COOLDOWN_DELAY;                                    \
        PlantConfig->Health = PLANT_##PLANT_NAME##_HEALTH;                                                              \
        PlantConfig->Dimensions = Vec2(PLANT_##PLANT_NAME##_DIMENSIONS_X, PLANT_##PLANT_NAME##_DIMENSIONS_Y);           \
        PlantConfig->RenderScale = Vec2(PLANT_##PLANT_NAME##_RENDER_SCALE_X, PLANT_##PLANT_NAME##_RENDER_SCALE_Y);      \
        PlantConfig->RenderOffset = Vec2(PLANT_##PLANT_NAME##_RENDER_OFFSET_X, PLANT_##PLANT_NAME##_RENDER_OFFSET_Y);   \
        PlantConfig->AssetID = AssetIDValue;                                                                            \
    }

#define CONFIGURE_DEFAULT_ZOMBIE(ZOMBIE_NAME, AssetIDValue)                                                                 \
    {                                                                                                                       \
        const zombie_type ZombieType = ZOMBIE_TYPE_##ZOMBIE_NAME;                                                           \
        game_zombie_config* ZombieConfig = &Config->Zombies[ZombieType];                                                    \
        ZombieConfig->Health = ZOMBIE_##ZOMBIE_NAME##_HEALTH;                                                               \
        ZombieConfig->Dimensions = Vec2(ZOMBIE_##ZOMBIE_NAME##_DIMENSIONS_X, ZOMBIE_##ZOMBIE_NAME##_DIMENSIONS_Y);          \
        ZombieConfig->RenderScale = Vec2(ZOMBIE_##ZOMBIE_NAME##_RENDER_SCALE_X, ZOMBIE_##ZOMBIE_NAME##_RENDER_SCALE_Y);     \
        ZombieConfig->RenderOffset = Vec2(ZOMBIE_##ZOMBIE_NAME##_RENDER_OFFSET_X, ZOMBIE_##ZOMBIE_NAME##_RENDER_OFFSET_Y);  \
        ZombieConfig->AssetID = AssetIDValue;                                                                               \
    }

#define CONFIGURE_DEFAULT_PROJECTILE(PROJECTILE_NAME, AssetIDValue)                             \
    {                                                                                           \
        const projectile_type ProjectileType = PROJECTILE_TYPE_##PROJECTILE_NAME;               \
        game_projectile_config* ProjectileConfig = &Config->Projectiles[ProjectileType];        \
        ProjectileConfig->RenderScale = Vec2(PROJECTILE_##PROJECTILE_NAME##_RENDER_SCALE_X,     \
                                             PROJECTILE_##PROJECTILE_NAME##_RENDER_SCALE_Y);    \
        ProjectileConfig->RenderOffset = Vec2(PROJECTILE_##PROJECTILE_NAME##_RENDER_OFFSET_X,   \
                                             PROJECTILE_##PROJECTILE_NAME##_RENDER_OFFSET_Y);   \
        ProjectileConfig->AssetID = AssetIDValue;                                               \
    }

    CONFIGURE_DEFAULT_PLANT(SUNFLOWER, GAME_ASSET_ID_PLANT_SUNFLOWER);
    CONFIGURE_DEFAULT_PLANT(PEASHOOTER, GAME_ASSET_ID_PLANT_PEASHOOTER);
    CONFIGURE_DEFAULT_PLANT(REPEATER, GAME_ASSET_ID_PLANT_REPEATER);
    CONFIGURE_DEFAULT_PLANT(TORCHWOOD, GAME_ASSET_ID_PLANT_TORCHWOOD);

    CONFIGURE_DEFAULT_PROJECTILE(SUN, GAME_ASSET_ID_PROJECTILE_SUN);
    CONFIGURE_DEFAULT_PROJECTILE(PEA, GAME_ASSET_ID_PROJECTILE_PEA);
    CONFIGURE_DEFAULT_PROJECTILE(FIRE_PEA, GAME_ASSET_ID_PROJECTILE_FIRE_PEA);

    CONFIGURE_DEFAULT_ZOMBIE(NORMAL, GAME_ASSET_ID_ZOMBIE_NORMAL);

#undef CONFIGURE_DEFAULT_PLANT
#undef CONFIGURE_DEFAULT_ZOMBIE
#undef CONFIGURE_DEFAULT_PROJECTILE
}

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

    //
    // NOTE(Traian): Initialize the renderer.
    //

    Renderer_Initialize(&GameState->Renderer, GameState->PermanentArena);

    //
    // NOTE(Traian): Initialize the game layers.
    //

    Game_SetDefaultConfiguration(GameState);

    GameGardenGrid_Initialize(GameState);
    GameSunCounter_Initialize(GameState);
    GamePlantSelector_Initialize(GameState);
    GameShovel_Initialize(GameState);

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
    GameShovel_Update(GameState, PlatformState, DeltaTime);

    GameGardenGrid_Render(GameState, PlatformState);
    GameSunCounter_Render(GameState, PlatformState);
    GamePlantSelector_Render(GameState, PlatformState);
    GameShovel_Render(GameState, PlatformState);

    Renderer_EndFrame(&GameState->Renderer);
    Renderer_DispatchClusters(&GameState->Renderer, PlatformState->RenderTarget);
}
