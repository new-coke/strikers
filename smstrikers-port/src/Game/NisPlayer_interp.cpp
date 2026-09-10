/**
 * Offset/Address/Size: 0x0 | 0x801186C4 | size: 0x74C
 */
void NisPlayer::DoFunctionCall(unsigned int functionId)
{
    switch (functionId)
    {
    case 0:
    {
        m_SP--;

        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;
        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_EFFECT, frame, name, target, &params);
        break;
    }
    case 1:
    {
        m_SP--;
        int useStopAtNisEnd = (int)*m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;
        if (useStopAtNisEnd == 1)
        {
            params.param2 = 1;
        }

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_SOUND, frame, name, target, &params);
        break;
    }
    case 2:
    {
        m_SP--;
        int useStopAtNisEnd = (int)*m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        float volume = *(float*)m_SP;
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.float1 = volume;
        params.param1 = triggerParam1;
        if (useStopAtNisEnd == 1)
        {
            params.param2 = 1;
        }

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_SOUND, frame, name, target, &params);
        break;
    }
    case 3:
    {
        m_SP--;
        int useNameAsTarget = (int)*m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        uintptr_t triggerParam2 = *m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;
        params.param2 = triggerParam2;
        if (useNameAsTarget == 1)
        {
            params.param3 = 1;
        }

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_RANDOM_DIALOGUE, frame, kNisEmpty, target, &params);
        break;
    }
    case 4:
    {
        m_SP--;
        uintptr_t triggerParam4 = *m_SP;
        m_SP--;
        uintptr_t triggerParam3 = *m_SP;
        m_SP--;
        uintptr_t triggerParam2 = *m_SP;
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;
        params.param2 = triggerParam2;
        params.param3 = triggerParam3;
        params.param4 = triggerParam4;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_STREAM, frame, kNisEmpty, kNisEmpty, &params);
        break;
    }
    case 5:
    {
        m_SP--;
        int useStopAtNisEnd = (int)*m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        if (useStopAtNisEnd == 1)
        {
            params.param2 = 1;
        }

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_SOUND, frame, name, target, &params);
        break;
    }
    case 6:
    {
        m_SP--;
        int useStopAtNisEnd = (int)*m_SP;
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        float volume = *(float*)m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.float1 = volume;
        if (useStopAtNisEnd == 1)
        {
            params.param2 = 1;
        }

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_PLAY_SOUND, frame, name, target, &params);
        break;
    }
    case 7:
    {
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_RAISE_EVENT, frame, name, target, NULL);
        break;
    }
    case 8:
    {
        m_SP--;
        float frame = *(float*)m_SP;
        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_REGISTER_GOAL_AUDIO, frame, kNisEmpty, kNisEmpty, NULL);
        break;
    }
    case 9:
    {
        m_SP--;
        uintptr_t loopActive = *m_SP;
        m_SP--;
        uintptr_t stopAtNisEnd = *m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;

        if (loopActive != 0)
        {
            params.param1 = 1;
        }
        else
        {
            params.param1 = 0;
        }

        params.param2 = stopAtNisEnd;
        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_SET_ACTIVE_STREAM_LOOPING, frame, kNisEmpty, kNisEmpty, &params);
        break;
    }
    case 10:
    {
        m_SP--;
        float frame = *(float*)m_SP;
        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_STOP_ALL_STREAMS, frame, kNisEmpty, kNisEmpty, NULL);
        break;
    }
    case 11:
    {
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_STOP_SOUND, frame, name, target, &params);
        break;
    }
    case 12:
    {
        m_SP--;
        uintptr_t triggerParam1 = *m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.param1 = triggerParam1;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_STOP_STREAM, frame, kNisEmpty, kNisEmpty, &params);
        break;
    }
    case 13:
    {
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_STOP_SOUND, frame, name, target, NULL);
        break;
    }
    case 14:
    {
        m_SP--;
        float delta = *(float*)m_SP;
        m_SP--;
        float frame = *(float*)m_SP;

        Nis::TriggerParams params;
        params.float1 = -1.0f;
        params.param1 = -1;
        params.param2 = -1;
        params.param3 = -1;
        params.param4 = -1;
        params.float1 = delta;

        mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_TIME_DILATION, frame, kNisEmpty, kNisEmpty, &params);
        break;
    }
    default:
        nlBreak();
        break;
    }
}
