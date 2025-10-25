// Copyright (c) 2025 Traian Avram. All rights reserved.
// This source file is part of the PvZ-Remake project and is distributed under the MIT license.

//====================================================================================================================//
//---------------------------------------------------- INITIALIZE ----------------------------------------------------//
//====================================================================================================================//

internal void
GameGardenGrid_Initialize(game_state* GameState)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;

    GardenGrid->CellCountX = 9;
    GardenGrid->CellCountY = 5;

    // NOTE(Traian): Allocate the plant entities buffer.
    GardenGrid->PlantEntities = PUSH_ARRAY(GameState->PermanentArena, plant_entity,
                                           GardenGrid->CellCountX * GardenGrid->CellCountY);

    // NOTE(Traian): Allocate the zombie entities buffer.
    GardenGrid->MaxZombieCount = 128;
    GardenGrid->CurrentZombieCount = 0;
    GardenGrid->ZombieEntities = PUSH_ARRAY(GameState->PermanentArena, zombie_entity,
                                            GardenGrid->MaxZombieCount);

    // NOTE(Traian): Allocate the projectile entities buffer.
    GardenGrid->MaxProjectileCount = 256;
    GardenGrid->CurrentProjectileCount = 0;
    GardenGrid->ProjectileEntities = PUSH_ARRAY(GameState->PermanentArena, projectile_entity,
                                                GardenGrid->MaxProjectileCount);

    GardenGrid->SpawnNaturalSunMinDelay = NATURAL_SUN_SPAWN_MIN_DELAY;
    GardenGrid->SpawnNaturalSunMaxDelay = NATURAL_SUN_SPAWN_MAX_DELAY;
    GardenGrid->SpawnNextNaturalSunDelay = 0.5F * (GardenGrid->SpawnNaturalSunMinDelay + GardenGrid->SpawnNaturalSunMaxDelay);

    // NOTE(Traian): Initialize the random series.
    Platform_SeedRandomSeries(&GardenGrid->RandomSeries);
}

//====================================================================================================================//
//------------------------------------------------------ UPDATE ------------------------------------------------------//
//====================================================================================================================//

internal inline f32
GameGardenGrid_GetCellPositionX(const game_garden_grid* GardenGrid, u32 CellIndexX)
{
    ASSERT(CellIndexX < GardenGrid->CellCountX);
    f32 Result = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X,
                           ((f32)CellIndexX + 0.5F) / (f32)GardenGrid->CellCountX);
    return Result;
}

internal inline f32
GameGardenGrid_GetCellPositionY(const game_garden_grid* GardenGrid, u32 CellIndexY)
{
    ASSERT(CellIndexY < GardenGrid->CellCountY);
    f32 Result = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y,
                           ((f32)CellIndexY + 0.5F) / (f32)GardenGrid->CellCountY);
    return Result;
}

internal inline vec2
GameGardenGrid_GetCellPosition(const game_garden_grid* GardenGrid, u32 CellIndexX, u32 CellIndexY)
{
    vec2 Result;
    Result.X = GameGardenGrid_GetCellPositionX(GardenGrid, CellIndexX);
    Result.Y = GameGardenGrid_GetCellPositionY(GardenGrid, CellIndexY);
    return Result;
}

internal inline s32
GameGardenGrid_GetCellIndexX(const game_garden_grid* GardenGrid, f32 PositionX)
{
    const f32 GridSizeX = GardenGrid->MaxPoint.X - GardenGrid->MinPoint.X;
    const f32 Percentage = (PositionX - GardenGrid->MinPoint.X) / GridSizeX;
    s32 Result = (s32)(Percentage * (f32)GardenGrid->CellCountX);
    if (PositionX < GardenGrid->MinPoint.X)
    {
        --Result;
    }
    return Result;
}

internal inline s32
GameGardenGrid_GetCellIndexY(const game_garden_grid* GardenGrid, f32 PositionY)
{
    const f32 GridSizeY = GardenGrid->MaxPoint.Y - GardenGrid->MinPoint.Y;
    const f32 Percentage = (PositionY - GardenGrid->MinPoint.Y) / GridSizeY;
    s32 Result = (s32)(Percentage * (f32)GardenGrid->CellCountY);
    if (PositionY < GardenGrid->MinPoint.Y)
    {
        --Result;
    }
    return Result;
}

internal projectile_entity*
GameGardenGrid_PushProjectileEntity(game_garden_grid* GardenGrid, projectile_type Type, u32 CellIndexY)
{
    ASSERT(Type != PROJECTILE_TYPE_NONE);

    if (GardenGrid->CurrentProjectileCount < GardenGrid->MaxProjectileCount)
    {
        projectile_entity* ProjectileEntity = GardenGrid->ProjectileEntities + GardenGrid->CurrentProjectileCount;
        GardenGrid->CurrentProjectileCount++;
        ProjectileEntity->Type = Type;
        ProjectileEntity->CellIndexY = CellIndexY;
        return ProjectileEntity;
    }
    else
    {
        PANIC("Overflown projectile entity buffer when trying to push a new one!");
    }
}

internal zombie_entity*
GameGardenGrid_PushZombieEntity(game_garden_grid* GardenGrid, zombie_type Type, u32 CellIndexY)
{
    ASSERT(Type != ZOMBIE_TYPE_NONE);

    if (GardenGrid->CurrentZombieCount < GardenGrid->MaxZombieCount)
    {
        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + GardenGrid->CurrentZombieCount;
        GardenGrid->CurrentZombieCount++;
        ZombieEntity->Type = Type;
        ZombieEntity->CellIndexY = CellIndexY;
        return ZombieEntity;
    }
    else
    {
        PANIC("Overflown zombie entity buffer when trying to push a new one!");
    }
}

internal u32
GameGardenGrid_RemovePendingDestroyEntities(void* EntityBuffer, memory_size EntityByteCount, u32 EntityCount)
{
    for (u32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
    {
        void* Entity = (u8*)EntityBuffer + ((memory_size)EntityIndex * EntityByteCount);
        const u16   EntityType              = (*(u32*)Entity) & 0x0000FFFF;
        const b8    EntityIsPendingDestroy  = (*(u32*)Entity) & 0x00FF0000;

        if (EntityIsPendingDestroy)
        {
            u8* SwapEntity = (u8*)EntityBuffer + ((memory_size)(EntityCount - 1) * EntityByteCount);
            while ((memory_index)SwapEntity > (memory_index)Entity)
            {
                const u16   SwapEntityType              = (*(u32*)SwapEntity) & 0x0000FFFF;
                const b8    SwapEntityIsPendingDestroy  = (*(u32*)SwapEntity) & 0x00FF0000;
                if (!SwapEntityIsPendingDestroy)
                {
                    break;
                }
                else
                {
                    ZeroMemory(SwapEntity, EntityByteCount);
                    --EntityCount;
                }

                SwapEntity -= EntityByteCount;
            }

            if (Entity != SwapEntity)
            {
                CopyMemory(Entity, SwapEntity, EntityByteCount);
            }

            ZeroMemory(SwapEntity, EntityByteCount);
            --EntityCount;
        }
    }

    return EntityCount;
}

internal void
GameGardenGrid_UpdatePlantSunflower(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                    u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    plant_entity_sunflower* Sunflower = &PlantEntity->Sunflower;

    const f32 DelayOffset = Random_RangeF32(&GardenGrid->RandomSeries,
                                            -Sunflower->GenerateDelayRandomOffset,
                                            +Sunflower->GenerateDelayRandomOffset);
    if (Sunflower->GenerateTimer >= Sunflower->GenerateDelayBase + DelayOffset)
    {
        // NOTE(Traian): Reset the generate timer.
        Sunflower->GenerateTimer = 0.0F;

        // NOTE(Traian): Determine the random offset to add to the sun spawn location.
        const f32 MinSpawnDistance = 0.5F;
        const f32 MaxSpawnDistance = 1.0F;
        vec2 SunSpawnOffset = {};
        SunSpawnOffset.X = Random_SignF32(&GardenGrid->RandomSeries) *
                           Random_RangeF32(&GardenGrid->RandomSeries, MinSpawnDistance, MaxSpawnDistance);
        SunSpawnOffset.Y = Random_SignF32(&GardenGrid->RandomSeries) *
                           Random_RangeF32(&GardenGrid->RandomSeries, MinSpawnDistance, MaxSpawnDistance);
        vec2 Position = CellPoint + SunSpawnOffset;
        Position.X = Clamp(Position.X,
                           GardenGrid->MinPoint.X + Sunflower->SunRadius,
                           GardenGrid->MaxPoint.X - Sunflower->SunRadius);
        Position.Y = Clamp(Position.Y,
                           GardenGrid->MinPoint.Y + Sunflower->SunRadius,
                           GardenGrid->MaxPoint.Y - Sunflower->SunRadius);

        // NOTE(Traian): Push a new sun "projectile" to the entity buffer.
        projectile_entity* SunProjectile = GameGardenGrid_PushProjectileEntity(GardenGrid,
                                                                               PROJECTILE_TYPE_SUN,
                                                                               CellIndexY);
        SunProjectile->Position = Position;
        SunProjectile->Radius = Sunflower->SunRadius;
        SunProjectile->Sun.SunAmount = Sunflower->SunAmount;
        SunProjectile->Sun.DecayDelay = Sunflower->SunDecayDelay;
    }
    else
    {
        Sunflower->GenerateTimer += DeltaTime;
    }
}

internal inline b8
GameGardenGrid_AreZombiesOnTheLane(game_state* GameState, u32 CellIndexX, u32 CellIndexY)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 CellPositionX = GameGardenGrid_GetCellPositionX(GardenGrid, CellIndexX);

    b8 AreZombiesOnTheLane = false;
    for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        const zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
        const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];

        const f32 ZombiePositionX = ZombieEntity->Position.X - (0.5F * ZombieConfig->Dimensions.X);
        if ((!ZombieEntity->IsPendingDestroy) &&
            (ZombieEntity->CellIndexY == CellIndexY) &&
            (ZombiePositionX >= CellPositionX))
        {
            AreZombiesOnTheLane = true;
            break;
        }
    }

    return AreZombiesOnTheLane;
}

internal inline zombie_entity*
GameGardenGrid_GetFirstZombieOnTheLane(game_state* GameState, u32 CellIndexX, u32 CellIndexY)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 CellPositionX = GameGardenGrid_GetCellPositionX(GardenGrid, CellIndexX);

    zombie_entity* ClosestZombieEntity = NULL;
    f32 ClosestZombieEntityPositionX;

    for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
        const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];

        const f32 ZombiePositionX = ZombieEntity->Position.X - (0.5F * ZombieConfig->Dimensions.X);
        if ((!ZombieEntity->IsPendingDestroy) &&
            (ZombieEntity->CellIndexY == CellIndexY) &&
            (ZombiePositionX >= CellPositionX))
        {
            if (!ClosestZombieEntity || ZombiePositionX < ClosestZombieEntityPositionX)
            {
                ClosestZombieEntity = ZombieEntity;
                ClosestZombieEntityPositionX = ZombiePositionX;
            }
        }
    }

    return ClosestZombieEntity;
}

internal inline b8
GameGardenGrid_ShootPeaProjectile(game_state* GameState, u32 CellIndexX, u32 CellIndexY, vec2 Position,
                                  f32 Velocity, f32 Damage, f32 Radius, b8 ShootOnlyWhenThereAreZombiesOnTheLane)
{
    b8 ShouldShoot = true;
    if (ShootOnlyWhenThereAreZombiesOnTheLane)
    {
        // NOTE(Traian): Only shoot when there are zombies on the plant's lane.
        ShouldShoot = GameGardenGrid_AreZombiesOnTheLane(GameState, CellIndexX, CellIndexY);
    }

    if (ShouldShoot)
    {
        // NOTE(Traian): Push a new pea projectile to the entity buffer.
        projectile_entity* PeaProjectile = GameGardenGrid_PushProjectileEntity(&GameState->GardenGrid,
                                                                               PROJECTILE_TYPE_PEA,
                                                                               CellIndexY);
        PeaProjectile->Position = Position;
        PeaProjectile->Radius = Radius;
        PeaProjectile->Pea.Velocity = Velocity;
        PeaProjectile->Pea.Damage = Damage;
    }

    return ShouldShoot;
}

internal void
GameGardenGrid_UpdatePlantPeashooter(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                     u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    plant_entity_peashooter* Peashooter = &PlantEntity->Peashooter;

    if (Peashooter->ShootTimer >= Peashooter->ShootDelay)
    {
        const vec2 ShootOffset = Vec2(PLANT_PEASHOOTER_SHOOT_POINT_OFFSET_X, PLANT_PEASHOOTER_SHOOT_POINT_OFFSET_Y);
        const vec2 ProjectilePosition = CellPoint + ShootOffset;

        if (GameGardenGrid_ShootPeaProjectile(GameState, CellIndexX, CellIndexY, ProjectilePosition,
                                              Peashooter->ProjectileVelocity, Peashooter->ProjectileDamage,
                                              Peashooter->ProjectileRadius, true))
        {
            Peashooter->ShootTimer = 0.0F;
        }
    }
    else
    {
        Peashooter->ShootTimer += DeltaTime;
    }
}

internal void
GameGardenGrid_UpdatePlantRepeater(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                   u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    plant_entity_repeater* Repeater = &PlantEntity->Repeater;

    const vec2 ShootOffset = Vec2(PLANT_REPEATER_SHOOT_POINT_OFFSET_X,
                                  PLANT_REPEATER_SHOOT_POINT_OFFSET_Y);
    const vec2 ProjectilePosition = CellPoint + ShootOffset;

    if (!Repeater->IsInShootSequence)
    {
        if (Repeater->ShootTimer >= Repeater->ShootSequenceDelay)
        {

            if (GameGardenGrid_ShootPeaProjectile(GameState, CellIndexX, CellIndexY, ProjectilePosition,
                                                  Repeater->ProjectileVelocity, Repeater->ProjectileDamage,
                                                  Repeater->ProjectileRadius, true))
            {
                Repeater->ShootTimer = 0.0F;
                Repeater->IsInShootSequence = true;
            }
        }
        else
        {
            Repeater->ShootTimer += DeltaTime;
        }
    }
    else
    {
        if (Repeater->ShootTimer >= Repeater->ShootSequenceDeltaDelay)
        {
            GameGardenGrid_ShootPeaProjectile(GameState, CellIndexX, CellIndexY, ProjectilePosition,
                                              Repeater->ProjectileVelocity, Repeater->ProjectileDamage,
                                              Repeater->ProjectileRadius, false);
            Repeater->ShootTimer = 0.0F;
            Repeater->IsInShootSequence = false;
        }
        else
        {
            Repeater->ShootTimer += DeltaTime;
        }
    }
}

internal void
GameGardenGrid_UpdatePlantTorchwood(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                    u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
}

internal void
GameGardenGrid_UpdatePlantMelonpult(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                    u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    plant_entity_melonpult* Melonpult = &PlantEntity->Melonpult;

    zombie_entity* TargetZombie = GameGardenGrid_GetFirstZombieOnTheLane(GameState, CellIndexX, CellIndexY);
    if (TargetZombie && !TargetZombie->IsPendingDestroy)
    {
        if (Melonpult->LaunchTimer >= Melonpult->LaunchDelay)
        {
            Melonpult->LaunchTimer = 0.0F;
            projectile_entity* MelonProjectile = GameGardenGrid_PushProjectileEntity(GardenGrid,
                                                                                     PROJECTILE_TYPE_MELON,
                                                                                     CellIndexY);
            const game_zombie_config* TargetZombieConfig = &GameState->Config.Zombies[TargetZombie->Type];

            const vec2 LaunchOffset = Vec2(PLANT_MELONPULT_LAUNCH_POINT_OFFSET_X,
                                           PLANT_MELONPULT_LAUNCH_POINT_OFFSET_Y);
            MelonProjectile->Position = CellPoint + LaunchOffset;
            MelonProjectile->Radius = Melonpult->ProjectileRadius;
            MelonProjectile->Melon.Damage = Melonpult->ProjectileDamage;
            MelonProjectile->Melon.SplashDamageRadius = Melonpult->ProjectileSplashDamageRadius;
            MelonProjectile->Melon.SplashDamageMultiplier = Melonpult->ProjectileSplashDamageMultiplier;
            MelonProjectile->Melon.StartPosition = CellPoint + LaunchOffset;
            MelonProjectile->Melon.TargetPosition.X = TargetZombie->Position.X;
            MelonProjectile->Melon.TargetPosition.Y = TargetZombie->Position.Y + 0.5F * TargetZombieConfig->Dimensions.Y;
            MelonProjectile->Melon.Velocity = Melonpult->ProjectileVelocity;
            MelonProjectile->Melon.TargetZombie = TargetZombie;
        }
        else
        {
            Melonpult->LaunchTimer += DeltaTime;
        }
    }
    else
    {
        Melonpult->LaunchTimer += DeltaTime;
    }
}

internal void
GameGardenGrid_UpdatePlantWallnut(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                  u32 CellIndexX, u32 CellIndexY, vec2 CellPoint, plant_entity* PlantEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    plant_entity_wallnut* Wallnut = &PlantEntity->Wallnut;

    const f32 CurrentHealthPercentage = PlantEntity->Health / Wallnut->MaxHealth;
    if (CurrentHealthPercentage <= Wallnut->CrackStage2HealthPercentage)
    {
        // NOTE(Traian): Cracked in stage 2.
        Wallnut->CrackIndex = 2;
    }
    else if (CurrentHealthPercentage <= Wallnut->CrackStage1HealthPercentage)
    {
        // NOTE(Traian): Cracked in stage 1.
        Wallnut->CrackIndex = 1;
    }
    else
    {
        // NOTE(Traian): No cracks.
        Wallnut->CrackIndex = 0;
    }
}

internal void
GameGardenGrid_UpdatePlants(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 InvCellCountX = 1.0F / (f32)GardenGrid->CellCountX;
    const f32 InvCellCountY = 1.0F / (f32)GardenGrid->CellCountY;

    //
    // NOTE(Traian): Update plant entities.
    //

	for (u32 CellIndexY = 0; CellIndexY < GardenGrid->CellCountY; ++CellIndexY)
    {
        for (u32 CellIndexX = 0; CellIndexX < GardenGrid->CellCountX; ++CellIndexX)
        {
            vec2 CellPoint;
            CellPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, ((f32)CellIndexX + 0.5F) * InvCellCountX);
            CellPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, ((f32)CellIndexY + 0.5F) * InvCellCountY);

            const u32 PlantEntityIndex = CellIndexX + (CellIndexY * GardenGrid->CellCountX);
            plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;
            if (PlantEntity->Type != PLANT_TYPE_NONE && !PlantEntity->IsPendingDestroy)
            {
                if (PlantEntity->Health <= 0.0F)
                {
                    // NOTE(Traian): The plant died and we should not run its update procedure any more.
                    PlantEntity->IsPendingDestroy = true;
                    continue;
                }

                // NOTE(Traian): This switch doesn't have to be exhaustive. There are plants that have no update logic.
                switch (PlantEntity->Type)
                {
#define _PVZ_UPDATE_PLANT(PlantType, PlaneName)                                                                 \
                    case PlantType:                                                                             \
                    {                                                                                           \
                        GameGardenGrid_UpdatePlant##PlaneName(GameState, PlatformState, DeltaTime,              \
                                                              CellIndexX, CellIndexY, CellPoint, PlantEntity);  \
                    }                                                                                           \
                    break;

                    PVZ_ENUMERATE_PLANT_TYPES(_PVZ_UPDATE_PLANT)
#undef _PVZ_UPDATE_PLANT
                }
            }
        }
    }
}

internal zombie_entity*
GameGardenGrid_SpawnZombie(game_state* GameState, zombie_type ZombieType, u32 CellIndexY, f32 PositionX)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    if (ZombieType != ZOMBIE_TYPE_NONE && ZombieType < ZOMBIE_TYPE_MAX_COUNT)
    {
        zombie_entity* ZombieEntity = GameGardenGrid_PushZombieEntity(GardenGrid, ZombieType, CellIndexY);
        const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieType];

        ZombieEntity->Position.X = PositionX;
        ZombieEntity->Position.Y = GameGardenGrid_GetCellPositionY(GardenGrid, CellIndexY);
        ZombieEntity->Health = ZombieConfig->Health;

        // NOTE(Traian): While there is no hard requirement that this switch is exhaustive, it would be weird to have a
        // zombie type that does *nothing* (the default structure doesn't provide any kind of state).
        switch (ZombieType)
        {
            case ZOMBIE_TYPE_NORMAL:
            {
                ZombieEntity->Normal.Velocity = -ZOMBIE_NORMAL_VELOCITY;
                ZombieEntity->Normal.AttackDamage = ZOMBIE_NORMAL_ATTACK_DAMAGE;
                ZombieEntity->Normal.AttackDelay = ZOMBIE_NORMAL_ATTACK_DELAY;
            }
            break;

            case ZOMBIE_TYPE_BUCKETHEAD:
            {
                ZombieEntity->Buckethead.Velocity = -ZOMBIE_BUCKETHEAD_VELOCITY;
                ZombieEntity->Buckethead.AttackDamage = ZOMBIE_BUCKETHEAD_ATTACK_DAMAGE;
                ZombieEntity->Buckethead.AttackDelay = ZOMBIE_BUCKETHEAD_ATTACK_DELAY;
                ZombieEntity->Buckethead.MaxHealth = ZombieEntity->Health;
                ZombieEntity->Buckethead.DamagedStage1HealthPercentage = ZOMBIE_BUCKETHEAD_DAMAGED_STAGE_1_HEALTH_PERCENTAGE;
                ZombieEntity->Buckethead.DamagedStage2HealthPercentage = ZOMBIE_BUCKETHEAD_DAMAGED_STAGE_2_HEALTH_PERCENTAGE;
                ZombieEntity->Buckethead.DamagedStage3HealthPercentage = ZOMBIE_BUCKETHEAD_DAMAGED_STAGE_3_HEALTH_PERCENTAGE;
            }
        }

        return ZombieEntity;
    }
    else
    {
        return NULL;
    }
}

internal f32
GameGardenGrid_CalculateZombieSpawnPointRate(f32 ElapsedTime)
{
    f32 SpawnPointRate = ElapsedTime * 0.2F;
    return SpawnPointRate;
}

internal void
GameGardenGrid_UpdateZombieSpawner(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    GardenGrid->ElapsedTime += DeltaTime;

    for (u16 ZombieType = ZOMBIE_TYPE_NONE + 1; ZombieType < ZOMBIE_TYPE_MAX_COUNT; ++ZombieType)
    {
        const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieType];
        while (GardenGrid->ZombieSpawnPoints[ZombieType] >= ZombieConfig->SpawnCost)
        {
            // NOTE(Traian): Chose a random garden lane where to spawn the zombie.
            const u32 SpawnCellIndexY = Random_RangeU32(&GardenGrid->RandomSeries, 0, GardenGrid->CellCountY - 1);
            const f32 SpawnPositionX = GameState->Camera.UnitCountX + 0.5F;

            GameGardenGrid_SpawnZombie(GameState, (zombie_type)ZombieType, SpawnCellIndexY, SpawnPositionX);
            GardenGrid->ZombieSpawnPoints[ZombieType] -= ZombieConfig->SpawnCost;
        }

        GardenGrid->ZombieSpawnPoints[ZombieType] += GardenGrid->ZombieSpawnPointRates[ZombieType] * DeltaTime;
        GardenGrid->ZombieSpawnPointRates[ZombieType] = GameGardenGrid_CalculateZombieSpawnPointRate(GardenGrid->ElapsedTime);
    }
}

internal b8
GameGardenGrid_ExcuteZombieBiteAttack(game_state* GameState, f32 DeltaTime, zombie_entity* ZombieEntity,
                                      f32* InOutAttackTimer, f32 AttackDelay, f32 AttackDamage)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];

    const f32 AttackPositionX = ZombieEntity->Position.X;
    const s32 AttackedCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, AttackPositionX);

    plant_entity* AttackedPlantEntity = NULL;
    if (0 <= AttackedCellIndexX && AttackedCellIndexX < GardenGrid->CellCountX)
    {
        const u32 PlantIndex = (ZombieEntity->CellIndexY * GardenGrid->CellCountX) + AttackedCellIndexX;
        plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantIndex;
        if (PlantEntity->Type != PLANT_TYPE_NONE)
        {
            const game_plant_config* PlantConfig = &GameState->Config.Plants[PlantEntity->Type];

            const f32 PlantPositionX = GameGardenGrid_GetCellPositionX(GardenGrid, AttackedCellIndexX);
            const f32 PlantMinPointX = PlantPositionX - (0.5F * PlantConfig->Dimensions.X);
            const f32 PlantMaxPointX = PlantPositionX + (0.5F * PlantConfig->Dimensions.X);

            if (PlantMinPointX <= AttackPositionX && AttackPositionX <= PlantMaxPointX)
            {
                AttackedPlantEntity = PlantEntity;
            }
        }
    }

    b8 HasAttackTarget;
    if (AttackedPlantEntity)
    {
        HasAttackTarget = true;
        if (*InOutAttackTimer >= AttackDelay)
        {
            *InOutAttackTimer = 0.0F;
            AttackedPlantEntity->Health -= AttackDamage;
        }
        else
        {
            *InOutAttackTimer += DeltaTime;
        }
    }
    else
    {
        HasAttackTarget = false;
        *InOutAttackTimer = 0.0F;
    }

    return HasAttackTarget;
}

internal void
GameGardenGrid_UpdateZombieNormal(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                  zombie_entity* ZombieEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];
    zombie_entity_normal* Normal = &ZombieEntity->Normal;

    if (ZombieEntity->Health <= 0.0F)
    {
        // NOTE(Traian): The zombie has died and we should not run its update procedure any more.
        ZombieEntity->IsPendingDestroy = true;
        return;
    }

    if (!GameGardenGrid_ExcuteZombieBiteAttack(GameState, DeltaTime, ZombieEntity,
                                               &Normal->AttackTimer, Normal->AttackDelay, Normal->AttackDamage))
    {
        // NOTE(Traian): The zombie has to attack target, so move forward.
        ZombieEntity->Position.X += Normal->Velocity * DeltaTime;
    }
}

internal void
GameGardenGrid_UpdateZombieBuckethead(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                      zombie_entity* ZombieEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];
    zombie_entity_buckethead* Buckethead = &ZombieEntity->Buckethead;

    if (ZombieEntity->Health <= 0.0F)
    {
        ZombieEntity->IsPendingDestroy = true;
        return;
    }

    if (!GameGardenGrid_ExcuteZombieBiteAttack(GameState, DeltaTime, ZombieEntity,
                                               &Buckethead->AttackTimer, Buckethead->AttackDelay, Buckethead->AttackDamage))
    {
        // NOTE(Traian): The zombie has to attack target, so move forward.
        ZombieEntity->Position.X += Buckethead->Velocity * DeltaTime;
    }

    const f32 CurrentHealthPercentage = ZombieEntity->Health / Buckethead->MaxHealth;
    if (CurrentHealthPercentage <= Buckethead->DamagedStage3HealthPercentage)
    {
        // NOTE(Traian): Damaged stage 3.
        Buckethead->DamagedStageIndex = 3;
    }
    else if (CurrentHealthPercentage <= Buckethead->DamagedStage2HealthPercentage)
    {
        // NOTE(Traian): Damaged stage 2.
        Buckethead->DamagedStageIndex = 2;
    }
    else if (CurrentHealthPercentage <= Buckethead->DamagedStage1HealthPercentage)
    {
        // NOTE(Traian): Damaged stage 1.
        Buckethead->DamagedStageIndex = 1;
    }
    else
    {
        // NOTE(Traian): Damaged stage 0 (undamaged).
        Buckethead->DamagedStageIndex = 0;
    }
}

internal void
GameGardenGrid_UpdateZombies(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;

	//
    // NOTE(Traian): Spawn new zombies.
    //

    GameGardenGrid_UpdateZombieSpawner(GameState, PlatformState, DeltaTime);

    //
    // NOTE(Traian): Update zombie entities.
    //

	for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
        if (!ZombieEntity->IsPendingDestroy)
        {
            const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];
            
            if (ZombieEntity->Position.X <= -0.2F)
            {
                // TODO(Traian): Open the game-ended screen.
                ZombieEntity->IsPendingDestroy = true;
                break;
            }

            // NOTE(Traian): While there is no hard requirement that this switch is exhaustive, it would be weird to have a
            // zombie type that does *nothing* (the default behaviour doesn't provide any kind of movement or attack logic).
            switch (ZombieEntity->Type)
            {
#define _PVZ_UPDATE_ZOMBIE(ZombieType, ZombieName)                                                              \
                case ZombieType:                                                                                \
                {                                                                                               \
                    GameGardenGrid_UpdateZombie##ZombieName(GameState, PlatformState, DeltaTime, ZombieEntity); \
                }                                                                                               \
                break;

                PVZ_ENUMERATE_ZOMBIE_TYPES(_PVZ_UPDATE_ZOMBIE)
#undef _PVZ_UPDATE_ZOMBIE
            }
        }
    }
}

internal void
GameGardenGrid_UpdateProjectileSun(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                   projectile_entity* ProjectileEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    projectile_entity_sun* Sun = &ProjectileEntity->Sun;

    const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                Vec2(PlatformState->Input->MousePositionX,
                                                                     PlatformState->Input->MousePositionY));

    if (Vec2_DistanceSquared(ProjectileEntity->Position, GameMousePosition) <= (ProjectileEntity->Radius * ProjectileEntity->Radius))
    {
        if (PlatformState->Input->Keys[GAME_INPUT_KEY_LEFT_MOUSE_BUTTON].WasPressedThisFrame)
        {
            ASSERT(!ProjectileEntity->IsPendingDestroy);
            GameState->SunCounter.SunAmount += Sun->SunAmount;
            ProjectileEntity->IsPendingDestroy = true;
        }
    }

    if (Sun->DecayTimer >= Sun->DecayDelay)
    {
        Sun->DecayTimer = 0.0F;
        ProjectileEntity->IsPendingDestroy = true;
    }
    else
    {
        Sun->DecayTimer += DeltaTime;
    }
}

internal zombie_entity*
GameGardenGrid_UpdateLinearProjectile(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                      projectile_entity* ProjectileEntity, f32 Velocity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    ProjectileEntity->Position.X += Velocity * DeltaTime;

    zombie_entity* ClosestZombieEntity = NULL;
    f32 ClosestZombieDistance;

    for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
        if (!ZombieEntity->IsPendingDestroy && (ZombieEntity->Health > 0) &&
            (ZombieEntity->CellIndexY == ProjectileEntity->CellIndexY))
        {
            const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];
            const f32 SignedDistance = ZombieEntity->Position.X - ProjectileEntity->Position.X;
            if (Abs(SignedDistance) <= (0.5F * ZombieConfig->Dimensions.X))
            {
                // NOTE(Traian): The projectile is inside the zombie.
                if (!ClosestZombieEntity || (ClosestZombieDistance > Abs(SignedDistance)))
                {
                    ClosestZombieEntity = ZombieEntity;
                    ClosestZombieDistance = Abs(SignedDistance);
                }
            }
        }
    }

    return ClosestZombieEntity;
}

internal void
GameGardenGrid_UpdateProjectilePea(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                   projectile_entity* ProjectileEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    projectile_entity_pea* Pea = &ProjectileEntity->Pea;
    
    zombie_entity* HitZombieEntity = GameGardenGrid_UpdateLinearProjectile(GameState, PlatformState, DeltaTime,
                                                                           ProjectileEntity, Pea->Velocity);
    if (HitZombieEntity)
    {
        HitZombieEntity->Health -= Pea->Damage;
        ProjectileEntity->IsPendingDestroy = true;
    }

    if (!ProjectileEntity->IsPendingDestroy)
    {
        const s32 GridCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, ProjectileEntity->Position.X);
        const s32 GridCellIndexY = GameGardenGrid_GetCellIndexY(GardenGrid, ProjectileEntity->Position.Y);
        if ((0 <= GridCellIndexX && GridCellIndexX < GardenGrid->CellCountX) &&
            (0 <= GridCellIndexY && GridCellIndexY < GardenGrid->CellCountY))
        {
            const u32 PlantEntityIndex = (GridCellIndexY * GardenGrid->CellCountX) + GridCellIndexX;
            plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;
            
            const f32 CellPointX = GameGardenGrid_GetCellPositionX(GardenGrid, GridCellIndexX);
            if (PlantEntity->Type == PLANT_TYPE_TORCHWOOD && ProjectileEntity->Position.X >= CellPointX)
            {
                projectile_entity_fire_pea FirePea = {};
                FirePea.Velocity = Pea->Velocity;
                FirePea.Damage = Pea->Damage * PlantEntity->Torchwood.DamageMultiplier;

                ProjectileEntity->Type = PROJECTILE_TYPE_FIRE_PEA;
                ProjectileEntity->FirePea = FirePea;
            }
        }
    }
}

internal void
GameGardenGrid_UpdateProjectileFirePea(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                       projectile_entity* ProjectileEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    projectile_entity_fire_pea* FirePea = &ProjectileEntity->FirePea;
    
    zombie_entity* HitZombieEntity = GameGardenGrid_UpdateLinearProjectile(GameState, PlatformState, DeltaTime,
                                                                           ProjectileEntity, FirePea->Velocity);
    if (HitZombieEntity)
    {
        HitZombieEntity->Health -= FirePea->Damage;
        ProjectileEntity->IsPendingDestroy = true;
    }
}

internal void
GameGardenGrid_UpdateProjectileMelon(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime,
                                     projectile_entity* ProjectileEntity)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    projectile_entity_melon* Melon = &ProjectileEntity->Melon;

    if (Melon->TargetZombie && Melon->TargetZombie->IsPendingDestroy)
    {
        Melon->TargetZombie = NULL;
    }

    if (Melon->TargetZombie)
    {
        // NOTE(Traian): Update the target position.
        const game_zombie_config* TargetZombieConfig = &GameState->Config.Zombies[Melon->TargetZombie->Type];
        Melon->TargetPosition.X = Melon->TargetZombie->Position.X;
        Melon->TargetPosition.Y = Melon->TargetZombie->Position.Y + 0.5F * TargetZombieConfig->Dimensions.Y;
    }

    const f32 X1 = Melon->StartPosition.X;
    const f32 Y1 = Melon->StartPosition.Y;
    const f32 X2 = Melon->TargetPosition.X;
    const f32 Y2 = Melon->TargetPosition.Y;

    // NOTE(Traian): The equation of a parabola that contains the points (X1, Y1) and (X2, Y2),
    // and has a height coefficient of 'A'.
    const f32 A = -0.2F;
    const f32 B = (Y1 - Y2) / (X1 - X2) - A * (X1 + X2);
    const f32 C = A * X1 * X2 + (X1 * Y2 - X2 * Y1) / (X1 - X2);

    const f32 X = ProjectileEntity->Position.X + Melon->Velocity * DeltaTime;
    const f32 Y = A * (X * X) + B * X + C;

    ProjectileEntity->Position.X = X;
    ProjectileEntity->Position.Y = Y;

    if (ProjectileEntity->Position.X >= Melon->TargetPosition.X)
    {
        zombie_entity* ClosestZombieEntity = NULL;
        f32 ClosestZombieDistance;

        for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
        {
            zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
            if (!ZombieEntity->IsPendingDestroy)
            {
                const f32 Distance = Abs(ZombieEntity->Position.X - ProjectileEntity->Position.X);
                if (Distance <= Melon->SplashDamageRadius)
                {
                    if (!ClosestZombieEntity || ClosestZombieDistance > Distance)
                    {
                        ClosestZombieEntity = ClosestZombieEntity;
                        ClosestZombieDistance = Distance; 
                    }

                    if (ZombieEntity != Melon->TargetZombie)
                    {
                        // NOTE(Traian): Apply splash damage.
                        ZombieEntity->Health -= Melon->SplashDamageMultiplier * Melon->Damage;
                    }
                }
            }
        }

        if (Melon->TargetZombie)
        {
            // NOTE(Traian): No splash damage was previously applied.
            Melon->TargetZombie->Health -= Melon->Damage;       
        }
        else if (ClosestZombieEntity)
        {
            // NOTE(Traian): Account for the splash damage applied before.
            ClosestZombieEntity->Health -= Melon->Damage * (1.0F - Melon->SplashDamageMultiplier);
        }

        ProjectileEntity->IsPendingDestroy = true;
    }
}

internal void
GameGardenGrid_UpdateProjectiles(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
	for (u32 ProjectileIndex = 0; ProjectileIndex < GardenGrid->CurrentProjectileCount; ++ProjectileIndex)
    {
        projectile_entity* ProjectileEntity = GardenGrid->ProjectileEntities + ProjectileIndex;
        const game_projectile_config* ProjectileConfig = &GameState->Config.Projectiles[ProjectileEntity->Type];

        if (!ProjectileEntity->IsPendingDestroy)
        {
            const vec2 Dimensions = Vec2(2.0F * ProjectileEntity->Radius, 2.0F * ProjectileEntity->Radius);
            const vec2 RenderScale = ProjectileConfig->RenderScale;
            const vec2 RenderOffset = ProjectileConfig->RenderOffset;
            const vec2 RenderDimensions = Vec2(Dimensions.X * RenderScale.X, Dimensions.Y * RenderScale.Y);
            const f32 HalfDimensionsX = 0.5F * Dimensions.X;
            const f32 HalfRenderDimensionsX = 0.5F * RenderDimensions.X;

            // NOTE(Traian): Check if both the projectile logic bounding box and the render bounding box have
            // gone out of the visible area, and if so mark is pending-destroy.
            if ((ProjectileEntity->Position.X <= -HalfDimensionsX) ||
                (ProjectileEntity->Position.X >= GameState->Camera.UnitCountX + HalfDimensionsX) ||
                (ProjectileEntity->Position.X + RenderOffset.X <= -HalfRenderDimensionsX) ||
                (ProjectileEntity->Position.X + RenderOffset.X >= GameState->Camera.UnitCountX + HalfRenderDimensionsX))
            {
                ProjectileEntity->IsPendingDestroy = true;
                continue;
            }

            // NOTE(Traian): There is no hard requirement that this switch is exhaustive, however it would be weird to
            // exist a projectile type that has no logic attached to it.
            switch(ProjectileEntity->Type)
            {
#define _PVZ_UPDATE_PROJECTILE(ProjectileType, ProjectileName)                                                              \
                case ProjectileType:                                                                                        \
                {                                                                                                           \
                    GameGardenGrid_UpdateProjectile##ProjectileName(GameState, PlatformState, DeltaTime, ProjectileEntity); \
                }                                                                                                           \
                break;

                PVZ_ENUMERATE_PROJECTILE_TYPES(_PVZ_UPDATE_PROJECTILE);
#undef _PVZ_UPDATE_PROJECTILE
            }
        }
    }
}

internal void
GameGardenGrid_Update(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;

	//
    // NOTE(Traian): Calculate the garden grid position and dimensions.
    //
    
    const vec2 GARDEN_MIN_POINT_PERCENTAGE = Vec2(0.05F, 0.01F);
    const vec2 GARDEN_MAX_POINT_PERCENTAGE = Vec2(0.95F, 0.80F);
    GardenGrid->MinPoint.X = GameState->Camera.UnitCountX * GARDEN_MIN_POINT_PERCENTAGE.X;
    GardenGrid->MinPoint.Y = GameState->Camera.UnitCountY * GARDEN_MIN_POINT_PERCENTAGE.Y;
    GardenGrid->MaxPoint.X = GameState->Camera.UnitCountX * GARDEN_MAX_POINT_PERCENTAGE.X;
    GardenGrid->MaxPoint.Y = GameState->Camera.UnitCountY * GARDEN_MAX_POINT_PERCENTAGE.Y;

    //
    // NOTE(Traian): Spawn natural suns.
    //

    if (GardenGrid->SpawnNextNaturalSunTimer >= GardenGrid->SpawnNextNaturalSunDelay)
    {
        GardenGrid->SpawnNextNaturalSunTimer = 0.0F;
        GardenGrid->SpawnNextNaturalSunDelay = Random_RangeF32(&GardenGrid->RandomSeries,
                                                               GardenGrid->SpawnNaturalSunMinDelay,
                                                               GardenGrid->SpawnNaturalSunMaxDelay);

        projectile_entity* SunEntity = GameGardenGrid_PushProjectileEntity(GardenGrid,
                                                                           PROJECTILE_TYPE_SUN,
                                                                           0);
        SunEntity->Position = Random_PointInRectangle2D(&GardenGrid->RandomSeries,
                                                        Rect2D(GardenGrid->MinPoint, GardenGrid->MaxPoint));
        SunEntity->Radius = PLANT_SUNFLOWER_SUN_RADIUS;
        SunEntity->Sun.SunAmount = PLANT_SUNFLOWER_SUN_AMOUNT;
        SunEntity->Sun.DecayDelay = PLANT_SUNFLOWER_SUN_DECAY;
    }
    else
    {
        GardenGrid->SpawnNextNaturalSunTimer += DeltaTime;
    }

    //
    // NOTE(Traian): Update components.
    //

    GameGardenGrid_UpdatePlants(GameState, PlatformState, DeltaTime);
    GameGardenGrid_UpdateZombies(GameState, PlatformState, DeltaTime);
    GameGardenGrid_UpdateProjectiles(GameState, PlatformState, DeltaTime);

    //
    // NOTE(Traian): Remove pending-destroy entities.
    //

    for (u32 CellIndexY = 0; CellIndexY < GardenGrid->CellCountY; ++CellIndexY)
    {
        for (u32 CellIndexX = 0; CellIndexX < GardenGrid->CellCountX; ++CellIndexX)
        {
            const u32 PlantIndex = (CellIndexY * GardenGrid->CellCountX) + CellIndexX;
            plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantIndex;
            if (PlantEntity->IsPendingDestroy)
            {
                ZERO_STRUCT_POINTER(PlantEntity);
                PlantEntity->Type = PLANT_TYPE_NONE; // Redundant.
            }
        }
    }

    GardenGrid->CurrentProjectileCount = GameGardenGrid_RemovePendingDestroyEntities(GardenGrid->ProjectileEntities,
                                                                                     sizeof(projectile_entity),
                                                                                     GardenGrid->CurrentProjectileCount);

    GardenGrid->CurrentZombieCount = GameGardenGrid_RemovePendingDestroyEntities(GardenGrid->ZombieEntities,
                                                                                 sizeof(zombie_entity),
                                                                                 GardenGrid->CurrentZombieCount);
}

//====================================================================================================================//
//------------------------------------------------------ RENDER ------------------------------------------------------//
//====================================================================================================================//

internal const f32 GARDEN_GRID_GRASS_TILE_GRID_Z_OFFSET  = 1.0F;
internal const f32 GARDEN_GRID_PLANTS_BASE_Z_OFFSET      = 2.0F;
internal const f32 GARDEN_GRID_ZOMBIES_BASE_Z_OFFSET     = 10.0F;
internal const f32 GARDEN_GRID_PROJECTILES_BASE_Z_OFFSET = 20.0F;

internal void
GameGardenGrid_RenderPlants(game_state* GameState, game_platform_state* PlatformState)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 InvCellCountX = 1.0F / (f32)GardenGrid->CellCountX;
    const f32 InvCellCountY = 1.0F / (f32)GardenGrid->CellCountY;

	for (u32 CellIndexY = 0; CellIndexY < GardenGrid->CellCountY; ++CellIndexY)
    {
        for (u32 CellIndexX = 0; CellIndexX < GardenGrid->CellCountX; ++CellIndexX)
        {
            const u32 PlantEntityIndex = CellIndexX + (CellIndexY * GardenGrid->CellCountX);
            const plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantEntityIndex;

            vec2 CellPoint;
            CellPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, ((f32)CellIndexX + 0.5F) * InvCellCountX);
            CellPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, ((f32)CellIndexY + 0.5F) * InvCellCountY);

            // NOTE(Traian): Render the lanes such that the entities on one lane are always on top of the entities
            // from the lanes below it.
            // NOTE(Traian): Since only one plant is rendered in a cell at any given time, we don't have to worry
            // about the Z ordering of plants (unlike zombies).
            const f32 RenderZOffset = GARDEN_GRID_PLANTS_BASE_Z_OFFSET + (GardenGrid->CellCountY - CellIndexY - 1);

            if (PlantEntity->Type != PLANT_TYPE_NONE && PlantEntity->Type < PLANT_TYPE_MAX_COUNT)
            {
                const game_plant_config* PlantConfig = &GameState->Config.Plants[PlantEntity->Type];
                const vec2 Dimensions = PlantConfig->Dimensions;
                const vec2 RenderScale = PlantConfig->RenderScale;
                const vec2 RenderOffset = PlantConfig->RenderOffset;
                const vec2 RenderDimensions = Vec2(Dimensions.X * RenderScale.X, Dimensions.Y * RenderScale.Y);
                const vec2 MinPoint = CellPoint - (0.5F * RenderDimensions) + RenderOffset;
                const vec2 MaxPoint = MinPoint + RenderDimensions;

                if (!PlantConfig->UseCustomRenderProcedure)
                {
                    asset* TextureAsset = Asset_Get(&GameState->Assets, PlantConfig->AssetID);
                    if (TextureAsset)
                    {
                        Renderer_PushPrimitive(&GameState->Renderer,
                                               Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                               Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                               RenderZOffset, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                                               &TextureAsset->Texture.RendererTexture);
                    }
                }
                else
                {
                    // NOTE(Traian): Render the plants that have the 'UseCustomRenderProcedure' configuration flag set to true.
                    switch (PlantEntity->Type)
                    {
                        case PLANT_TYPE_WALLNUT:
                        {
                            const plant_entity_wallnut* Wallnut = &PlantEntity->Wallnut;
                            game_asset_id TextureAssetID = GAME_ASSET_ID_NONE;
                            switch (Wallnut->CrackIndex)
                            {
                                case 0: { TextureAssetID = GAME_ASSET_ID_PLANT_WALLNUT_NORMAL; }    break;
                                case 1: { TextureAssetID = GAME_ASSET_ID_PLANT_WALLNUT_CRACKED_1; } break;
                                case 2: { TextureAssetID = GAME_ASSET_ID_PLANT_WALLNUT_CRACKED_2; } break;
                            }

                            asset* TextureAsset = Asset_Get(&GameState->Assets, TextureAssetID);
                            if (TextureAsset)
                            {
                                Renderer_PushPrimitive(&GameState->Renderer,
                                                       Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                                       Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                                       RenderZOffset, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                                                       &TextureAsset->Texture.RendererTexture);   
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
}

internal void
GameGardenGrid_RenderZombies(game_state* GameState, game_platform_state* PlatformState)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;

	for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        const zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;

        // NOTE(Traian): Render the lanes such that the entities on one lane are always on top of the entities
        // from the lanes below it.
        // NOTE(Traian): Primitives with the same Z-offset are rendered in the same order as 'Renderer_PushPrimitive'
        // is called. Since any two zombies maintain their relative order in the entity buffer no matter what other
        // entities are created/destroyed, no Z-fighting will occurr.
        // TODO(Traian): Maybe the zombies should be sorted based on the position such that the zombie closest to
        // the house will always be rendered on top on the other zombies on the same line?
        const f32 RenderZOffset = GARDEN_GRID_ZOMBIES_BASE_Z_OFFSET + (GardenGrid->CellCountY - ZombieEntity->CellIndexY - 1);

        if (!ZombieEntity->IsPendingDestroy)
        {
            if (ZombieEntity->Type < ZOMBIE_TYPE_MAX_COUNT)
            {
                const game_zombie_config* ZombieConfig = &GameState->Config.Zombies[ZombieEntity->Type];
                const vec2 Dimensions = ZombieConfig->Dimensions;
                const vec2 RenderScale = ZombieConfig->RenderScale;
                const vec2 RenderOffset = ZombieConfig->RenderOffset;
                const vec2 RenderDimensions = Vec2(Dimensions.X * RenderScale.X, Dimensions.Y * RenderScale.Y);

                const vec2 MinPoint = ZombieEntity->Position - (0.5F * RenderDimensions) + RenderOffset;
                const vec2 MaxPoint = MinPoint + RenderDimensions;

                if (!ZombieConfig->UseCustomRenderProcedure)
                {
                    asset* TextureAsset = Asset_Get(&GameState->Assets, ZombieConfig->AssetID);
                    if (TextureAsset)
                    {
                        Renderer_PushPrimitive(&GameState->Renderer,
                                               Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                               Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                               RenderZOffset, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                                               &TextureAsset->Texture.RendererTexture);
                    }
                }
                else
                {
                    switch (ZombieEntity->Type)
                    {
                        case ZOMBIE_TYPE_BUCKETHEAD:
                        {
                            const zombie_entity_buckethead* Buckethead = &ZombieEntity->Buckethead;
                            game_asset_id BucketTextureAssetID = GAME_ASSET_ID_NONE;
                            switch (Buckethead->DamagedStageIndex)
                            {
                                case 0: { BucketTextureAssetID = GAME_ASSET_ID_ZOMBIE_BUCKET_DAMAGED_0; }   break;
                                case 1: { BucketTextureAssetID = GAME_ASSET_ID_ZOMBIE_BUCKET_DAMAGED_1; }   break;
                                case 2: { BucketTextureAssetID = GAME_ASSET_ID_ZOMBIE_BUCKET_DAMAGED_2; }   break;
                                case 3: { BucketTextureAssetID = GAME_ASSET_ID_NONE; }                      break;
                            }

                            asset* ZombieTextureAsset = Asset_Get(&GameState->Assets, ZombieConfig->AssetID);
                            if (ZombieTextureAsset)
                            {
                                Renderer_PushPrimitive(&GameState->Renderer,
                                                       Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                                       Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                                       RenderZOffset, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                                                       &ZombieTextureAsset->Texture.RendererTexture);
                                
                            }

                            if (BucketTextureAssetID != GAME_ASSET_ID_NONE)
                            {
                            asset* BucketTextureAsset = Asset_Get(&GameState->Assets, BucketTextureAssetID);
                                if (BucketTextureAsset)
                                {
                                    const vec2 BucketDimensions = Vec2(ZOMBIE_BUCKETHEAD_BUCKET_DIMENSIONS_X,
                                                                       ZOMBIE_BUCKETHEAD_BUCKET_DIMENSIONS_Y);
                                    const vec2 BucketRenderOffset = Vec2(ZOMBIE_BUCKETHEAD_BUCKET_RENDER_OFFSET_X,
                                                                         ZOMBIE_BUCKETHEAD_BUCKET_RENDER_OFFSET_Y);
                                    const vec2 BucketMinPoint = ZombieEntity->Position + BucketRenderOffset - (0.5F * BucketDimensions);
                                    const vec2 BucketMaxPoint = BucketMinPoint + BucketDimensions;

                                    Renderer_PushPrimitive(&GameState->Renderer,
                                                           Game_TransformGamePointToNDC(&GameState->Camera, BucketMinPoint),
                                                           Game_TransformGamePointToNDC(&GameState->Camera, BucketMaxPoint),
                                                           RenderZOffset, Color4(1.0F), Vec2(0.0F), Vec2(1.0F),
                                                           &BucketTextureAsset->Texture.RendererTexture);
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
}

internal void
GameGardenGrid_RenderProjectiles(game_state* GameState, game_platform_state* PlatformState)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;

	for (u32 ProjectileIndex = 0; ProjectileIndex < GardenGrid->CurrentProjectileCount; ++ProjectileIndex)
    {
        const projectile_entity* ProjectileEntity = GardenGrid->ProjectileEntities + ProjectileIndex;
        if (!ProjectileEntity->IsPendingDestroy)
        {
            const game_projectile_config* ProjectileConfig = &GameState->Config.Projectiles[ProjectileEntity->Type];
            
            f32 RenderZOffset = GARDEN_GRID_PROJECTILES_BASE_Z_OFFSET;
            if (ProjectileEntity->Type != PROJECTILE_TYPE_SUN)
            {
                // NOTE(Traian): Always render the suns behind any other kind of projectiles.
                RenderZOffset += 1.0F;
            }

            const vec2 Dimensions = Vec2(2.0F * ProjectileEntity->Radius, 2.0F * ProjectileEntity->Radius);
            const vec2 RenderScale = ProjectileConfig->RenderScale;
            const vec2 RenderOffset = ProjectileConfig->RenderOffset;
            const vec2 RenderDimensions = Vec2(Dimensions.X * RenderScale.X, Dimensions.Y * RenderScale.Y);
            const vec2 RenderOffsetScale = Vec2(ProjectileEntity->Radius / 1.0F, ProjectileEntity->Radius / 1.0F);

            const vec2 MinPoint = ProjectileEntity->Position - (0.5F * RenderDimensions) +
                                  Vec2(RenderOffset.X * RenderOffsetScale.X, RenderOffset.Y * RenderOffsetScale.Y);
            const vec2 MaxPoint = MinPoint + RenderDimensions;

            asset* TextureAsset = Asset_Get(&GameState->Assets, ProjectileConfig->AssetID);
            if (TextureAsset)
            {
                Renderer_PushPrimitive(&GameState->Renderer,
                                       Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                       Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                       RenderZOffset, Color4(1.0), Vec2(0.0F), Vec2(1.0F),
                                       &TextureAsset->Texture.RendererTexture);   
            }
        }
    }
}

internal void
GameGardenGrid_Render(game_state* GameState, game_platform_state* PlatformState)
{
    game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 InvCellCountX = 1.0F / (f32)GardenGrid->CellCountX;
    const f32 InvCellCountY = 1.0F / (f32)GardenGrid->CellCountY;

	//
    // NOTE(Traian): Render garden grass tile grid.
    //

    for (u32 CellIndexY = 0; CellIndexY < GardenGrid->CellCountY; ++CellIndexY)
    {
        for (u32 CellIndexX = 0; CellIndexX < GardenGrid->CellCountX; ++CellIndexX)
        {
            vec2 CellMinPoint;
            vec2 CellMaxPoint;
            CellMinPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, (f32)CellIndexX * InvCellCountX);
            CellMinPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, (f32)CellIndexY * InvCellCountY);
            CellMaxPoint.X = Math_Lerp(GardenGrid->MinPoint.X, GardenGrid->MaxPoint.X, (f32)(CellIndexX + 1) * InvCellCountX);
            CellMaxPoint.Y = Math_Lerp(GardenGrid->MinPoint.Y, GardenGrid->MaxPoint.Y, (f32)(CellIndexY + 1) * InvCellCountY);

            const color4 GRASS_CELL_COLOR_TABLE[2] =
            {
                Color4_FromLinear(LinearColor(53, 122, 50)), // NOTE(Traian): Dark.
                Color4_FromLinear(LinearColor(92, 150, 59)), // NOTE(Traian): Light.
            };
            const u32 CellColorIndex = (CellIndexX + CellIndexY) % 2;

            Renderer_PushPrimitive(&GameState->Renderer,
                                   Game_TransformGamePointToNDC(&GameState->Camera, CellMinPoint),
                                   Game_TransformGamePointToNDC(&GameState->Camera, CellMaxPoint),
                                   GARDEN_GRID_GRASS_TILE_GRID_Z_OFFSET, GRASS_CELL_COLOR_TABLE[CellColorIndex]);
        }
    }

    //
    // NOTE(Traian): Render components.
    //

	GameGardenGrid_RenderPlants(GameState, PlatformState);
	GameGardenGrid_RenderZombies(GameState, PlatformState);
	GameGardenGrid_RenderProjectiles(GameState, PlatformState);
}
