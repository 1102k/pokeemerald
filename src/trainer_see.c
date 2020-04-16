#include "global.h"
#include "battle_setup.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "field_player_avatar.h"
#include "pokemon.h"
#include "script.h"
#include "script_movement.h"
#include "sprite.h"
#include "task.h"
#include "trainer_see.h"
#include "trainer_hill.h"
#include "util.h"
#include "battle_pyramid.h"
#include "constants/battle_setup.h"
#include "constants/event_objects.h"
#include "constants/event_object_movement.h"
#include "constants/field_effects.h"

// this file's functions
static u8 CheckTrainer(u8 objectEventId);
static u8 GetTrainerApproachDistance(struct ObjectEvent *trainerObj);
static u8 CheckPathBetweenTrainerAndPlayer(struct ObjectEvent *trainerObj, u8 approachDistance, u8 direction);
static void TrainerApproachPlayer(struct ObjectEvent *trainerObj, u8 range);
static void Task_RunTrainerSeeFuncList(u8 taskId);
static void Task_DestroyTrainerApproachTask(u8 taskId);
static void SetIconSpriteData(struct Sprite *sprite, u16 fldEffId, u8 spriteAnimNum);

static u8 GetTrainerApproachDistanceSouth(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y);
static u8 GetTrainerApproachDistanceNorth(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y);
static u8 GetTrainerApproachDistanceWest(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y);
static u8 GetTrainerApproachDistanceEast(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y);

static bool8 sub_80B4178(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 TrainerExclamationMark(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 WaitTrainerExclamationMark(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 TrainerMoveToPlayer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 PlayerFaceApproachingTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 WaitPlayerFaceApproachingTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 RevealDisguisedTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 WaitRevealDisguisedTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 RevealHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 PopOutOfAshHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 JumpInPlaceHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);
static bool8 WaitRevealHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj);

static void SpriteCB_TrainerIcons(struct Sprite *sprite);

// IWRAM common
u16 gWhichTrainerToFaceAfterBattle;
u8 gPostBattleMovementScript[4];
struct ApproachingTrainer gApproachingTrainers[2];
u8 gNoOfApproachingTrainers;
bool8 gTrainerApproachedPlayer;

// EWRAM
EWRAM_DATA u8 gApproachingTrainerId = 0;

// const rom data
static const u8 sEmotion_ExclamationMarkGfx[] = INCBIN_U8("graphics/misc/emotion_exclamation.4bpp");
static const u8 sEmotion_QuestionMarkGfx[] = INCBIN_U8("graphics/misc/emotion_question.4bpp");
static const u8 sEmotion_HeartGfx[] = INCBIN_U8("graphics/misc/emotion_heart.4bpp");

static u8 (*const sDirectionalApproachDistanceFuncs[])(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y) =
{
    GetTrainerApproachDistanceSouth,
    GetTrainerApproachDistanceNorth,
    GetTrainerApproachDistanceWest,
    GetTrainerApproachDistanceEast,
};

static bool8 (*const sTrainerSeeFuncList[])(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj) =
{
    sub_80B4178,
    TrainerExclamationMark,
    WaitTrainerExclamationMark,
    TrainerMoveToPlayer,
    PlayerFaceApproachingTrainer,
    WaitPlayerFaceApproachingTrainer,
    RevealDisguisedTrainer,
    WaitRevealDisguisedTrainer,
    RevealHiddenTrainer,
    PopOutOfAshHiddenTrainer,
    JumpInPlaceHiddenTrainer,
    WaitRevealHiddenTrainer,
};

static bool8 (*const sTrainerSeeFuncList2[])(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj) =
{
    RevealHiddenTrainer,
    PopOutOfAshHiddenTrainer,
    JumpInPlaceHiddenTrainer,
    WaitRevealHiddenTrainer,
};

static const struct OamData sOamData_Icons =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteFrameImage sSpriteImageTable_ExclamationQuestionMark[] =
{
    {
        .data = sEmotion_ExclamationMarkGfx,
        .size = 0x80
    },
    {
        .data = sEmotion_QuestionMarkGfx,
        .size = 0x80
    }
};

static const struct SpriteFrameImage sSpriteImageTable_HeartIcon[] =
{
    {
        .data = sEmotion_HeartGfx,
        .size = 0x80
    }
};

static const union AnimCmd sSpriteAnim_Icons1[] =
{
    ANIMCMD_FRAME(0, 60),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_Icons2[] =
{
    ANIMCMD_FRAME(1, 60),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_Icons[] =
{
    sSpriteAnim_Icons1,
    sSpriteAnim_Icons2
};

static const struct SpriteTemplate sSpriteTemplate_ExclamationQuestionMark =
{
    .tileTag = 0xffff,
    .paletteTag = 0xffff,
    .oam = &sOamData_Icons,
    .anims = sSpriteAnimTable_Icons,
    .images = sSpriteImageTable_ExclamationQuestionMark,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_TrainerIcons
};

static const struct SpriteTemplate sSpriteTemplate_HeartIcon =
{
    .tileTag = 0xffff,
    .paletteTag = 0x1004,
    .oam = &sOamData_Icons,
    .anims = sSpriteAnimTable_Icons,
    .images = sSpriteImageTable_HeartIcon,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_TrainerIcons
};

// code
bool8 CheckForTrainersWantingBattle(void)
{
    u8 i;

    gNoOfApproachingTrainers = 0;
    gApproachingTrainerId = 0;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        u8 retVal;

        if (!gObjectEvents[i].active)
            continue;
        if (gObjectEvents[i].trainerType != 1 && gObjectEvents[i].trainerType != 3)
            continue;

        retVal = CheckTrainer(i);
        if (retVal == 2)
            break; // two trainers have been found

        if (retVal == 0) // no trainers
            continue;

        if (gNoOfApproachingTrainers > 1)
            break;
        if (GetMonsStateToDoubles_2() != 0) // one trainer found and cant have a double battle
            break;
    }

    if (gNoOfApproachingTrainers == 1)
    {
        ResetTrainerOpponentIds();
        ConfigureAndSetUpOneTrainerBattle(gApproachingTrainers[gNoOfApproachingTrainers - 1].objectEventId,
                                          gApproachingTrainers[gNoOfApproachingTrainers - 1].trainerScriptPtr);
        gTrainerApproachedPlayer = TRUE;
        return TRUE;
    }
    else if (gNoOfApproachingTrainers == 2)
    {
        ResetTrainerOpponentIds();
        for (i = 0; i < gNoOfApproachingTrainers; i++, gApproachingTrainerId++)
        {
            ConfigureTwoTrainersBattle(gApproachingTrainers[i].objectEventId,
                                       gApproachingTrainers[i].trainerScriptPtr);
        }
        SetUpTwoTrainersBattle();
        gApproachingTrainerId = 0;
        gTrainerApproachedPlayer = TRUE;
        return TRUE;
    }
    else
    {
        gTrainerApproachedPlayer = FALSE;
        return FALSE;
    }
}

static u8 CheckTrainer(u8 objectEventId)
{
    const u8 *scriptPtr;
    u8 ret = 1;
    u8 approachDistance;

    if (InTrainerHill() == TRUE)
        scriptPtr = GetTrainerHillTrainerScript();
    else
        scriptPtr = GetObjectEventScriptPointerByObjectEventId(objectEventId);

    if (InBattlePyramid())
    {
        if (GetBattlePyramidTrainerFlag(objectEventId))
            return 0;
    }
    else if (InTrainerHill() == TRUE)
    {
        if (GetHillTrainerFlag(objectEventId))
            return 0;
    }
    else
    {
        if (GetTrainerFlagFromScriptPointer(scriptPtr))
            return 0;
    }

    approachDistance = GetTrainerApproachDistance(&gObjectEvents[objectEventId]);

    if (approachDistance != 0)
    {
        if (scriptPtr[1] == TRAINER_BATTLE_DOUBLE
            || scriptPtr[1] == TRAINER_BATTLE_REMATCH_DOUBLE
            || scriptPtr[1] == TRAINER_BATTLE_CONTINUE_SCRIPT_DOUBLE)
        {
            if (GetMonsStateToDoubles_2() != 0)
                return 0;

            ret = 2;
        }

        gApproachingTrainers[gNoOfApproachingTrainers].objectEventId = objectEventId;
        gApproachingTrainers[gNoOfApproachingTrainers].trainerScriptPtr = scriptPtr;
        gApproachingTrainers[gNoOfApproachingTrainers].radius = approachDistance;
        TrainerApproachPlayer(&gObjectEvents[objectEventId], approachDistance - 1);
        gNoOfApproachingTrainers++;

        return ret;
    }

    return 0;
}

static u8 GetTrainerApproachDistance(struct ObjectEvent *trainerObj)
{
    s16 x, y;
    u8 i;
    u8 approachDistance;

    PlayerGetDestCoords(&x, &y);
    if (trainerObj->trainerType == 1)  // can only see in one direction
    {
        approachDistance = sDirectionalApproachDistanceFuncs[trainerObj->facingDirection - 1](trainerObj, trainerObj->trainerRange_berryTreeId, x, y);
        return CheckPathBetweenTrainerAndPlayer(trainerObj, approachDistance, trainerObj->facingDirection);
    }
    else  // can see in all directions
    {
        for (i = 0; i < 4; i++)
        {
            approachDistance = sDirectionalApproachDistanceFuncs[i](trainerObj, trainerObj->trainerRange_berryTreeId, x, y);
            if (CheckPathBetweenTrainerAndPlayer(trainerObj, approachDistance, i + 1)) // directions are 1-4 instead of 0-3. south north west east
                return approachDistance;
        }
    }

    return 0;
}

// Returns how far south the player is from trainer. 0 if out of trainer's sight.
static u8 GetTrainerApproachDistanceSouth(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y)
{
    if (trainerObj->currentCoords.x == x
     && y > trainerObj->currentCoords.y
     && y <= trainerObj->currentCoords.y + range)
        return (y - trainerObj->currentCoords.y);
    else
        return 0;
}

// Returns how far north the player is from trainer. 0 if out of trainer's sight.
static u8 GetTrainerApproachDistanceNorth(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y)
{
    if (trainerObj->currentCoords.x == x
     && y < trainerObj->currentCoords.y
     && y >= trainerObj->currentCoords.y - range)
        return (trainerObj->currentCoords.y - y);
    else
        return 0;
}

// Returns how far west the player is from trainer. 0 if out of trainer's sight.
static u8 GetTrainerApproachDistanceWest(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y)
{
    if (trainerObj->currentCoords.y == y
     && x < trainerObj->currentCoords.x
     && x >= trainerObj->currentCoords.x - range)
        return (trainerObj->currentCoords.x - x);
    else
        return 0;
}

// Returns how far east the player is from trainer. 0 if out of trainer's sight.
static u8 GetTrainerApproachDistanceEast(struct ObjectEvent *trainerObj, s16 range, s16 x, s16 y)
{
    if (trainerObj->currentCoords.y == y
     && x > trainerObj->currentCoords.x
     && x <= trainerObj->currentCoords.x + range)
        return (x - trainerObj->currentCoords.x);
    else
        return 0;
}

#define COLLISION_MASK (~1)

static u8 CheckPathBetweenTrainerAndPlayer(struct ObjectEvent *trainerObj, u8 approachDistance, u8 direction)
{
    s16 x, y;
    u8 unk19_temp;
    u8 unk19b_temp;
    u8 i;
    u8 collision;

    if (approachDistance == 0)
        return 0;

    x = trainerObj->currentCoords.x;
    y = trainerObj->currentCoords.y;

    MoveCoords(direction, &x, &y);
    for (i = 0; i < approachDistance - 1; i++, MoveCoords(direction, &x, &y))
    {
        collision = GetCollisionFlagsAtCoords(trainerObj, x, y, direction);
        if (collision != 0 && (collision & COLLISION_MASK))
            return 0;
    }

    // preserve mapobj_unk_19 before clearing.
    unk19_temp = trainerObj->range.as_nybbles.x;
    unk19b_temp = trainerObj->range.as_nybbles.y;
    trainerObj->range.as_nybbles.x = 0;
    trainerObj->range.as_nybbles.y = 0;

    collision = GetCollisionAtCoords(trainerObj, x, y, direction);

    trainerObj->range.as_nybbles.x = unk19_temp;
    trainerObj->range.as_nybbles.y = unk19b_temp;
    if (collision == 4)
        return approachDistance;

    return 0;
}

#define tFuncId             data[0]
#define tTrainerRange       data[3]
#define tOutOfAshSpriteId   data[4]
#define tTrainerObjectEventId data[7]

static void TrainerApproachPlayer(struct ObjectEvent *trainerObj, u8 range)
{
    struct Task *task;

    gApproachingTrainers[gNoOfApproachingTrainers].taskId = CreateTask(Task_RunTrainerSeeFuncList, 0x50);
    task = &gTasks[gApproachingTrainers[gNoOfApproachingTrainers].taskId];
    task->tTrainerRange = range;
    task->tTrainerObjectEventId = gApproachingTrainers[gNoOfApproachingTrainers].objectEventId;
}

static void sub_80B40C8(TaskFunc followupFunc)
{
    u8 taskId;
    TaskFunc taskFunc;

    if (gApproachingTrainerId == 0)
        taskId = gApproachingTrainers[0].taskId;
    else
        taskId = gApproachingTrainers[1].taskId;

    taskFunc = Task_RunTrainerSeeFuncList;
    SetTaskFuncWithFollowupFunc(taskId, taskFunc, followupFunc);
    gTasks[taskId].tFuncId = 1;
    taskFunc(taskId);
}

static void Task_RunTrainerSeeFuncList(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    struct ObjectEvent *trainerObj = &gObjectEvents[task->tTrainerObjectEventId];

    if (!trainerObj->active)
    {
        SwitchTaskToFollowupFunc(taskId);
    }
    else
    {
        while (sTrainerSeeFuncList[task->tFuncId](taskId, task, trainerObj));
    }
}

static bool8 sub_80B4178(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    return FALSE;
}

static bool8 TrainerExclamationMark(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    u8 direction;

    ObjectEventGetLocalIdAndMap(trainerObj, &gFieldEffectArguments[0], &gFieldEffectArguments[1], &gFieldEffectArguments[2]);
    FieldEffectStart(FLDEFF_EXCLAMATION_MARK_ICON);
    direction = GetFaceDirectionMovementAction(trainerObj->facingDirection);
    ObjectEventSetHeldMovement(trainerObj, direction);
    task->tFuncId++;
    return TRUE;
}

static bool8 WaitTrainerExclamationMark(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (FieldEffectActiveListContains(FLDEFF_EXCLAMATION_MARK_ICON))
    {
        return FALSE;
    }
    else
    {
        task->tFuncId++;
        if (trainerObj->movementType == MOVEMENT_TYPE_TREE_DISGUISE || trainerObj->movementType == MOVEMENT_TYPE_MOUNTAIN_DISGUISE)
            task->tFuncId = 6;
        if (trainerObj->movementType == MOVEMENT_TYPE_HIDDEN)
            task->tFuncId = 8;
        return TRUE;
    }
}

static bool8 TrainerMoveToPlayer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (!ObjectEventIsMovementOverridden(trainerObj) || ObjectEventClearHeldMovementIfFinished(trainerObj))
    {
        if (task->tTrainerRange)
        {
            ObjectEventSetHeldMovement(trainerObj, GetWalkNormalMovementAction(trainerObj->facingDirection));
            task->tTrainerRange--;
        }
        else
        {
            ObjectEventSetHeldMovement(trainerObj, MOVEMENT_ACTION_FACE_PLAYER);
            task->tFuncId++;
        }
    }
    return FALSE;
}

static bool8 PlayerFaceApproachingTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    struct ObjectEvent *playerObj;

    if (ObjectEventIsMovementOverridden(trainerObj) && !ObjectEventClearHeldMovementIfFinished(trainerObj))
        return FALSE;

    SetTrainerMovementType(trainerObj, GetTrainerFacingDirectionMovementType(trainerObj->facingDirection));
    TryOverrideTemplateCoordsForObjectEvent(trainerObj, GetTrainerFacingDirectionMovementType(trainerObj->facingDirection));
    OverrideTemplateCoordsForObjectEvent(trainerObj);

    playerObj = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (ObjectEventIsMovementOverridden(playerObj) && !ObjectEventClearHeldMovementIfFinished(playerObj))
        return FALSE;

    sub_808BCE8();
    ObjectEventSetHeldMovement(&gObjectEvents[gPlayerAvatar.objectEventId], GetFaceDirectionMovementAction(GetOppositeDirection(trainerObj->facingDirection)));
    task->tFuncId++;
    return FALSE;
}

static bool8 WaitPlayerFaceApproachingTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    struct ObjectEvent *playerObj = &gObjectEvents[gPlayerAvatar.objectEventId];

    if (!ObjectEventIsMovementOverridden(playerObj)
     || ObjectEventClearHeldMovementIfFinished(playerObj))
        SwitchTaskToFollowupFunc(taskId);
    return FALSE;
}

static bool8 RevealDisguisedTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (!ObjectEventIsMovementOverridden(trainerObj)
     || ObjectEventClearHeldMovementIfFinished(trainerObj))
    {
        ObjectEventSetHeldMovement(trainerObj, MOVEMENT_ACTION_REVEAL_TRAINER);
        task->tFuncId++;
    }
    return FALSE;
}

static bool8 WaitRevealDisguisedTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (ObjectEventClearHeldMovementIfFinished(trainerObj))
        task->tFuncId = 3;

    return FALSE;
}

static bool8 RevealHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (!ObjectEventIsMovementOverridden(trainerObj)
     || ObjectEventClearHeldMovementIfFinished(trainerObj))
    {
        ObjectEventSetHeldMovement(trainerObj, MOVEMENT_ACTION_FACE_PLAYER);
        task->tFuncId++;
    }
    return FALSE;
}

static bool8 PopOutOfAshHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (ObjectEventCheckHeldMovementStatus(trainerObj))
    {
        gFieldEffectArguments[0] = trainerObj->currentCoords.x;
        gFieldEffectArguments[1] = trainerObj->currentCoords.y;
        gFieldEffectArguments[2] = gSprites[trainerObj->spriteId].subpriority - 1;
        gFieldEffectArguments[3] = 2;
        task->tOutOfAshSpriteId = FieldEffectStart(FLDEFF_POP_OUT_OF_ASH);
        task->tFuncId++;
    }
    return FALSE;
}

static bool8 JumpInPlaceHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    struct Sprite *sprite;

    if (gSprites[task->tOutOfAshSpriteId].animCmdIndex == 2)
    {
        trainerObj->fixedPriority = 0;
        trainerObj->triggerGroundEffectsOnMove = 1;

        sprite = &gSprites[trainerObj->spriteId];
        sprite->oam.priority = 2;
        ObjectEventClearHeldMovementIfFinished(trainerObj);
        ObjectEventSetHeldMovement(trainerObj, GetJumpInPlaceMovementAction(trainerObj->facingDirection));
        task->tFuncId++;
    }

    return FALSE;
}

static bool8 WaitRevealHiddenTrainer(u8 taskId, struct Task *task, struct ObjectEvent *trainerObj)
{
    if (!FieldEffectActiveListContains(FLDEFF_POP_OUT_OF_ASH))
        task->tFuncId = 3;

    return FALSE;
}

#undef tFuncId
#undef tTrainerRange
#undef tOutOfAshSpriteId
#undef tTrainerObjectEventId

static void sub_80B44C8(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    struct ObjectEvent *objEvent;

    // another objEvent loaded into by loadword?
    LoadWordFromTwoHalfwords(&task->data[1], (u32 *)&objEvent);
    if (!task->data[7])
    {
        ObjectEventClearHeldMovement(objEvent);
        task->data[7]++;
    }
    sTrainerSeeFuncList2[task->data[0]](taskId, task, objEvent);
    if (task->data[0] == 3 && !FieldEffectActiveListContains(FLDEFF_POP_OUT_OF_ASH))
    {
        SetTrainerMovementType(objEvent, GetTrainerFacingDirectionMovementType(objEvent->facingDirection));
        TryOverrideTemplateCoordsForObjectEvent(objEvent, GetTrainerFacingDirectionMovementType(objEvent->facingDirection));
        DestroyTask(taskId);
    }
    else
    {
        objEvent->heldMovementFinished = 0;
    }
}

void sub_80B4578(struct ObjectEvent *var)
{
    StoreWordInTwoHalfwords(&gTasks[CreateTask(sub_80B44C8, 0)].data[1], (u32)var);
}

void EndTrainerApproach(void)
{
    sub_80B40C8(Task_DestroyTrainerApproachTask);
}

static void Task_DestroyTrainerApproachTask(u8 taskId)
{
    DestroyTask(taskId);
    EnableBothScriptContexts();
}

void TryPrepareSecondApproachingTrainer(void)
{
    if (gNoOfApproachingTrainers == 2)
    {
        if (gApproachingTrainerId == 0)
        {
            gApproachingTrainerId++;
            gSpecialVar_Result = TRUE;
            UnfreezeObjectEvents();
            FreezeObjectEventsExceptOne(gApproachingTrainers[1].objectEventId);
        }
        else
        {
            gApproachingTrainerId = 0;
            gSpecialVar_Result = FALSE;
        }
    }
    else
    {
        gSpecialVar_Result = FALSE;
    }
}

#define sLocalId    data[0]
#define sMapNum     data[1]
#define sMapGroup   data[2]
#define sData3      data[3]
#define sData4      data[4]
#define sFldEffId   data[7]

u8 FldEff_ExclamationMarkIcon(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSpriteTemplate_ExclamationQuestionMark, 0, 0, 0x53);

    if (spriteId != MAX_SPRITES)
        SetIconSpriteData(&gSprites[spriteId], FLDEFF_EXCLAMATION_MARK_ICON, 0);

    return 0;
}

u8 FldEff_QuestionMarkIcon(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSpriteTemplate_ExclamationQuestionMark, 0, 0, 0x52);

    if (spriteId != MAX_SPRITES)
        SetIconSpriteData(&gSprites[spriteId], FLDEFF_QUESTION_MARK_ICON, 1);

    return 0;
}

u8 FldEff_HeartIcon(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSpriteTemplate_HeartIcon, 0, 0, 0x52);

    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];

        SetIconSpriteData(sprite, FLDEFF_HEART_ICON, 0);
        sprite->oam.paletteNum = 2;
    }

    return 0;
}

static void SetIconSpriteData(struct Sprite *sprite, u16 fldEffId, u8 spriteAnimNum)
{
    sprite->oam.priority = 1;
    sprite->coordOffsetEnabled = 1;

    sprite->sLocalId = gFieldEffectArguments[0];
    sprite->sMapNum = gFieldEffectArguments[1];
    sprite->sMapGroup = gFieldEffectArguments[2];
    sprite->sData3 = -5;
    sprite->sFldEffId = fldEffId;

    StartSpriteAnim(sprite, spriteAnimNum);
}

static void SpriteCB_TrainerIcons(struct Sprite *sprite)
{
    u8 objEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objEventId)
     || sprite->animEnded)
    {
        FieldEffectStop(sprite, sprite->sFldEffId);
    }
    else
    {
        struct Sprite *objEventSprite = &gSprites[gObjectEvents[objEventId].spriteId];
        sprite->sData4 += sprite->sData3;
        sprite->pos1.x = objEventSprite->pos1.x;
        sprite->pos1.y = objEventSprite->pos1.y - 16;
        sprite->pos2.x = objEventSprite->pos2.x;
        sprite->pos2.y = objEventSprite->pos2.y + sprite->sData4;
        if (sprite->sData4)
            sprite->sData3++;
        else
            sprite->sData3 = 0;
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sData3
#undef sData4
#undef sFldEffId

u8 GetCurrentApproachingTrainerObjectEventId(void)
{
    if (gApproachingTrainerId == 0)
        return gApproachingTrainers[0].objectEventId;
    else
        return gApproachingTrainers[1].objectEventId;
}

u8 GetChosenApproachingTrainerObjectEventId(u8 arrayId)
{
    if (arrayId >= ARRAY_COUNT(gApproachingTrainers))
        return 0;
    else if (arrayId == 0)
        return gApproachingTrainers[0].objectEventId;
    else
        return gApproachingTrainers[1].objectEventId;
}

void PlayerFaceTrainerAfterBattle(void)
{
    struct ObjectEvent *objEvent;

    if (gTrainerApproachedPlayer == TRUE)
    {
        objEvent = &gObjectEvents[gApproachingTrainers[gWhichTrainerToFaceAfterBattle].objectEventId];
        gPostBattleMovementScript[0] = GetFaceDirectionMovementAction(GetOppositeDirection(objEvent->facingDirection));
        gPostBattleMovementScript[1] = MOVEMENT_ACTION_STEP_END;
        ScriptMovement_StartObjectMovementScript(OBJ_EVENT_ID_PLAYER, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup, gPostBattleMovementScript);
    }
    else
    {
        objEvent = &gObjectEvents[gPlayerAvatar.objectEventId];
        gPostBattleMovementScript[0] = GetFaceDirectionMovementAction(objEvent->facingDirection);
        gPostBattleMovementScript[1] = MOVEMENT_ACTION_STEP_END;
        ScriptMovement_StartObjectMovementScript(OBJ_EVENT_ID_PLAYER, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup, gPostBattleMovementScript);
    }

    SetMovingNpcId(OBJ_EVENT_ID_PLAYER);
}
