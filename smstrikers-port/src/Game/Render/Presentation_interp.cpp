/**
 * Offset/Address/Size: 0x0 | 0x801267BC | size: 0xAE4
 */
void Presentation::DoFunctionCall(unsigned int func)
{
    switch (func)
    {
    case 0:
        BeginByPass();
        break;
    case 1:
        DisplayElectricFence();
        break;
    case 2:
        EndByPass();
        break;
    case 3:
        StopWithUndo();
        break;
    case 4:
        mInterruptWipe = (const char*)Pop();
        break;
    case 5:
        Jumbotron::instance.m_AnimationClass = (eJumboType)Pop();
        Jumbotron::instance.BeginLoad();
        break;
    case 6:
    {
        const char* name;
        NisWinnerType arg4 = (NisWinnerType)Pop();
        NisUseFilter arg3 = (NisUseFilter)Pop();
        NisUseStadiumOffset arg2 = (NisUseStadiumOffset)Pop();
        NisTarget arg1 = (NisTarget)Pop();
        name = (const char*)Pop();
        LoadNis(name, arg1, arg2, arg3, arg4);
        break;
    }
    case 7:
        if (cupTrophyHash == 0)
        {
            break;
        }
        mTrophyTextureLoaded = false;
        trophyFileName[nlStrLen<char>(trophyFileName) - 1] = 't';
        glBeginLoadTextureBundle(trophyFileName, ReadTrophyTexture, g_TrophyTextureLocationInMemory);
        break;
    case 8:
        BeginFrameTask::s_FramerateLocked = 1;
        break;
    case 9:
    {
        ReplayType arg0 = (ReplayType)Pop();
        PlayAutoReplay(arg0);
        break;
    }
    case 10:
        if (mByPassing)
        {
            break;
        }
        PlayCharacterDirection();
        break;
    case 11:
        PlayCupOverlay();
        break;
    case 12:
        PlayHighlights();
        break;
    case 13:
        PlayJumbotron();
        break;
    case 14:
        if (mByPassing)
        {
            break;
        }
        PlayNis();
        break;
    case 15:
    {
        float length, delay;
        const char* name;
        m_SP--;
        length = *(float*)m_SP;
        m_SP--;
        delay = *(float*)m_SP;
        name = (const char*)Pop();
        PlayOverlay(name, delay, length);
        break;
    }
    case 16:
    {
        const char* name = (const char*)Pop();
        if (mByPassing)
        {
            break;
        }
        PlaySfx(name);
        break;
    }
    case 17:
    {
        float vol;
        const char* name;
        m_SP--;
        vol = *(float*)m_SP;
        name = (const char*)Pop();
        if (mByPassing)
        {
            break;
        }
        PlaySfxWithVol(name, vol);
        break;
    }
    case 18:
        Pop();
        Pop();
        Pop();
        Pop();
        break;
    case 19:
    {
        /* PORT: sequenced. The order of evaluation of two Pop() calls in one argument list is unspecified. */
        const char* param = (const char*)Pop();
        const char* type = (const char*)Pop();
        RaiseEvent(type, param);
    }
        break;
    case 20:
        ResetNisPlayer();
        break;
    case 21:
        if (mByPassing)
        {
            break;
        }
        SaveGoalAsHighlight();
        break;
    case 22:
        CrowdManager::instance.SetState((eCrowdState)Pop(), false);
        break;
    case 23:
    {
        bool arg0;
        arg0 = Pop() != 0;
        SetTrophyVisible(arg0);
        break;
    }
    case 24:
        StopAllStreams();
        break;
    case 25:
        StopJumbotron();
        break;
    case 26:
        StopOverlay();
        break;
    case 27:
        Pop();
        break;
    case 28:
        UnloadJumbotron();
        break;
    case 29:
        BeginFrameTask::s_FramerateLocked = 0;
        break;
    case 30:
    {
        const char* filter = (const char*)Pop();
        if (mByPassing)
        {
            break;
        }
        WaitForAutoReplayCompletion(filter);
        break;
    }
    case 31:
        if (mByPassing)
        {
            break;
        }
        WaitForCharacterDirection();
        break;
    case 32:
    {
        const char* filter = (const char*)Pop();
        if (mByPassing)
        {
            break;
        }
        WaitForNisCompletion(filter);
        break;
    }
    case 33:
        if (cupTrophyHash == 0)
        {
            break;
        }
        if (mTrophyTextureLoaded)
        {
            break;
        }
        StopWithUndo();
        break;
    case 34:
        Wipe((const char*)Pop());
        break;
    default:
        nlBreak();
        break;
    }
}
