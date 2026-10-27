#pragma once

#include "core/Database.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QStackedWidget;

// Ajustes por secciones. Lee y guarda directo en la base (tabla settings);
// cuando se acepta, MainWindow vuelve a leer lo que necesita.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(Database* db, QWidget* parent = nullptr);

    // M15: tema guardado (color + modo) → clave de paleta.
    static QString themeKey(Database* db);
    static void currentTheme(Database* db, QString* color, QString* mode);

    void accept() override;
    void reject() override;

private:
    QWidget* pageGeneral();
    QWidget* pageTimer();
    QWidget* pageAppearance();
    QWidget* pageSystem();

    Database* m_db;
    QString   m_originalTheme;

    QListWidget*    m_nav;
    QStackedWidget* m_pages;

    // General
    QSpinBox*  m_work;
    QSpinBox*  m_break;
    QSpinBox*  m_jokers;
    QComboBox* m_dateFormat;
    QSpinBox*  m_dayStart;
    QComboBox* m_goalInput;   // M17

    // Cronómetro
    QCheckBox* m_wheel;
    QSpinBox*  m_wheelStep;
    QCheckBox* m_type;
    QCheckBox* m_extraButtons;
    QSpinBox*  m_stepSmall;
    QSpinBox*  m_stepBig;

    // Apariencia
    QButtonGroup* m_themes;
    QButtonGroup* m_modes;
    QString       m_themeColor, m_themeMode;
    QComboBox*    m_size;
    QCheckBox*    m_resizable;
    QCheckBox*    m_groupByActivity;   // M8
    QCheckBox*    m_sizeModes[3];      // M16
    QCheckBox*    m_sizeButton;
    QComboBox*    m_sizeDefault;
    QCheckBox*    m_celebrations;      // M8

    // Sistema
    QCheckBox* m_autostart;
    QCheckBox* m_greeting;
    QCheckBox* m_hotkey;
    QCheckBox* m_quietFullscreen;
    QCheckBox* m_backup;
    QLineEdit* m_backupDir;
};
