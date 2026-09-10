#ifndef _NIS_H_
#define _NIS_H_

#include "NL/nlMath.h"
#include "NL/nlList.h"
#include "NL/nlSlotPool.h"
#include "Game/SAnim/pnSAnimController.h"

class cAnimCamera;
struct SFXEmitter;

enum NisTriggerType
{
    NIS_TRIGGER_TYPE_EFFECT = 0,
    NIS_TRIGGER_TYPE_TIME_DILATION = 1,
    NIS_TRIGGER_TYPE_PLAY_SOUND = 2,
    NIS_TRIGGER_TYPE_PLAY_RANDOM_DIALOGUE = 3,
    NIS_TRIGGER_TYPE_STOP_SOUND = 4,
    NIS_TRIGGER_TYPE_PLAY_STREAM = 5,
    NIS_TRIGGER_TYPE_STOP_STREAM = 6,
    NIS_TRIGGER_TYPE_STOP_ALL_STREAMS = 7,
    NIS_TRIGGER_TYPE_SET_ACTIVE_STREAM_LOOPING = 8,
    NIS_TRIGGER_TYPE_REGISTER_GOAL_AUDIO = 9,
    NIS_TRIGGER_TYPE_RAISE_EVENT = 10,
};

enum NisCharacterClass
{
    NIS_CHAR_CLASS_INVALID = -1,
    NIS_CHAR_CLASS_BIRDO = 0,
    NIS_CHAR_CLASS_DAISY = 1,
    NIS_CHAR_CLASS_DONKEYKONG = 2,
    NIS_CHAR_CLASS_HAMMERBROS = 3,
    NIS_CHAR_CLASS_KOOPA = 4,
    NIS_CHAR_CLASS_LUIGI = 5,
    NIS_CHAR_CLASS_MARIO = 6,
    NIS_CHAR_CLASS_PEACH = 7,
    NIS_CHAR_CLASS_TOAD = 8,
    NIS_CHAR_CLASS_WALUIGI = 9,
    NIS_CHAR_CLASS_WARIO = 10,
    NIS_CHAR_CLASS_YOSHI = 11,
    NIS_CHAR_CLASS_MYSTERY = 12,
    NIS_CHAR_CLASS_HOME_GOALIE = 13,
    NIS_CHAR_CLASS_AWAY_GOALIE = 14,
    NIS_CHAR_CLASS_NUM_CLASSES = 15,
};

enum NisTarget
{
    NIS_TARGET_NONE = 0,
    NIS_TARGET_STADIUM = 1,
    NIS_TARGET_HOME_CAPTAIN = 2,
    NIS_TARGET_AWAY_CAPTAIN = 3,
    NIS_TARGET_HOME_SIDEKICK = 4,
    NIS_TARGET_AWAY_SIDEKICK = 5,
    NIS_TARGET_HOME_GOALIE = 6,
    NIS_TARGET_AWAY_GOALIE = 7,
    NIS_TARGET_LOSER_CAPTAIN = 8,
    NIS_TARGET_WINNER_CAPTAIN = 9,
    NIS_TARGET_LOSER_SIDEKICK = 10,
    NIS_TARGET_WINNER_SIDEKICK = 11,
    NIS_TARGET_LOSER_GOALIE = 12,
    NIS_TARGET_WINNER_GOALIE = 13,
    NIS_NUM_TARGETS = 14,
};

enum NisWinnerType
{
    NIS_GAME_WINNER = 0,
    NIS_GOAL_WINNER = 1,
    NIS_NUM_WINNER_TYPES = 2,
    NIS_DO_NOT_CARE = 3,
};

struct NisHeader
{
    /* 0x00 */ char name[64];
    /* 0x40 */ int size;
    /* 0x44 */ int numBalls;
    /* 0x48 */ int numAnimations;
    /* 0x4C */ int numCameras;
    /* 0x50 */ nlVector3 center;
    /* 0x5C */ nlVector3 minBounds;
    /* 0x68 */ nlVector3 maxBounds;
    /* 0x74 */ nlVector3 beginPositions[4];
    /* 0xA4 */ NisTarget target;
    /* 0xA8 */ NisWinnerType winnerType;
    /* 0xAC */ nlVector3 stadiumOffset;
    /* 0xB8 */ float mTime;
}; // total size: 0xBC (0xB8 on G4QP01)

enum NisAudioType
{
    NIS_AUDIO_TYPE_NONE = 0,
    NIS_AUDIO_TYPE_STREAM = 1,
    NIS_AUDIO_TYPE_SFX = 2,
};

class Nis
{
public:
    static const int MAX_NUM_TRIGGERS = 48;
    static const int MAX_NUM_CHARACTERS = 10;

    struct NisAudioData
    {
        static const int MAX_NIS_AUDIO_STR_CHARS = 128;
        static inline NisAudioData* Allocate();

        /* 0x00 */ NisAudioType audioType;
        union
        {
            SFXEmitter* pEmitter;
            uintptr_t index;
        }
        /* 0x04 */ identifier;
        /* 0x08 */ unsigned long soundType;
        /* 0x0C */ char str[MAX_NIS_AUDIO_STR_CHARS];
        /* 0x8C */ bool isEmitter;
        /* 0x8D */ bool stopAtNisEnd;
        /* 0x90 */ NisAudioData* next;
    }; // total size: 0x94

    struct TriggerParams
    {
        /* 0x00 */ float float1;
        /* 0x04 */ unsigned long param1;
        /* 0x08 */ unsigned long param2;
        /* 0x0C */ unsigned long param3;
        /* 0x10 */ unsigned long param4;
    }; // total size: 0x14

    struct Trigger
    {
        /* 0x00 */ NisTriggerType type;
        /* 0x04 */ float frameNumber;
        /* 0x08 */ const char* name;
        /* 0x0C */ const char* target;
        /* 0x10 */ TriggerParams params;

        void FireEffect(const Nis& nis) const;
        void Fire(Nis& nis) const;
    }; // total size: 0x24

    Nis(NisHeader& header, char* data, int size);
    ~Nis();
    char* Name() const;
    void Update(float dt);
    void UpdateTriggers(float oldTime, float newTime, float duration);
    void SelectCamera(cAnimCamera& camera, int cameraIndex);
    bool SelectRandomCamera(cAnimCamera& camera);
    void Render();
    nlVector3 Offset() const;
    void AddTrigger(NisTriggerType triggerType, float frameNumber, const char* name, const char* target, Nis::TriggerParams* trigParams);
    void StopAllOutstandingNisAudio();
    NisAudioData* RemoveNisAudioData(NisAudioData* pNisAudioData);
    NisAudioData* StopNisAudio(NisAudioData* pNisAudioData, bool bNisEndedNormally);
    void StopNisAudio(NisAudioType type, const char* str);
    void AddNisAudioData(NisAudioType type, uintptr_t index, const char* str, bool isEmitter, bool stopAtNisEnd, unsigned long soundType);

    /* 0x000 */ NisHeader* mHeader;                                             // offset 0x0, size 0x4
    /* 0x004 */ NisTarget mTarget;                                              // offset 0x4, size 0x4
    /* 0x008 */ NisWinnerType mWinnerType;                                      // offset 0x8, size 0x4
    /* 0x00C */ char* mData;                                                    // offset 0xC, size 0x4
    /* 0x010 */ int mSize;                                                      // offset 0x10, size 0x4
    /* 0x014 */ int mBallId[MAX_NUM_CHARACTERS];                                // offset 0x14, size 0x28
    /* 0x03C */ cPN_SAnimController* mCharacterControllers[MAX_NUM_CHARACTERS]; // offset 0x3C, size 0x28
    /* 0x064 */ bool mMirrored;                                                 // offset 0x64, size 0x1
    /* 0x068 */ cAnimCamera* mCamera;                                           // offset 0x68, size 0x4
    /* 0x06C */ int mNumCameras;                                                // offset 0x6C, size 0x4
    /* 0x070 */ int mNumTriggers;                                               // offset 0x70, size 0x4
    /* 0x074 */ Trigger mTriggers[MAX_NUM_TRIGGERS];                            // offset 0x74, size 0x6C0
    /* 0x734 */ int mMainCharacterIndex;                                        // offset 0x734, size 0x4
    /* 0x738 */ int mAudioCharacterIndex;                                       // offset 0x738, size 0x4
    /* 0x73C */ NisAudioData* mNisAudioDataList;                                // offset 0x73C, size 0x4
}; // total size: 0x740

#endif // _NIS_H_
