// A button that shows a key binding and becomes the next key pressed while capturing.

#pragma once

#include <QPushButton>
#include <QString>

class KeyCaptureButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyCaptureButton(QWidget* parent = nullptr);

    QString binding() const { return m_binding; }
    void setBinding(const QString& sdlName);

    // Draw the label as a conflict: this key is bound somewhere else too.
    void setConflict(bool conflict);

    bool isCapturing() const { return m_capturing; }

signals:
    void bindingChanged(const QString& sdlName);

protected:
    bool event(QEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void beginCapture();
    void endCapture();
    void refresh();

    QString m_binding;
    bool m_capturing = false;
    bool m_conflict = false;
};
