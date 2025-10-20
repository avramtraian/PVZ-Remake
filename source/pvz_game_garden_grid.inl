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

    GardenGrid->SpawnZombieMinDelay = 1.0F;
    GardenGrid->SpawnZombieMaxDelay = 3.0F;

    // NOTE(Traian): Initialize the random series.
    Random_InitializeSeries(&GardenGrid->RandomSeries);
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
        SunProjectile->Sun.Radius = Sunflower->SunRadius;
        SunProjectile->Sun.SunAmount = Sunflower->SunAmount;
        SunProjectile->Sun.DecayDelay = Sunflower->SunDecayDelay;
    }
    else
    {
        Sunflower->GenerateTimer += DeltaTime;
    }
}

internal inline b8
GameGardenGrid_ShootPeaProjectile(game_garden_grid* GardenGrid, u32 CellIndexY, vec2 Position,
                                  f32 Velocity, f32 Damage, f32 Radius,
                                  b8 ShootOnlyWhenThereAreZombiesOnTheLane)
{
    b8 ShouldShoot = true;
    if (ShootOnlyWhenThereAreZombiesOnTheLane)
    {
        ShouldShoot = false;
        for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
        {
            const zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
            if (ZombieEntity->CellIndexY == CellIndexY)
            {
                ShouldShoot = true;
                break;
            }
        }
    }

    if (ShouldShoot)
    {
        // NOTE(Traian): Push a new pea projectile to the entity buffer.
        projectile_entity* PeaProjectile = GameGardenGrid_PushProjectileEntity(GardenGrid,
                                                                               PROJECTILE_TYPE_PEA,
                                                                               CellIndexY);
        PeaProjectile->Position = Position;
        PeaProjectile->Pea.Velocity = Velocity;
        PeaProjectile->Pea.Damage = Damage;
        PeaProjectile->Pea.Radius = Radius;
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

        if (GameGardenGrid_ShootPeaProjectile(GardenGrid, CellIndexY, ProjectilePosition,
                                              Peashooter->ProjectileVelocity, Peashooter->ProjectileDamage,
                                              Peashooter->ProjectileRadius, true))
        {
            // NOTE(Traian): Reset the shoot timer.
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

            if (GameGardenGrid_ShootPeaProjectile(GardenGrid, CellIndexY, ProjectilePosition,
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
            GameGardenGrid_ShootPeaProjectile(GardenGrid, CellIndexY, ProjectilePosition,
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
GameGardenGrid_UpdatePlants(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;
    const f32 InvCellCountX = 1.0F / (f32)GardenGrid->CellCountX;
    const f32 InvCellCountY = 1.0F / (f32)GardenGrid->CellCountY;

    const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                Vec2(PlatformState->Input->MousePositionX,
                                                                     PlatformState->Input->MousePositionY));

	//
    // NOTE(Traian): Place new plants using the debug placement system.
    //

    const s32 MouseCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, GameMousePosition.X);
    const s32 MouseCellIndexY = GameGardenGrid_GetCellIndexY(GardenGrid, GameMousePosition.Y);

    // INTERNAL_LOG("%5f x %5f %5f x %5f %3d x %3d\n",
    //              GameMousePosition.X, GameMousePosition.Y,
    //              GardenGrid->MinPoint.X, GardenGrid->MaxPoint.Y,
    //              MouseCellIndexX, MouseCellIndexY);

    if ((0 <= MouseCellIndexX && MouseCellIndexX < GardenGrid->CellCountX) &&
        (0 <= MouseCellIndexY && MouseCellIndexY < GardenGrid->CellCountY))
    {
        const u32 PlantIndex = (MouseCellIndexY * GardenGrid->CellCountX) + MouseCellIndexX;
        plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantIndex;
        if (PlantEntity->Type != PLANT_TYPE_NONE)
        {
            if (PlatformState->Input->Keys[GAME_INPUT_KEY_F1].WasPressedThisFrame)
            {
                PlantEntity->IsPendingDestroy = true;
            }
        }
    }

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

                switch (PlantEntity->Type)
                {
                    case PLANT_TYPE_SUNFLOWER:
                    {
                    }
                    break;

                    case PLANT_TYPE_PEASHOOTER:
                    {
                        GameGardenGrid_UpdatePlantPeashooter(GameState, PlatformState, DeltaTime,
                                                             CellIndexX, CellIndexY, CellPoint, PlantEntity);
                    }
                    break;

                    case PLANT_TYPE_REPEATER:
                    {
                        GameGardenGrid_UpdatePlantRepeater(GameState, PlatformState, DeltaTime,
                                                                   CellIndexX, CellIndexY, CellPoint, PlantEntity);
                    }
                    break;
                }
            }
        }
    }
}

internal void
GameGardenGrid_UpdateZombies(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;

	//
    // NOTE(Traian): Spawn new zombies.
    //

    if (GardenGrid->SpawnNextZombieTimer >= GardenGrid->SpawnNextZombieDelay)
    {
        GardenGrid->SpawnNextZombieTimer = 0.0F;
        GardenGrid->SpawnNextZombieDelay = Random_RangeF32(&GardenGrid->RandomSeries,
                                                           GardenGrid->SpawnZombieMinDelay,
                                                           GardenGrid->SpawnZombieMaxDelay);
        if (GardenGrid->CellCountY > 0)
        {
            const u32 SpawnCellIndexY = Random_RangeU32(&GardenGrid->RandomSeries,
                                                        0,
                                                        GardenGrid->CellCountY - 1);
            zombie_entity* ZombieEntity = GameGardenGrid_PushZombieEntity(GardenGrid, ZOMBIE_TYPE_NORMAL, SpawnCellIndexY);
            ZombieEntity->Health = ZOMBIE_NORMAL_HEALTH;
            ZombieEntity->Position.X = GameState->Camera.UnitCountX + 0.2F;
            ZombieEntity->Position.Y = GameGardenGrid_GetCellPositionY(GardenGrid, SpawnCellIndexY);
            ZombieEntity->Dimensions.X = ZOMBIE_NORMAL_DIMENSIONS_X;
            ZombieEntity->Dimensions.Y = ZOMBIE_NORMAL_DIMENSIONS_Y;
            ZombieEntity->Normal.Velocity = -ZOMBIE_NORMAL_VELOCITY;
            ZombieEntity->Normal.AttackDamage = ZOMBIE_NORMAL_ATTACK_DAMAGE;
            ZombieEntity->Normal.AttackDelay = ZOMBIE_NORMAL_ATTACK_DELAY;
        }
    }
    else
    {
        GardenGrid->SpawnNextZombieTimer += DeltaTime;
    }

    //
    // NOTE(Traian): Update zombie entities.
    //

	for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
    {
        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
        
        if (ZombieEntity->Health <= 0.0F)
        {
            // NOTE(Traian): The zombie has died and we should not run its update procedure any more.
            ZombieEntity->IsPendingDestroy = true;
            continue;
        }

        if (ZombieEntity->Position.X <= -0.2F)
        {
            // TODO(Traian): Open the game-ended screen.
            ZombieEntity->IsPendingDestroy = true;
            break;
        }

        const f32 ZombieFrontPositionX = ZombieEntity->Position.X - (0.5F * ZombieEntity->Dimensions.X);
        const s32 AttackCellIndexX = GameGardenGrid_GetCellIndexX(GardenGrid, ZombieFrontPositionX);
        plant_entity* AttackPlantEntity = NULL;
        if (0 <= AttackCellIndexX && AttackCellIndexX < GardenGrid->CellCountX)
        {
            const u32 PlantIndex = (ZombieEntity->CellIndexY * GardenGrid->CellCountX) + AttackCellIndexX;
            plant_entity* PlantEntity = GardenGrid->PlantEntities + PlantIndex;
            if (PlantEntity->Type != PLANT_TYPE_NONE)
            {
                AttackPlantEntity = PlantEntity;
            }
        }

        switch (ZombieEntity->Type)
        {
            case ZOMBIE_TYPE_NORMAL:
            {
                zombie_entity_normal* Normal = &ZombieEntity->Normal;
                
                if (AttackPlantEntity)
                {
                    if (Normal->AttackTimer >= Normal->AttackDelay)
                    {
                        // NOTE(Traian): Reset the attack timer.
                        Normal->AttackTimer = 0.0F;

                        AttackPlantEntity->Health -= Normal->AttackDamage;
                    }
                    else
                    {
                        Normal->AttackTimer += DeltaTime;
                    }
                }
                else
                {
                    // NOTE(Traian): The zombie can't attack any plant - continue walking.
                    ZombieEntity->Position.X += Normal->Velocity * DeltaTime;
                    Normal->AttackTimer = 0.0F;
                }
            }
            break;
        }
    }
}

internal void
GameGardenGrid_UpdateProjectiles(game_state* GameState, game_platform_state* PlatformState, f32 DeltaTime)
{
	game_garden_grid* GardenGrid = &GameState->GardenGrid;

    const vec2 GameMousePosition = Game_TransformNDCPointToGame(&GameState->Camera,
                                                                Vec2(PlatformState->Input->MousePositionX,
                                                                     PlatformState->Input->MousePositionY));

	for (u32 ProjectileIndex = 0; ProjectileIndex < GardenGrid->CurrentProjectileCount; ++ProjectileIndex)
    {
        projectile_entity* ProjectileEntity = GardenGrid->ProjectileEntities + ProjectileIndex;
        switch(ProjectileEntity->Type)
        {
            case PROJECTILE_TYPE_SUN:
            {
                projectile_entity_sun* Sun = &ProjectileEntity->Sun;

                if (Vec2_DistanceSquared(ProjectileEntity->Position, GameMousePosition) <= (Sun->Radius * Sun->Radius))
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
            break;

            case PROJECTILE_TYPE_PEA:
            {
                projectile_entity_pea* Pea = &ProjectileEntity->Pea;

                ProjectileEntity->Position.X += Pea->Velocity * DeltaTime;
                if ((ProjectileEntity->Position.X <= -1.0F) ||
                    (ProjectileEntity->Position.X >= GameState->Camera.UnitCountX + 1.0F))
                {
                    // NOTE(Traian): The projectile has went out of range.
                    ProjectileEntity->IsPendingDestroy = true;
                }
                else
                {
                    zombie_entity* ClosestZombieEntity = NULL;
                    f32 ClosestZombieDistance;

                    for (u32 ZombieIndex = 0; ZombieIndex < GardenGrid->CurrentZombieCount; ++ZombieIndex)
                    {
                        zombie_entity* ZombieEntity = GardenGrid->ZombieEntities + ZombieIndex;
                        if (!ZombieEntity->IsPendingDestroy && (ZombieEntity->Health > 0) &&
                            (ZombieEntity->CellIndexY == ProjectileEntity->CellIndexY))
                        {
                            const f32 SignedDistance = ZombieEntity->Position.X - ProjectileEntity->Position.X;
                            if (Abs(SignedDistance) <= (0.5F * ZombieEntity->Dimensions.X))
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

                    if (ClosestZombieEntity)
                    {
                        // NOTE(Traian): Damage the hit zombie and mark the projectile as pending destroy.
                        ClosestZombieEntity->Health -= Pea->Damage;
                        ProjectileEntity->IsPendingDestroy = true;
                    }
                }
            }
            break;
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
    
    const vec2 GARDEN_MIN_POINT_PERCENTAGE = Vec2(0.01F, 0.01F);
    const vec2 GARDEN_MAX_POINT_PERCENTAGE = Vec2(0.99F, 0.80F);
    GardenGrid->MinPoint.X = GameState->Camera.UnitCountX * GARDEN_MIN_POINT_PERCENTAGE.X;
    GardenGrid->MinPoint.Y = GameState->Camera.UnitCountY * GARDEN_MIN_POINT_PERCENTAGE.Y;
    GardenGrid->MaxPoint.X = GameState->Camera.UnitCountX * GARDEN_MAX_POINT_PERCENTAGE.X;
    GardenGrid->MaxPoint.Y = GameState->Camera.UnitCountY * GARDEN_MAX_POINT_PERCENTAGE.Y;

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

            switch (PlantEntity->Type)
            {
                case PLANT_TYPE_SUNFLOWER:
                {
                    const vec2 Dimensions = Vec2(PLANT_SUNFLOWER_DIMENSIONS_X, PLANT_SUNFLOWER_DIMENSIONS_Y);
                    const vec2 MinPoint = CellPoint - (0.5F * Dimensions) +
                                          Vec2(PLANT_SUNFLOWER_RENDER_OFFSET_X, PLANT_SUNFLOWER_RENDER_OFFSET_Y);
                    const vec2 MaxPoint = MinPoint + Dimensions;

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PLANT_SUNFLOWER);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           RenderZOffset, Color4(1.0F),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;

                case PLANT_TYPE_PEASHOOTER:
                {
                    const vec2 Dimensions = Vec2(PLANT_PEASHOOTER_DIMENSIONS_X, PLANT_PEASHOOTER_DIMENSIONS_Y);
                    const vec2 MinPoint = CellPoint - (0.5F * Dimensions) +
                                          Vec2(PLANT_PEASHOOTER_RENDER_OFFSET_X, PLANT_PEASHOOTER_RENDER_OFFSET_Y);
                    const vec2 MaxPoint = MinPoint + Dimensions;

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PLANT_PEASHOOTER);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           RenderZOffset, Color4(1.0F),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;

                case PLANT_TYPE_REPEATER:
                {
                    const vec2 Dimensions = Vec2(PLANT_REPEATER_DIMENSIONS_X,
                                                 PLANT_REPEATER_DIMENSIONS_Y);
                    const vec2 RenderOffset = Vec2(PLANT_REPEATER_RENDER_OFFSET_X,
                                                   PLANT_REPEATER_RENDER_OFFSET_Y);
                    const vec2 MinPoint = CellPoint - (0.5F * Dimensions) + RenderOffset;
                    const vec2 MaxPoint = MinPoint + Dimensions;

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PLANT_REPEATER);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           RenderZOffset, Color4(1.0F),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;
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
            switch (ZombieEntity->Type)
            {
                case ZOMBIE_TYPE_NORMAL:
                {
                    const vec2 MinPoint = ZombieEntity->Position - (0.5F * ZombieEntity->Dimensions) +
                                          Vec2(ZOMBIE_NORMAL_RENDER_OFFSET_X, ZOMBIE_NORMAL_RENDER_OFFSET_Y);
                    const vec2 MaxPoint = MinPoint + ZombieEntity->Dimensions;

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_ZOMBIE_NORMAL);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           RenderZOffset, Color4(1.0F),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;
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

        const f32 SUN_ADDITIONAL_Z_OFFSET = 0.0F;
        const f32 PEA_ADDITIONAL_Z_OFFSET = 1.0F;

        if (!ProjectileEntity->IsPendingDestroy)
        {
            switch (ProjectileEntity->Type)
            {
                case PROJECTILE_TYPE_SUN:
                {
                    const projectile_entity_sun* Sun = &ProjectileEntity->Sun;
                    const vec2 Dimensions = Vec2(2.0F * Sun->Radius * PROJECTILE_SUN_RENDER_DIMENSIONS_MULTIPLIER_X,
                                                 2.0F * Sun->Radius * PROJECTILE_SUN_RENDER_DIMENSIONS_MULTIPLIER_Y);
                    const vec2 MinPoint = ProjectileEntity->Position - (0.5F * Dimensions);
                    const vec2 MaxPoint = MinPoint + Dimensions;

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PROJECTILE_SUN);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           GARDEN_GRID_PROJECTILES_BASE_Z_OFFSET + SUN_ADDITIONAL_Z_OFFSET, Color4(1.0),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;

                case PROJECTILE_TYPE_PEA:
                {
                    const projectile_entity_pea* Pea = &ProjectileEntity->Pea;
                    const vec2 MinPoint = ProjectileEntity->Position - (0.5F * Vec2(Pea->Radius));
                    const vec2 MaxPoint = MinPoint + Vec2(Pea->Radius);

                    asset* TextureAsset = Asset_Get(&GameState->Assets, GAME_ASSET_ID_PROJECTILE_PEA);
                    Renderer_PushPrimitive(&GameState->Renderer,
                                           Game_TransformGamePointToNDC(&GameState->Camera, MinPoint),
                                           Game_TransformGamePointToNDC(&GameState->Camera, MaxPoint),
                                           GARDEN_GRID_PROJECTILES_BASE_Z_OFFSET + PEA_ADDITIONAL_Z_OFFSET, Color4(1.0),
                                           Vec2(0.0F), Vec2(1.0F),
                                           &TextureAsset->Texture.RendererTexture);
                }
                break;
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
