// The settings window. One idea holds the whole thing together: every control registers a pair of
// closures that turn it into the string the ini file wants and back.

#pragma once

#include "inifile.h"
#include "schema.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>

#include <functional>

class InfoButton;
class KeyCaptureButton;
class SettingsPage;
class QCheckBox;
class QComboBox;
class QDir;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Read `path`. A missing file is not an error, and it is not an empty document either:
    // strikers.ini.example beside the application is loaded as the starting text so the player's
    // own file inherits every comment the release shipped.
    void openFile(const QString& path);
    QString filePath() const { return m_path; }

    bool saveTo(const QString& path, QString* error);

    // For --selftest: reach a control by key without a mouse.
    bool setValueByKey(const QString& key, const QString& value);
    QString valueByKey(const QString& key) const;

    // For --screenshot-help: open one setting's info panel, so a headless run can show what the
    // explanation actually looks like.
    bool showHelpFor(const QString& key);

    // Restore the size and position the window had last time, and write them back on the way out.
    void rememberGeometry();

    // Resize events seen so far. --stress-resize reads it: a window whose layout feeds back into
    // its own geometry answers one resize with many.
    int resizeCount() const { return m_resizes; }

    // The game launched this, on a player's first run, and is waiting for it to close
    // (include/port/firstrun.h).
    void setFirstRun(bool firstRun);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSave();
    void onRevert();
    void onResetAll();
    void onPlay();

private:
    struct Control
    {
        QString key;
        QString section;
        QString def;
        std::function<QString()> get;
        std::function<void(const QString&)> set;
    };
    QWidget* buildDisplayTab();
    QWidget* buildAudioTab();
    QWidget* buildInputTab();
    QWidget* buildKeyboardPage();
    QWidget* buildGamepadPage();
    QWidget* buildGameTab();
    QWidget* buildAdvancedBox();

    // Widget factories that also register the control
    void addSwitch(SettingsPage* page, const Setting& s);
    QComboBox* addChoice(SettingsPage* page, const Setting& s);
    InfoButton* infoFor(const Setting& s);

    void registerControl(const Setting& s, std::function<QString()> get,
                         std::function<void(const QString&)> set);

    void loadIntoUi();
    void collectFromUi();

    void refreshAdvancedTable();
    void applyAdvancedTable();

    void updateConflicts();
    void updateDataState();
    void updateLanguageState(const QString& gameId);
    static QString discGameId(const QDir& files);
    void updatePlayButton();
    void setDirty(bool dirty);
    void markDirty() { setDirty(true); }
    void showPath(const QString& text);
    void fitToCurrentTab();

    IniFile m_ini;
    QString m_path;
    bool m_dirty = false;

    QVector<Control> m_controls;
    QHash<QString, int> m_controlByKey;  // normalised key -> index into m_controls
    QHash<QString, InfoButton*> m_infoByKey;

    QTabWidget* m_tabs = nullptr;

    QVector<KeyCaptureButton*> m_keyButtons;
    QLabel* m_conflictLabel = nullptr;

    QLineEdit* m_dataEdit = nullptr;
    QLabel* m_dataState = nullptr;
    QComboBox* m_language = nullptr;
    QLabel* m_languageState = nullptr;

    QTableWidget* m_advanced = nullptr;

    QPushButton* m_saveButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QLabel* m_firstRunBanner = nullptr;
    bool m_firstRun = false;
    QLabel* m_pathLabel = nullptr;
    QString m_pathText;

    bool m_loading = false;
    bool m_rememberGeometry = false;
    bool m_fitting = false;
    int m_resizes = 0;
};
