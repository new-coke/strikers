#include "Game/FE/feScrollingTicker.h"
#include "Game/FE/feFinder.h"

#include "NL/nlLocalization.h"

/**
 * Offset/Address/Size: 0xF9C | 0x800A0BF4 | size: 0xB0
 */
void ScrollingTickerScene::setSizeTweenCallback(void* scene, const float* value)
{
    ScrollingTickerScene* tscene = (ScrollingTickerScene*)scene;
    f32 val = value[0];
    f32 closedY = tscene->m_leftBallClosedPos.f.y;
    f32 open = tscene->m_leftBallOpenPos.f.x;
    f32 x;

    x = val * (open - tscene->m_leftBallClosedPos.f.x) + tscene->m_leftBallClosedPos.f.x;
    tscene->m_leftBall->SetAssetPosition(x, closedY, 0.0f);

    open = tscene->m_rightBallOpenPos.f.x;
    x = open - tscene->m_rightBallClosedPos.f.x;
    x = val * x + tscene->m_rightBallClosedPos.f.x;
    tscene->m_rightBall->SetAssetPosition(x, closedY, 0.0f);

    open = tscene->m_grayOpenScale.f.x;
    x = open - tscene->m_grayClosedScale.f.x;
    x = val * x + tscene->m_grayClosedScale.f.x;
    tscene->m_backRectangle->SetAssetScale(x, tscene->m_grayOpenScale.f.y, 1.0f);
}

/**
 * Offset/Address/Size: 0xED0 | 0x800A0B28 | size: 0xCC
 */
void ScrollingTickerScene::setScaleTweenCallback(void* scene, const float* value)
{
    ScrollingTickerScene* tscene = (ScrollingTickerScene*)scene;
    f32 val = *value;
    f32 x = tscene->m_ballClosedScale.f.x * val;
    f32 y = tscene->m_ballClosedScale.f.y * val;
    f32 z = tscene->m_ballClosedScale.f.z * val;
    tscene->m_leftBall->SetAssetScale(x, y, z);
    tscene->m_rightBall->SetAssetScale(x, y, z);
    tscene->m_backRectangle->SetAssetScale(
        tscene->m_grayClosedScale.f.x * val,
        tscene->m_grayClosedScale.f.y * val,
        tscene->m_grayClosedScale.f.z * val);
}

/**
 * Offset/Address/Size: 0xEB8 | 0x800A0B10 | size: 0x18
 */
void ScrollingTickerScene::tickerOpened(void* pScene)
{
    ScrollingTickerScene* scene = (ScrollingTickerScene*)pScene;
    scene->m_active = 1;
    scene->m_textScroller->m_msgTime = 0.0f;
}

/**
 * Offset/Address/Size: 0xE88 | 0x800A0AE0 | size: 0x30
 */
void ScrollingTickerScene::tickerClosed(void* scene)
{
    ((ScrollingTickerScene*)scene)->SetVisible(false);
}

/**
 * Offset/Address/Size: 0xE00 | 0x800A0A58 | size: 0x88
 */
ScrollingTickerScene::ScrollingTickerScene()
    : BaseSceneHandler()
    , m_active(false)
    , m_cbFunc()
    , m_textScroller(NULL)
    , m_pFETweenManager()
{
}

/**
 * Offset/Address/Size: 0xC88 | 0x800A08E0 | size: 0x178
 */
ScrollingTickerScene::~ScrollingTickerScene()
{
    if (m_textScroller)
    {
        delete m_textScroller;
    }
}

/**
 * Offset/Address/Size: 0x654 | 0x800A02AC | size: 0x634
 */
void ScrollingTickerScene::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();

    TLInstance* instance = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("closed")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ticker_ball_left")));
    m_leftBallClosedPos = instance->GetPosition();

    instance = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("closed")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ticker_ball_right")));
    m_rightBallClosedPos = instance->GetPosition();
    m_ballClosedScale = instance->GetScale();

    instance = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("closed")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Rectangle")));
    m_grayClosedScale = instance->GetScale();

    pres->SetActiveSlide("open");

    m_textBox = (TLTextInstance*)FEFinder<TLInstance, 3>::Find(
        pres,
        InlineHasher(nlStringLowerHash("open")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Text")));

    m_leftBall = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("open")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ticker_ball_left")));

    m_rightBall = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("open")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ticker_ball_right")));

    m_backRectangle = FEFinder<TLInstance, 2>::Find(
        pres,
        InlineHasher(nlStringLowerHash("open")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Rectangle")));

    m_leftBallOpenPos = m_leftBall->GetPosition();
    m_rightBallOpenPos = m_rightBall->GetPosition();
    m_grayOpenScale = m_backRectangle->GetScale();

    f32 closedY = m_leftBallClosedPos.f.y;
    f32 open = m_leftBallOpenPos.f.x;
    f32 x;
    f32 val = 0.0f;

    x = val * (open - m_leftBallClosedPos.f.x) + m_leftBallClosedPos.f.x;
    m_leftBall->SetAssetPosition(x, closedY, val);

    open = m_rightBallOpenPos.f.x;
    x = open - m_rightBallClosedPos.f.x;
    x = val * x + m_rightBallClosedPos.f.x;
    m_rightBall->SetAssetPosition(x, closedY, val);

    open = m_grayOpenScale.f.x;
    x = open - m_grayClosedScale.f.x;
    x = val * x + m_grayClosedScale.f.x;
    m_backRectangle->SetAssetScale(x, m_grayOpenScale.f.y, 1.0f);

    f32 sz;
    f32 sy;
    f32 sx;
    sx = m_ballClosedScale.f.x * val;
    sy = m_ballClosedScale.f.y * val;
    sz = m_ballClosedScale.f.z * val;
    m_leftBall->SetAssetScale(sx, sy, sz);
    m_rightBall->SetAssetScale(sx, sy, sz);

    m_backRectangle->SetAssetScale(
        m_grayClosedScale.f.x * val,
        m_grayClosedScale.f.y * val,
        m_grayClosedScale.f.z * val);

    m_textBox->m_bVisible = false;

    m_textScroller = new (nlMalloc(sizeof(FEScrollText), 8, false))
        FEScrollText(m_textBox, 0, (int)(m_rightBallOpenPos.f.x - m_leftBallOpenPos.f.x));

    m_textScroller->m_messageFinishedCB = m_cbFunc;

    SetVisible(false);
}

/**
 * Offset/Address/Size: 0x630 | 0x800A0288 | size: 0x24
 */
void ScrollingTickerScene::SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>& msg)
{
    m_textScroller->SetDisplayMessage(msg);
}

static inline const unsigned short* LookupLocText(const char* locMessage)
{
    unsigned long hash = nlStringLowerHash(locMessage);
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }
    nlLocalization::StringLookup* lookup = nlBSearch<nlLocalization::StringLookup, unsigned long>(hash, loc->m_LookupTable, loc->m_pFile->StringCount);
    if (lookup != 0)
    {
        return loc->m_FirstString + lookup->StringOffset;
    }
    return MissingLocString;
}

void ScrollingTickerScene::SetDisplayMessage(const char* locMessage)
{
    const unsigned short* text = LookupLocText(locMessage);
    SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(text));
}

/**
 * Offset/Address/Size: 0x5D0 | 0x800A0228 | size: 0x60
 */
void ScrollingTickerScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    m_pFETweenManager.Update(fDeltaT);
    if (m_active)
    {
        m_textScroller->Update(fDeltaT);
    }
}

static inline void setBallPositions(ScrollingTickerScene* tscene, f32 val)
{
    f32 closedY = tscene->m_leftBallClosedPos.f.y;
    f32 open = tscene->m_leftBallOpenPos.f.x;
    f32 x;

    x = val * (open - tscene->m_leftBallClosedPos.f.x) + tscene->m_leftBallClosedPos.f.x;
    tscene->m_leftBall->SetAssetPosition(x, closedY, 0.0f);

    open = tscene->m_rightBallOpenPos.f.x;
    x = open - tscene->m_rightBallClosedPos.f.x;
    x = val * x + tscene->m_rightBallClosedPos.f.x;
    tscene->m_rightBall->SetAssetPosition(x, closedY, 0.0f);

    open = tscene->m_grayOpenScale.f.x;
    x = open - tscene->m_grayClosedScale.f.x;
    x = val * x + tscene->m_grayClosedScale.f.x;
    tscene->m_backRectangle->SetAssetScale(x, tscene->m_grayOpenScale.f.y, 1.0f);
}

/**
 * Offset/Address/Size: 0x3C4 | 0x800A001C | size: 0x20C
 */
void ScrollingTickerScene::OpenMessenger()
{
    m_pFETweenManager.clearTweens();

    f32 from = 0.0f;
    f32 to = 1.0f;

    setBallPositions(this, 0.0f);

    setScaleTweenCallback(this, &from);

    m_textBox->m_bVisible = false;

    FETweener* tickerExpand = m_pFETweenManager.createTween(
        &from, &to, 1.0f, 0.0f, 1, TweenFunctions::easeoutelastic, this, setSizeTweenCallback);

    FETweener* tickerGrow = m_pFETweenManager.createTween(
        &from, &to, 0.15f, 0.0f, 1, TweenFunctions::linear, this, setScaleTweenCallback);

    tickerGrow->setNextTween(tickerExpand);
    tickerExpand->setDoneCallFunc(tickerOpened, this);
    m_pFETweenManager.startTween(tickerGrow);

    SetVisible(true);
}

/**
 * Offset/Address/Size: 0x258 | 0x8009FEB0 | size: 0x16C
 */
void ScrollingTickerScene::OpenMessengerNow()
{
    m_pFETweenManager.clearTweens();

    f32 closedY = m_leftBallClosedPos.f.y;
    f32 open = m_leftBallOpenPos.f.x;
    f32 x;
    f32 val = 1.0f;
    f32 leftClosedX = m_leftBallClosedPos.f.x;

    x = open - leftClosedX;
    x = val * x + leftClosedX;
    m_leftBall->SetAssetPosition(x, closedY, 0.0f);

    open = m_rightBallOpenPos.f.x;
    x = open - m_rightBallClosedPos.f.x;
    x = val * x + m_rightBallClosedPos.f.x;
    m_rightBall->SetAssetPosition(x, closedY, 0.0f);

    open = m_grayOpenScale.f.x;
    x = open - m_grayClosedScale.f.x;
    x = val * x + m_grayClosedScale.f.x;
    m_backRectangle->SetAssetScale(x, m_grayOpenScale.f.y, 1.0f);

    f32 sz;
    f32 sy;
    f32 sx;
    sx = m_ballClosedScale.f.x * val;
    sy = m_ballClosedScale.f.y * val;
    sz = m_ballClosedScale.f.z * val;
    m_leftBall->SetAssetScale(sx, sy, sz);
    m_rightBall->SetAssetScale(sx, sy, sz);

    m_backRectangle->SetAssetScale(
        m_grayClosedScale.f.x * val,
        m_grayClosedScale.f.y * val,
        m_grayClosedScale.f.z * val);

    m_textBox->m_bVisible = true;
    SetVisible(true);
    m_active = 1;
    m_textScroller->m_msgTime = 0.0f;
}

/**
 * Offset/Address/Size: 0x164 | 0x8009FDBC | size: 0xF4
 */
void ScrollingTickerScene::CloseMessenger()
{
    m_pFETweenManager.clearTweensOnObj(this);

    f32 endScale = 1.0f;
    f32 startScale = 0.0f;

    FETweener* scaleTween = m_pFETweenManager.createTween(
        &endScale, &startScale, 0.4f, -0.3f, 1, TweenFunctions::easeinelastic, this, setScaleTweenCallback);

    FETweener* sizeTween = m_pFETweenManager.createTween(
        &endScale, &startScale, 0.1f, 0.0f, 1, TweenFunctions::linear, this, setSizeTweenCallback);

    sizeTween->setNextTween(scaleTween);
    scaleTween->setDoneCallFunc(tickerClosed, this);
    m_pFETweenManager.startTween(sizeTween);

    m_textBox->m_bVisible = false;
    m_active = 0;
}

/**
 * Offset/Address/Size: 0x8 | 0x8009FC60 | size: 0x15C
 */
void ScrollingTickerScene::CloseMessengerNow()
{
    m_pFETweenManager.clearTweensOnObj(this);

    f32 closedY = m_leftBallClosedPos.f.y;
    f32 open = m_leftBallOpenPos.f.x;
    f32 x;
    f32 val = 0.0f;

    x = val * (open - m_leftBallClosedPos.f.x) + m_leftBallClosedPos.f.x;
    m_leftBall->SetAssetPosition(x, closedY, val);

    open = m_rightBallOpenPos.f.x;
    x = open - m_rightBallClosedPos.f.x;
    x = val * x + m_rightBallClosedPos.f.x;
    m_rightBall->SetAssetPosition(x, closedY, val);

    open = m_grayOpenScale.f.x;
    x = open - m_grayClosedScale.f.x;
    x = val * x + m_grayClosedScale.f.x;
    m_backRectangle->SetAssetScale(x, m_grayOpenScale.f.y, 1.0f);

    f32 sz;
    f32 sy;
    f32 sx;
    sx = m_ballClosedScale.f.x * val;
    sy = m_ballClosedScale.f.y * val;
    sz = m_ballClosedScale.f.z * val;
    m_leftBall->SetAssetScale(sx, sy, sz);
    m_rightBall->SetAssetScale(sx, sy, sz);

    m_backRectangle->SetAssetScale(
        m_grayClosedScale.f.x * val,
        m_grayClosedScale.f.y * val,
        m_grayClosedScale.f.z * val);

    m_textBox->m_bVisible = false;
    m_active = 0;
    SetVisible(false);
}

/**
 * Offset/Address/Size: 0x0 | 0x8009FC58 | size: 0x8
 */
bool ScrollingTickerScene::IsMessengerOpen() const
{
    return m_bVisible;
}
