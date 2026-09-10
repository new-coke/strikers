#ifndef _FEBUTTONCOMPONENT_H_
#define _FEBUTTONCOMPONENT_H_

#include "Game/FE/feRender.h"
#include "Game/FE/tlComponentInstance.h"

class nlFont;

class ButtonComponent
{
public:
    enum ButtonState
    {
        BS_INVALID = -1,
        BS_A_AND_B = 0,
        BS_A_ONLY = 1,
        BS_B_ONLY = 2,
        BS_A_AND_B_AND_Y = 3,
    };

    void CentreButtons();
    void SetState(ButtonComponent::ButtonState buttonstate);
    ~ButtonComponent();
    ButtonComponent();
    static int GetRenderedStringLength(const unsigned short* pString, const nlFont* pFont);

    /* 0x00 */ TLComponentInstance* mButtonInstance;
    /* 0x04 */ TLImageInstance* mButtonImages[3];
    /* 0x10 */ TLTextInstance* mButtonLabels[3];
    /* 0x1C */ int mNumButtons;
    /* 0x20 */ bool mAlreadyCentred;
};

#endif // _FEBUTTONCOMPONENT_H_
