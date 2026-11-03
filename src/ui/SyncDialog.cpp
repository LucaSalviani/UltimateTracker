#include "ui/SyncDialog.h"
#include "core/Sync.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SyncDialog::SyncDialog(SyncClient* sync, QWidget* parent)
    : QDialog(parent)
    , m_sync(sync)
{
    setWindowTitle(QStringLiteral("Sincronizar con el celu"));
    setMinimumWidth(380);
    auto* title = new QLabel(QStringLiteral("Sincronizar con el celu"), this);
    title->setProperty("heading", true);
    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    m_info->setProperty("dim", true);
    m_email = new QLineEdit(this);
    m_email->setPlaceholderText(QStringLiteral("Mail"));
    m_pass = new QLineEdit(this);
    m_pass->setPlaceholderText(QStringLiteral("Contraseña (6 o más letras)"));
    m_pass->setEchoMode(QLineEdit::Password);
    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_signUp = new QPushButton(QStringLiteral("Crear cuenta"), this);
    m_signIn = new QPushButton(QStringLiteral("Entrar"), this);
    m_signIn->setDefault(true);
    m_syncNow = new QPushButton(QStringLiteral("Sincronizar ahora"), this);
    m_signOut = new QPushButton(QStringLiteral("Cerrar sesión"), this);
    auto* close = new QPushButton(QStringLiteral("Cerrar"), this);

    connect(m_signUp, &QPushButton::clicked, this, [this] { m_note->clear(); m_sync->signUp(m_email->text(), m_pass->text()); });
    connect(m_signIn, &QPushButton::clicked, this, [this] { m_note->clear(); m_sync->signIn(m_email->text(), m_pass->text()); });
    connect(m_pass, &QLineEdit::returnPressed, m_signIn, &QPushButton::click);
    connect(m_syncNow, &QPushButton::clicked, m_sync, &SyncClient::syncNow);
    connect(m_signOut, &QPushButton::clicked, m_sync, &SyncClient::signOut);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_sync, &SyncClient::changed, this, &SyncDialog::refresh);
    connect(m_sync, &SyncClient::message, this, [this](const QString& t) { m_note->setText(t); });

    auto* row = new QHBoxLayout;
    row->addWidget(m_signUp);
    row->addWidget(m_signIn);
    row->addWidget(m_signOut);
    row->addWidget(m_syncNow);
    row->addStretch();
    row->addWidget(close);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);
    lay->addWidget(title);
    lay->addWidget(m_info);
    lay->addWidget(m_email);
    lay->addWidget(m_pass);
    lay->addWidget(m_note);
    lay->addLayout(row);
    refresh();
}

void SyncDialog::refresh()
{
    const bool in = m_sync->signedIn();
    m_info->setText(in ? QStringLiteral("Conectado como %1.\n%2").arg(m_sync->email(), m_sync->status())
                       : QStringLiteral("Usá la misma cuenta en la PC y en el celu: los pomodoros, actividades, objetivos "
                                        "y títulos de los días se juntan solos (cada minuto con la app abierta).\n"
                                        "Si es la primera vez, tocá «Crear cuenta» y confirmá el mail que te llega."));
    for (QWidget* w : {static_cast<QWidget*>(m_email), static_cast<QWidget*>(m_pass), static_cast<QWidget*>(m_signUp),
                       static_cast<QWidget*>(m_signIn)})
        w->setVisible(!in);
    m_syncNow->setVisible(in);
    m_signOut->setVisible(in);
    m_syncNow->setEnabled(!m_sync->busy());
    adjustSize();
}
