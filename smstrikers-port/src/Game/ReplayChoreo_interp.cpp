/**
 * Offset/Address/Size: 0x0 | 0x801287C0 | size: 0x3B4
 */
void ReplayChoreo::DoFunctionCall(unsigned int func)
{
    switch (func)
    {
    case 0:
    {
        m_SP--;
        ReplayCameraFocus arg0 = (ReplayCameraFocus)*m_SP;
        AddCameraFocus(arg0);
        break;
    }
    case 1:
    {
        m_SP--;
        f32 duration = *(f32*)m_SP;
        FilterOn(duration);
        break;
    }
    case 2:
    {
        m_SP--;
        f32 duration = *(f32*)m_SP;
        FilterOff(duration);
        break;
    }
    case 3:
    {
        FreezeCamera();
        break;
    }
    case 4:
    {
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        const char* sfxName = (const char*)*m_SP;
        PlayWorldSfx(sfxName, target);
        break;
    }
    case 5:
    {
        m_SP--;
        const char* target = (const char*)*m_SP;
        m_SP--;
        f32 vol = *(f32*)m_SP;
        m_SP--;
        const char* sfxName = (const char*)*m_SP;
        PlayWorldSfxWithVol(sfxName, vol, target);
        break;
    }
    case 6:
    {
        m_SP--;
        f32 timeOffset = *(f32*)m_SP;
        m_SP--;
        ReplayEvent arg0 = (ReplayEvent)*m_SP;
        Rewind(arg0, timeOffset);
        break;
    }
    case 7:
    {
        m_SP--;
        f32 time = *(f32*)m_SP;
        RunFor(time);
        break;
    }
    case 8:
    {
        m_SP--;
        f32 timeOffset = *(f32*)m_SP;
        m_SP--;
        ReplayEvent arg0 = (ReplayEvent)*m_SP;
        if (!IsFinished())
        {
            f32 currentTime = mReplayManager->mTime;
            f32 lastOccurence = mReplay->TimeOfLastOccurence(arg0);
            if (currentTime < timeOffset + lastOccurence)
            {
                StopWithUndo();
            }
        }
        break;
    }
    case 9:
    {
        m_SP--;
        ReplayCameraPosition position = (ReplayCameraPosition)*m_SP;
        SetCamera(position);
        break;
    }
    case 10:
    {
        m_SP--;
        ReplayCameraFocus arg0 = (ReplayCameraFocus)*m_SP;
        SetCameraFocus(arg0);
        break;
    }
    case 11:
    {
        m_SP--;
        f32 fov = *(f32*)m_SP;
        SetCameraFov(fov);
        break;
    }
    case 12:
    {
        m_SP--;
        f32 speed = *(f32*)m_SP;
        Speed(speed);
        break;
    }
    case 13:
    {
        m_SP--;
        f32 deltaFov = *(f32*)m_SP;
        StartCameraZoom(deltaFov);
        break;
    }
    case 14:
    {
        m_SP--;
        f32 speedUp = *(f32*)m_SP;
        StartSpeedUp(speedUp);
        break;
    }
    case 15:
    {
        m_SP--;
        const char* sfxName = (const char*)*m_SP;
        StopWorldSfx(sfxName);
        break;
    }
    default:
        nlBreak();
        break;
    }
}
