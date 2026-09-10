#include "Game/SH/SHSkillSelect.h"

#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feSlideMenu.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "NL/nlBind.h"
#include "NL/nlString.h"

static const char* NOVICE_SLIDE_NAME = "NOVICE";
static const char* INTERMEDIATE_SLIDE_NAME = "INTERMEDIATE";
static const char* EXPERT_SLIDE_NAME = "EXPERT";
static const char* PRO_SLIDE_NAME = "PRO";

static void onSelectThird(bool);
static void onSelectSecond(bool);
static void onSelectFirst(bool);
static void SkillSelectMenuBack(bool);
static void Proceed(bool);

void SkillSelectScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        GameplaySettings::eSkillLevel skillLevel;
        switch (m_SlideSkillSelect->m_currentSlide)
        {
        case 0:
            skillLevel = mIsSuperCup ? GameplaySettings::PROFESSIONAL : GameplaySettings::ROOKIE;
            break;
        case 1:
            skillLevel = mIsSuperCup ? GameplaySettings::SUPERSTAR : GameplaySettings::PROFESSIONAL;
            break;
        default:
            skillLevel = mIsSuperCup ? GameplaySettings::LEGEND : GameplaySettings::SUPERSTAR;
            break;
        }

        nlSingleton<GameInfoManager>::Instance()->mCurrentCup->mCupSettings.SkillLevel = skillLevel;
        m_SlideSkillSelect->ApplyFunction();
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        SkillSelectMenuBack(mIsSuperCup);
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        m_SlideSkillSelect->PrevItem();
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        m_SlideSkillSelect->NextItem();
    }
}

void SkillSelectScene::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLInstance* inst = FEFinder<TLInstance, 4>::Find(
        presentation,
        InlineHasher(nlStringLowerHash("SkillSelect")));
    TLComponentInstance* comp = (TLComponentInstance*)inst;

    m_SlideSkillSelect = new ((FESlideMenu*)nlMalloc(sizeof(FESlideMenu), 8, false)) FESlideMenu(comp);

    {
        Function<FnVoidVoid> callback(Bind<void, void (*)(bool), bool>(onSelectFirst, mIsSuperCup));
        m_SlideSkillSelect->AddMenuItem(NOVICE_SLIDE_NAME, callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, void (*)(bool), bool>(onSelectSecond, mIsSuperCup));
        m_SlideSkillSelect->AddMenuItem(INTERMEDIATE_SLIDE_NAME, callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, void (*)(bool), bool>(onSelectThird, mIsSuperCup));
        m_SlideSkillSelect->AddMenuItem(EXPERT_SLIDE_NAME, callback);
    }

    m_SlideSkillSelect->m_doWrapAround = true;
    m_SlideSkillSelect->UpdatePresentation();
}

static void onSelectThird(bool isSuperCup)
{
    Proceed(isSuperCup);
}

static void onSelectSecond(bool isSuperCup)
{
    Proceed(isSuperCup);
}

static void onSelectFirst(bool isSuperCup)
{
    Proceed(isSuperCup);
}

static void SkillSelectMenuBack(bool isSuperCup)
{
    if (isSuperCup)
    {
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SUPER_CUP_CHOOSE_CUP, SCREEN_BACK, true);
    }
    else
    {
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_CHOOSE_CUP, SCREEN_BACK, true);
    }
}

static void Proceed(bool isSuperCup)
{
    if (isSuperCup)
    {
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SUPER_CUP_CHOOSE_CAPTAIN, SCREEN_FORWARD, true);
    }
    else
    {
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_CHOOSE_CAPTAIN, SCREEN_FORWARD, true);
    }
}

SkillSelectScene::~SkillSelectScene()
{
    if (m_SlideSkillSelect != NULL)
    {
        delete m_SlideSkillSelect;
    }
}

SkillSelectScene::SkillSelectScene(bool isSuperCup)
    : BaseSceneHandler()
    , mIsSuperCup(isSuperCup)
    , m_SlideSkillSelect(NULL)
{
}
