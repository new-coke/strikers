// One tab's worth of settings, laid out the way the platform lays settings out.

#pragma once

#include <QTabWidget>
#include <QWidget>

class QFormLayout;
class QLabel;
class QScrollArea;

// The tab strip over those pages. QTabWidget::sizeHint() asks every page how tall it wants to be
// and takes the largest, reading sizeHint() directly, so marking a page Ignored does nothing.
class SettingsTabs : public QTabWidget
{
    Q_OBJECT

public:
    explicit SettingsTabs(QWidget* parent = nullptr);
    QSize sizeHint() const override;
};

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    // A rule and a short heading. The first section on a page gets the heading without the rule:
    // there is nothing above it to separate it from.
    void beginSection(const QString& title);

    // `label` goes in the right-aligned column, `field` in the control column.
    void addSetting(const QString& label, QWidget* field, QWidget* info = nullptr);

    // A line under the control column, aligned with it: for state, not for help.
    void addFieldNote(QWidget* note);

    // Full width, under everything: a note about the section rather than about one control.
    void addNote(QWidget* note);

    // A widget that is not a setting, a controller layout grid, the advanced table; spanning both
    // columns.
    void addBlock(QWidget* block);

    QFormLayout* form() const { return m_form; }

    // Call once, after the last row: the trailing stretch that keeps the rows at the top of a
    // window taller than they are.
    void finish();

    // The page in a scroll area, which is what a tab holds.
    QScrollArea* scrollable();

    // The secondary-colour, smaller label used for state and notes.
    static QLabel* note(const QString& text);

private:
    QFormLayout* m_form;
    bool m_haveSection = false;
};
