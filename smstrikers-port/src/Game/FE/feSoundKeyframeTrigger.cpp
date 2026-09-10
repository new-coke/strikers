#include "Game/FE/feSoundKeyframeTrigger.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/feAnimation.h"
#include "Game/FE/tlInstance.h"

#include "NL/nlDLRing.h"

inline fAnimationKeyframe* GetAnimationKeyframe(fAnimationKeyframe* start, float t)
{
    fAnimationKeyframe* it = start;
    do
    {
        if (t <= it->pKeyFrameData.m_fTime)
        {
            return it;
        }
        it = it->m_next;
    } while (it != start);
    return NULL;
}

/**
 * Offset/Address/Size: 0x0 | 0x8010EF64 | size: 0x1A8
 */
void SoundKeyframeTrigger::Update(float previoustime, float currenttime)
{
    fAnimationKeyframe* iter;
    FEAnimation* animhead;
    FEAnimation* animnode;
    fAnimationKeyframe* startkey;

    if (previoustime == currenttime)
        return;

    animhead = m_slide->m_animations;
    animnode = nlDLRingGetStart(animhead);

    for (;;)
    {
        if (animnode == NULL)
            break;

        if (animnode->m_cast_type == 0 && animnode->m_type == eAnimOpacity)
        {
            fAnimationKeyframe* keyframehead = nlDLRingGetStart((fAnimationKeyframe*)animnode->m_DLRingHead);
            fAnimationKeyframe* testkey = keyframehead;

            for (;;)
            {
                if (previoustime <= testkey->pKeyFrameData.m_fTime)
                {
                    startkey = testkey;
                    break;
                }
                testkey = testkey->m_next;
                if (testkey == keyframehead)
                {
                    startkey = NULL;
                    break;
                }
            }

            f32 threshold = 255.0f;
            iter = startkey;
            while (startkey != NULL)
            {
                if (previoustime > currenttime && currenttime > iter->pKeyFrameData.m_fTime)
                {
                    previoustime = 0.0f;
                }

                if (!(previoustime <= iter->pKeyFrameData.m_fTime))
                    break;
                if (!(currenttime >= iter->pKeyFrameData.m_fTime))
                    break;

                if (iter->pKeyFrameData.m_fPoint >= threshold)
                {
                    char* name;
                    TLInstance* instance;

                    instance = animnode->m_pTLInstanceTarget;
                    name = instance->m_szName;
                    if (name != NULL)
                    {
                        if (name[0] == 's' && name[1] == 'f' && name[2] == 'x')
                        {
                            instance->m_bVisible = false;
                            FEAudio::PlayAnimAudioEvent(instance->m_hash, false);
                        }
                    }
                }

                iter = iter->m_next;
                if (iter == startkey)
                    break;
            }
        }

        if (nlDLRingIsEnd(animhead, animnode))
            break;
        animnode = animnode->m_next;
    }
}
