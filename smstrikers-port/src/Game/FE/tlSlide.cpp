#include "Game/FE/tlSlide.h"
#include "Game/FE/tlComponentInstance.h"

#include "NL/nlDLRing.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020FBE0 | size: 0xD8
 */
void TLSlide::Update(float time)
{
    FEAnimation* anim;
    TLComponentInstance* var_r31_2;

    anim = nlDLRingGetStart<FEAnimation>(this->m_animations);
    for (;;)
    {
        if (anim == NULL)
        {
            break;
        }
        anim->Update(m_time);
        if (nlDLRingIsEnd<FEAnimation>(this->m_animations, anim) != 0)
        {
            break;
        }
        anim = anim->m_next;
    }

    var_r31_2 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(this->m_instances);
    for (;;)
    {
        if (var_r31_2 == NULL)
        {
            break;
        }
        if (var_r31_2->m_type == TLAT_COMPONENT)
        {
            var_r31_2->Update(time);
        }
        UpdateAsset(var_r31_2, time);
        if (nlDLRingIsEnd<TLInstance>(this->m_instances, var_r31_2) != 0)
        {
            break;
        }
        var_r31_2 = (TLComponentInstance*)var_r31_2->m_next;
    }
}

/**
 * Offset/Address/Size: 0xD8 | 0x8020FCB8 | size: 0x290
 */
void TLSlide::UpdateAsset(TLInstance* instance, float time)
{
    TLComponentInstance* var_r28;
    TLComponentInstance* var_r27;
    TLComponentInstance* var_r26;
    TLComponentInstance* var_r25;
    TLComponentInstance* var_r24;
    TLComponentInstance* var_r23;
    TLInstance* var_r22;

    if (instance->pChildren == NULL)
    {
        return;
    }

    TLComponentInstance* var_r31 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(instance->pChildren);
    for (;;)
    {
        if (var_r31->m_type == TLAT_COMPONENT)
        {
            var_r31->Update(time);
        }

        if (var_r31->pChildren != NULL)
        {
            var_r28 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r31->pChildren);
            for (;;)
            {
                if (var_r28->m_type == TLAT_COMPONENT)
                {
                    var_r28->Update(time);
                }

                if (var_r28->pChildren != NULL)
                {
                    var_r27 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r28->pChildren);
                    for (;;)
                    {
                        if (var_r27->m_type == TLAT_COMPONENT)
                        {
                            var_r27->Update(time);
                        }

                        if (var_r27->pChildren != NULL)
                        {
                            var_r26 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r27->pChildren);
                            for (;;)
                            {
                                if (var_r26->m_type == TLAT_COMPONENT)
                                {
                                    var_r26->Update(time);
                                }

                                if (var_r26->pChildren != NULL)
                                {
                                    var_r25 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r26->pChildren);
                                    for (;;)
                                    {
                                        if (var_r25->m_type == TLAT_COMPONENT)
                                        {
                                            var_r25->Update(time);
                                        }

                                        if (var_r25->pChildren != NULL)
                                        {
                                            var_r24 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r25->pChildren);
                                            for (;;)
                                            {
                                                if (var_r24->m_type == TLAT_COMPONENT)
                                                {
                                                    var_r24->Update(time);
                                                }

                                                if (var_r24->pChildren != NULL)
                                                {
                                                    var_r23 = (TLComponentInstance*)nlDLRingGetStart<TLInstance>(var_r24->pChildren);
                                                    for (;;)
                                                    {
                                                        if (var_r23->m_type == TLAT_COMPONENT)
                                                        {
                                                            var_r23->Update(time);
                                                        }

                                                        if (var_r23->pChildren != NULL)
                                                        {
                                                            var_r22 = nlDLRingGetStart<TLInstance>(var_r23->pChildren);
                                                            for (;;)
                                                            {
                                                                if (var_r22->GetType() == TLAT_COMPONENT)
                                                                {
                                                                    ((TLComponentInstance*)var_r22)->Update(time);
                                                                }

                                                                UpdateAsset(var_r22, time);
                                                                if (nlDLRingIsEnd<TLInstance>(var_r23->pChildren, var_r22) != 0)
                                                                {
                                                                    break;
                                                                }
                                                                var_r22 = var_r22->m_next;
                                                            }
                                                        }

                                                        if (nlDLRingIsEnd<TLInstance>(var_r24->pChildren, var_r23) != 0)
                                                        {
                                                            break;
                                                        }
                                                        var_r23 = (TLComponentInstance*)var_r23->m_next;
                                                    }
                                                }

                                                if (nlDLRingIsEnd<TLInstance>(var_r25->pChildren, var_r24) != 0)
                                                {
                                                    break;
                                                }
                                                var_r24 = (TLComponentInstance*)var_r24->m_next;
                                            }
                                        }

                                        if (nlDLRingIsEnd<TLInstance>(var_r26->pChildren, var_r25) != 0)
                                        {
                                            break;
                                        }
                                        var_r25 = (TLComponentInstance*)var_r25->m_next;
                                    }
                                }

                                if (nlDLRingIsEnd<TLInstance>(var_r27->pChildren, var_r26) != 0)
                                {
                                    break;
                                }
                                var_r26 = (TLComponentInstance*)var_r26->m_next;
                            }
                        }

                        if (nlDLRingIsEnd<TLInstance>(var_r28->pChildren, var_r27) != 0)
                        {
                            break;
                        }
                        var_r27 = (TLComponentInstance*)var_r27->m_next;
                    }
                }

                if (nlDLRingIsEnd<TLInstance>(var_r31->pChildren, var_r28) != 0)
                {
                    break;
                }
                var_r28 = (TLComponentInstance*)var_r28->m_next;
            }
        }

        if (nlDLRingIsEnd<TLInstance>(instance->pChildren, var_r31) != 0)
        {
            break;
        }
        var_r31 = (TLComponentInstance*)var_r31->m_next;
    }
}
