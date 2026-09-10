#ifndef _FEOPTIONSSUBMENUS_H_
#define _FEOPTIONSSUBMENUS_H_

#include "NL/nlString.h"
#include "NL/nlMain.h"

#include "Game/DB/UserOptions.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/feSlideMenu.h"
#include "Game/FE/tlTextInstance.h"

class FEPresentation;
class TLComponentInstance;
class TLImageInstance;
class TLTextInstance;
class TLInstance;
class GameplaySettings;
class VisualSettings;
class AudioSettings;
class CheatSettings;

class OptionsSubMenu
{
public:
    OptionsSubMenu(FEPresentation* pres, ButtonComponent::ButtonState buttonstate)
        : m_pres(pres)
        , m_buttons(NULL)
        , m_currentButtonState(buttonstate)
        , mSettingsCRC(0)
    {
        mSlideMenuLists[0] = NULL;
        mSlideMenuLists[1] = NULL;
        mSlideMenuLists[2] = NULL;
        mSlideMenuLists[3] = NULL;
        mSlideMenuLists[4] = NULL;
        mSlideMenuLists[5] = NULL;
        mSlideMenuLists[6] = NULL;
        mSlideMenuLists[7] = NULL;
    }

    void ColourAllText(const nlColour& colour, int menuitem)
    {
        SlideMenuList* list = mSlideMenuLists[menuitem];
        if (list != NULL)
        {
            TLComponentInstance* component = list->GetComponentInstance();
            if (component != NULL)
            {
                ColourAllText(*component, colour);
            }
        }
    }

    void ColourAllText(TLComponentInstance& component, const nlColour& colour)
    {
        if (component.GetActiveSlide() != NULL)
        {
            TLSlide* firstSlide = component.GetActiveSlide();
            TLSlide* slide = firstSlide;
            do
            {
                component.SetActiveSlide(slide);
                TLInstance* firstChild = component.GetActiveSlide()->m_instances;
                TLInstance* child = firstChild;
                if (firstChild != NULL)
                {
                    do
                    {
                        if (child->m_type == TLAT_TEXT)
                        {
                            child->SetAssetColour(colour);
                        }
                        else if (child->m_type == TLAT_IMAGE)
                        {
                            unsigned long hash = child->GetHashID();
                            if (hash != nlStringLowerHash("white_box"))
                            {
                                child->SetAssetColour(colour);
                            }
                        }
                        child = child->m_next;
                    } while (child != firstChild);
                }
                slide = slide->m_next;
            } while (slide != firstSlide);
            component.SetActiveSlide(firstSlide);
        }
    }

    virtual ~OptionsSubMenu();
    virtual void Update(float dt);
    virtual void Save() = 0;
    virtual void Revert() = 0;
    virtual bool ChangesMade() = 0;
    virtual void GoBack();

    void SetAButtonLOC(unsigned long locStrId);
    void SetButtonState(ButtonComponent::ButtonState buttonState);
    void BuildSubMenuList(int menuitem, TLComponentInstance* compinstance, bool wraps, int startindex);

    /* 0x004 */ FEPresentation* m_pres;
    /* 0x008 */ TLComponentInstance* m_buttons;
    /* 0x00C */ ButtonComponent::ButtonState m_currentButtonState;
    /* 0x010 */ ButtonComponent mButtons;
    /* 0x034 */ MenuList<TLComponentInstance> mMenuItems;
    /* 0x248 */ SlideMenuList* mSlideMenuLists[8];
    /* 0x268 */ unsigned long mSettingsCRC;
}; // total size: 0x26C

class OptionsSaveLoad : public OptionsSubMenu
{
public:
    OptionsSaveLoad(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate);
    virtual ~OptionsSaveLoad();
    virtual void Update(float dt);
    virtual void Save();
    virtual void Revert();
    virtual bool ChangesMade()
    {
        return false;
    }
}; // total size: 0x26C

class OptionsGameplayMenuV2 : public OptionsSubMenu
{
public:
    OptionsGameplayMenuV2(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate, GameplaySettings& settings, int skilltoskip);
    virtual ~OptionsGameplayMenuV2();
    virtual void Save();
    virtual void Revert();
    virtual bool ChangesMade()
    {
        u32 checksum = nlChecksum32(&mSettings, sizeof(GameplaySettings));
        return mSettingsCRC != checksum;
    }

    void BuildSkillLevelMenu(TLComponentInstance* compinstance, int startindex, int skilltoskip);
    void CloseItem(TLComponentInstance* compinstance);
    void OpenItem(TLComponentInstance* compinstance);

    /* 0x26C */ GameplaySettings& mSettings;
    /* 0x270 */ GameplaySettings mBackupSettings;
}; // total size: 0x27C

class OptionsVisualMenuV2 : public OptionsSubMenu
{
public:
    OptionsVisualMenuV2(FEPresentation* pres, ButtonComponent::ButtonState btnState, VisualSettings& settings);
    virtual ~OptionsVisualMenuV2();
    virtual void Update(float dt);
    virtual void Save();
    virtual void Revert();
    virtual bool ChangesMade()
    {
        u32 checksum = nlChecksum32(&mSettings, sizeof(VisualSettings));
        return mSettingsCRC != checksum;
    }

    /* 0x26C */ VisualSettings& mSettings;
    /* 0x270 */ VisualSettings mBackupSettings;
}; // total size: 0x27C

class OptionsAudioMenuV2 : public OptionsSubMenu
{
public:
    OptionsAudioMenuV2(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate, AudioSettings& settings);
    virtual ~OptionsAudioMenuV2();
    virtual void Update(float dt);
    virtual void Save();
    virtual void Revert();
    virtual bool ChangesMade()
    {
        u32 checksum = nlChecksum32(&mSettings, sizeof(AudioSettings));
        return mSettingsCRC != checksum;
    }

    /* 0x26C */ AudioSettings& mSettings;
    /* 0x270 */ AudioSettings mBackupSettings;
    /* 0x290 */ bool mbUpdateMode;
}; // total size: 0x294

class OptionsCheatsMenu : public OptionsSubMenu
{
public:
    OptionsCheatsMenu(FEPresentation* pres, ButtonComponent::ButtonState btnState, CheatSettings& settings);
    virtual ~OptionsCheatsMenu();
    virtual void Save();
    virtual void Revert();
    virtual bool ChangesMade()
    {
        u32 checksum = nlChecksum32(&mSettings, sizeof(CheatSettings));
        return mSettingsCRC != checksum;
    }

    void BuildCustomPowerupsList(TLComponentInstance* compinstance, CustomPowerups startOption, FEPresentation* presentation);
    void BuildLockableSubMenuList(int menuitem, TLComponentInstance* compinstance, FEPresentation* presentation, bool unlocked, int startindex);

    /* 0x26C */ CheatSettings& mSettings;
    /* 0x270 */ CheatSettings mBackupSettings;
}; // total size: 0x278

#endif // _FEOPTIONSSUBMENUS_H_
