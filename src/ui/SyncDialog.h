#pragma once

#include <QDialog>

class SyncClient;
class QLabel;
class QLineEdit;
class QPushButton;

// Celu-4: cuenta para sincronizar la PC con el celu (la misma en los dos).
class SyncDialog : public QDialog {
    Q_OBJECT
public:
    SyncDialog(SyncClient* sync, QWidget* parent = nullptr);

private:
    void refresh();

    SyncClient*  m_sync;
    QLabel*      m_info;
    QLabel*      m_note;
    QLineEdit*   m_email;
    QLineEdit*   m_pass;
    QPushButton* m_signUp;
    QPushButton* m_signIn;
    QPushButton* m_syncNow;
    QPushButton* m_signOut;
};
