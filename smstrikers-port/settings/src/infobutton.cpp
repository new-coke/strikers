#include "infobutton.h"

#include "helptext.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QVBoxLayout>

namespace {

// One panel for the whole application. Two open at once would be two answers to the same question,
// and closing the old one when a new one opens is what every popover on the platform does.
class HelpPanel : public QFrame
{
public:
    static void showFor(InfoButton* button, const QString& richText,
                        const QString& expanded);
    static void hideAny();

private:
    explicit HelpPanel(QWidget* parent);
    bool eventFilter(QObject* watched, QEvent* event) override;
    // Position against the button, clamped to the window.
    void place(InfoButton* button);

    QLabel* m_label;
    static QPointer<HelpPanel> s_open;
};

QPointer<HelpPanel> HelpPanel::s_open;

HelpPanel::HelpPanel(QWidget* parent)
    : QFrame(parent)
{
    // Painted, not styled by the platform. The style's own panel is invisible against a dark
    // window, and QFrame's plain box comes out as a hard black rectangle in the light appearance,
    // neither of which is what a popover looks like.
    setObjectName(QStringLiteral("helpPanel"));
    setFrameShape(QFrame::NoFrame);

    // Base and Text, not ToolTipBase and ToolTipText.
    const QPalette pal = palette();
    QColor ground = pal.color(QPalette::Base);
    ground.setAlpha(255);
    const QColor ink = pal.color(QPalette::Text);

    setStyleSheet(QStringLiteral(
        "#helpPanel { background: rgb(%1,%2,%3);"
        " border: 1px solid rgba(%4,%5,%6,70); border-radius: 6px; }"
        "#helpPanel QLabel { color: rgb(%4,%5,%6); background: transparent;"
        " border: none; }")
        .arg(ground.red()).arg(ground.green()).arg(ground.blue())
        .arg(ink.red()).arg(ink.green()).arg(ink.blue()));

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    m_label = new QLabel(this);
    m_label->setTextFormat(Qt::RichText);
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_label->setOpenExternalLinks(false);
    v->addWidget(m_label);
}

void HelpPanel::hideAny()
{
    if (!s_open.isNull())
    {
        qApp->removeEventFilter(s_open);
        s_open->deleteLater();
        s_open.clear();
    }
}

void HelpPanel::showFor(InfoButton* button, const QString& richText,
                        const QString& expanded)
{
    hideAny();

    QWidget* host = button->window();
    if (host == nullptr)
        return;

    auto* panel = new HelpPanel(host);
    s_open = panel;

    // Width is fixed before the height is asked for: a wrapped label's height follows its width,
    // and Qt would otherwise pick a near-square shape. 520 px is what res_scale's row table needs
    // unwrapped.
    const int wanted = qBound(280, host->width() - 80, 520);
    panel->m_label->setText(richText);
    panel->m_label->setFixedWidth(wanted);
    panel->adjustSize();

    // Following the link swaps in the same explanation with the reference paragraphs open and
    // re-lays the panel out around it.
    QObject::connect(panel->m_label, &QLabel::linkActivated, panel,
                     [panel, expanded, button](const QString& href) {
                         if (href != QLatin1String(HelpText::technicalLink()))
                             return;
                         panel->m_label->setText(expanded);
                         panel->adjustSize();
                         panel->place(button);
                     });

    panel->place(button);

    panel->show();
    panel->raise();
    qApp->installEventFilter(panel);
}

void HelpPanel::place(InfoButton* button)
{
    QWidget* host = parentWidget();
    if (host == nullptr)
        return;

    // Below the button, nudged left so the panel's left edge lines up with the control rather than
    // starting at the far right of the row; then clamped, so a setting near the bottom of a short
    // window opens upwards instead of off the end of it.
    const QPoint anchor = button->mapTo(host, QPoint(0, button->height() + 4));
    int x = anchor.x() - width() + button->width() + 8;
    int y = anchor.y();
    x = qBound(8, x, qMax(8, host->width() - width() - 8));
    if (y + height() > host->height() - 8)
        y = qMax(8, button->mapTo(host, QPoint(0, 0)).y() - height() - 4);
    move(x, y);
}

bool HelpPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* w = qobject_cast<QWidget*>(watched);
        // A press inside the panel is a text selection; anywhere else, and the reader has finished
        // with it.
        if (w == nullptr || (w != this && !isAncestorOf(w)))
        {
            hideAny();
            return false;
        }
    }
    else if (event->type() == QEvent::KeyPress)
    {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
        {
            hideAny();
            return true;
        }
    }
    else if (event->type() == QEvent::Resize && watched == window())
    {
        hideAny();
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace

InfoButton::InfoButton(const QString& brief, const QString& full, QWidget* parent)
    : QAbstractButton(parent)
    , m_text(brief)
    , m_full(full)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    // The same text on hover as on click: a tooltip is the discoverable half and the panel is the
    // one you can keep open while you change the setting.
    setToolTip(brief);
    setAccessibleName(tr("What this setting does"));
    connect(this, &QAbstractButton::clicked, this, &InfoButton::showPanel);
}

QSize InfoButton::sizeHint() const
{
    // Tied to the font rather than fixed, so it grows with the system text size instead of turning
    // into a dot at 200%.
    const int d = qMax(14, fontMetrics().height() - 2);
    return QSize(d, d);
}

void InfoButton::showPanel()
{
    HelpPanel::showFor(this, m_text, m_full);
}

void InfoButton::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void InfoButton::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void InfoButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Palette colours only: the window's background is whatever the platform's light or dark
    // appearance says, and a hard-coded grey is legible in exactly one of them.
    const QColor ink = m_hover || isDown() ? palette().color(QPalette::Highlight)
                                           : palette().color(QPalette::Mid);

    const qreal d = qMin(width(), height()) - 1.0;
    const QRectF circle((width() - d) / 2.0, (height() - d) / 2.0, d, d);

    QPen pen(ink);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.drawEllipse(circle);

    // A drawn glyph rather than the letter "i": at 14 pixels the font's own lowercase i is a
    // smudge, and it moves with the font.
    const qreal cx = circle.center().x();
    const qreal r = d / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    const qreal dotR = qMax(0.8, r * 0.11);
    p.drawEllipse(QPointF(cx, circle.top() + r * 0.42), dotR, dotR);
    const qreal stemW = qMax(1.2, r * 0.20);
    p.drawRoundedRect(QRectF(cx - stemW / 2.0, circle.top() + r * 0.68,
                             stemW, r * 0.74),
                      stemW / 2.0, stemW / 2.0);
}
