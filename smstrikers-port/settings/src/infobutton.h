// The circled i beside every setting, and the panel it opens.

#pragma once

#include <QAbstractButton>
#include <QString>


class InfoButton : public QAbstractButton
{
    Q_OBJECT

public:
    // `brief` is what the panel opens with; `full` is the same thing with the reference paragraphs
    // already expanded, which the panel swaps in when the "Technical details" link is followed.
    InfoButton(const QString& brief, const QString& full, QWidget* parent = nullptr);

    QSize sizeHint() const override;

    // Show the panel, as a click does. --screenshot-help uses this.
    void showPanel();

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_text;
    QString m_full;
    bool m_hover = false;
};
