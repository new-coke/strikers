#ifndef _OVERLAYHANDLERHUD_H_
#define _OVERLAYHANDLERHUD_H_

#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feTextureResource.h"
#include "Game/FE/BaseOverlayHandler.h"
#include "Game/FE/feRender.h"
#include "Game/FE/tlTextInstance.h"

class HUDOverlay : public BaseOverlayHandler
{
public:
    HUDOverlay();
    virtual ~HUDOverlay();
    virtual void Update(float fDeltaT);
    virtual void SceneCreated();
    void SetSlideIn();
    void SetSlideOut();
    void LoadHUDTextures();
    void DisplayPowerUps();
    void SetTeamIcons();
    void UpdateScore();
    void DisplayNewScore();
    void ResetScores();
    void SwapPowerUps(int homeAway);

    /*0x28*/ TLImageInstance* m_pImagePowerUps[2][2][2];
    /*0x48*/ TLImageInstance* m_pImageFlares[2][2][2];
    /*0x68*/ TLComponentInstance* m_pComponentFlares[2][2];
    /*0x78*/ TLComponentInstance* mSuddenDeath[2];
    /*0x80*/ TLComponentInstance* m_pPowerupTextComponents[2][2][2];
    /*0xA0*/ int mNumFlareCycles[2][2];
    /*0xB0*/ FETextureResource* m_pStar;
    /*0xB4*/ FETextureResource* m_pShellGreen;
    /*0xB8*/ FETextureResource* m_pShellRed;
    /*0xBC*/ FETextureResource* m_pBanana;
    /*0xC0*/ FETextureResource* m_pMushroom;
    /*0xC4*/ FETextureResource* m_pShellBlue;
    /*0xC8*/ FETextureResource* m_pBobomb;
    /*0xCC*/ FETextureResource* m_pShellSpike;
    /*0xD0*/ FETextureResource* m_pChomp;
    /*0xD4*/ u32 mSeconds;
    /*0xD8*/ u32 mMinutes;
    /*0xDC*/ u32 mTenths;
    /*0xE0*/ TLTextInstance* m_pTextInstanceClock[2];
    /*0xE8*/ u16 mClockBuffer[32];
    /*0x128*/ bool mClockColourChanged;
    /*0x129*/ bool mOvertimeSFXPlayed;
    /*0x12A*/ bool mIsHUDSlideIn;
    /*0x12B*/ bool mStartScoreAnimation;
    /*0x12C*/ nlColour mOriginalClockColour;
    /*0x130*/ int mScore[2];
    /*0x138*/ int mNewScore[2];
    /*0x140*/ u16 mScoreBuffer[2][32];
    /*0x1C0*/ TLTextInstance* m_pTextInstanceScore[2][2];
    /*0x1D0*/ AsyncImage* mAsyncImage[2];
    /*0x1D8*/ float mScoreUpdateDelay[2];
};

/*class FEFinder<TLImageInstance, 2>
{
public:
    void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
    void _Find<TLInstance>(TLInstance*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void _Find<TLSlide>(TLSlide*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void Find<TLSlide>(TLSlide*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
};


class FEFinder<TLComponentInstance, 4>
{
public:
    void _Find<TLInstance>(TLInstance*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void _Find<TLSlide>(TLSlide*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
};


class FEFinder<TLTextInstance, 3>
{
public:
    void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
    void _Find<TLInstance>(TLInstance*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void _Find<TLSlide>(TLSlide*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    void Find<TLSlide>(TLSlide*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
};
*/

/*
class Format<BasicString<unsigned short, Detail
{
public:
    void TempStringAllocator>, unsigned short[8], unsigned short[8]>(const BasicString<unsigned short, Detail::TempStringAllocator>&, const unsigned short(&)[8], const unsigned short(&)[8]);
};


class FormatImpl<BasicString<unsigned short, Detail
{
public:
    void TempStringAllocator>>::operator%<const unsigned short*>(const unsigned short* const&);
};
*/

#endif // _OVERLAYHANDLERHUD_H_
