#include "settingspage.h"

#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kMargin = 20;   // window edge to content
constexpr int kColumnGap = 8; // label column to control column
constexpr int kRowGap = 6;    // one control to the next
constexpr int kSectionGap = 16;

} // namespace

SettingsTabs::SettingsTabs(QWidget* parent)
    : QTabWidget(parent)
{
}

QSize SettingsTabs::sizeHint() const
{
    const QSize base = QTabWidget::sizeHint();
    const QWidget* current = currentWidget();
    if (current == nullptr)
        return base;

    // The tallest page, for every tab; deliberately, and this replaced a version that reported the
    // *selected* page's height so the window could shrink to fit each one.
    (void)current;
    return base;
}

QLabel* SettingsPage::note(const QString& text)
{
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    // A wrapped label's own width hint is one word, which in a form column makes a one-line note
    // wrap into four.
    l->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    QFont f = l->font();
    // 11pt against the system's 13pt: the platform's own secondary size, and small enough that a
    // note reads as a note without a colour that stops working when the appearance changes.
    f.setPointSizeF(f.pointSizeF() * 11.0 / 13.0);
    l->setFont(f);
    l->setForegroundRole(QPalette::PlaceholderText);
    return l;
}

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
{
    m_form = new QFormLayout(this);
    m_form->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    m_form->setHorizontalSpacing(kColumnGap);
    m_form->setVerticalSpacing(kRowGap);
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    // Rows never wrap into two lines: a label that jumps above its control at a narrow width is the
    // shape the platform never uses, and the pane scrolls horizontally long before it would be
    // needed.
    m_form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    m_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
}

void SettingsPage::beginSection(const QString& title)
{
    if (title.isEmpty())
    {
        // Nothing to draw, but the next section still needs its rule.
        m_haveSection = true;
        return;
    }

    auto* head = new QWidget;
    auto* v = new QVBoxLayout(head);
    v->setContentsMargins(0, m_haveSection ? kSectionGap : 0, 0, 2);
    v->setSpacing(6);

    if (m_haveSection)
    {
        auto* rule = new QFrame;
        rule->setFrameShape(QFrame::HLine);
        rule->setFrameShadow(QFrame::Plain);
        rule->setFixedHeight(1);
        // The palette's own divider weight, so it stays a hairline in both appearances instead of a
        // black bar in one of them.
        QPalette p = rule->palette();
        p.setColor(QPalette::WindowText, p.color(QPalette::Mid));
        rule->setPalette(p);
        v->addWidget(rule);
    }

    auto* label = new QLabel(title);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    v->addWidget(label);

    m_form->addRow(head);
    m_haveSection = true;
}

void SettingsPage::addSetting(const QString& label, QWidget* field, QWidget* info)
{
    QWidget* cell = field;
    if (info != nullptr)
    {
        cell = new QWidget;
        auto* h = new QHBoxLayout(cell);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(field);
        h->addWidget(info, 0, Qt::AlignVCenter);
        h->addStretch(1);
    }
    m_form->addRow(label, cell);
}

void SettingsPage::addFieldNote(QWidget* note)
{
    m_form->addRow(QString(), note);
}

void SettingsPage::addNote(QWidget* note)
{
    m_form->addRow(note);
}

void SettingsPage::addBlock(QWidget* block)
{
    m_form->addRow(block);
}

void SettingsPage::finish()
{
    auto* filler = new QWidget;
    filler->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_form->addRow(filler);
}

namespace {

// QScrollArea's own sizeHint is a guess capped at a fraction of the screen, which is the right
// answer for a document and the wrong one for a settings pane: the window is supposed to be the
// height of its content.
class FittingScrollArea : public QScrollArea
{
public:
    QSize sizeHint() const override
    {
        if (widget() == nullptr)
            return QScrollArea::sizeHint();
        const QSize s = widget()->sizeHint();
        const int f = 2 * frameWidth();
        return QSize(s.width() + f, s.height() + f);
    }

    QSize minimumSizeHint() const override { return QSize(320, 160); }
};

} // namespace

QScrollArea* SettingsPage::scrollable()
{
    auto* scroll = new FittingScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(this);
    return scroll;
}
