#include "ui/Libreta.h"

#include "core/Database.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTimer>
#include <QVBoxLayout>

Libreta::Libreta(Database* db, QWidget* parent) : QWidget(parent), m_db(db)
{
    m_search = new QLineEdit(this);
    m_search->setObjectName("NotePadText");
    m_search->setPlaceholderText(QStringLiteral("Buscar en la libreta…"));
    m_search->setClearButtonEnabled(true);

    m_list = new QListWidget(this);
    m_list->setObjectName("LibretaList");
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setMaximumHeight(124);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto mk = [this](const QString& t, const QString& tip) {
        auto* b = new QPushButton(t, this);
        b->setObjectName("LibretaBtn");
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };
    auto* add = mk(QStringLiteral("＋ Página"), QStringLiteral("Página nueva"));
    m_pin = mk(QStringLiteral("Fijar"), QStringLiteral("Fijar arriba / soltar"));
    auto* check = mk(QStringLiteral("☐"), QStringLiteral("Agregar una casilla (click en ella para tildar)"));
    auto* exp = mk(QStringLiteral("⤓"), QStringLiteral("Exportar toda la libreta a un archivo de texto"));
    auto* del = mk(QStringLiteral("Borrar"), QStringLiteral("Borrar esta página"));
    auto* tools = new QHBoxLayout;
    tools->setSpacing(4);
    for (QPushButton* b : {add, m_pin, check, exp, del}) tools->addWidget(b);
    tools->addStretch();

    m_title = new QLineEdit(this);
    m_title->setObjectName("NotePadText");
    m_title->setPlaceholderText(QStringLiteral("Título de la página"));
    m_body = new QPlainTextEdit(this);
    m_body->setObjectName("NotePadText");
    m_body->setPlaceholderText(QStringLiteral("Lo que quieras guardar para siempre: apuntes, ideas, listas…"));
    m_body->setTabChangesFocus(true);
    m_body->viewport()->installEventFilter(this);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(m_search);
    lay->addWidget(m_list);
    lay->addLayout(tools);
    lay->addWidget(m_title);
    lay->addWidget(m_body, 1);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(700);
    connect(m_timer, &QTimer::timeout, this, &Libreta::flush);
    auto dirty = [this] { if (!m_loading) { m_dirty = true; m_timer->start(); } };
    connect(m_title, &QLineEdit::textChanged, this, dirty);
    connect(m_body, &QPlainTextEdit::textChanged, this, dirty);
    connect(m_search, &QLineEdit::textChanged, this, [this] { flush(); reload(); });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it) { if (it && it->data(Qt::UserRole).toLongLong() != m_id) { flush(); select(it->data(Qt::UserRole).toLongLong()); } });
    connect(add, &QPushButton::clicked, this, &Libreta::newPage);
    connect(m_pin, &QPushButton::clicked, this, &Libreta::togglePin);
    connect(check, &QPushButton::clicked, this, &Libreta::insertCheckbox);
    connect(exp, &QPushButton::clicked, this, &Libreta::exportAll);
    connect(del, &QPushButton::clicked, this, &Libreta::removePage);
    reload();
}

void Libreta::reload()
{
    const auto pages = notebook::pages(*m_db, m_search->text());
    m_loading = true;
    m_list->clear();
    qint64 keep = 0;
    for (const notebook::Page& p : pages) {
        auto* it = new QListWidgetItem(QStringLiteral("%1%2  ·  %3").arg(p.pinned ? QStringLiteral("● ") : QString(),
                                                                         p.title.isEmpty() ? QStringLiteral("Sin título") : p.title,
                                                                         QDateTime::fromSecsSinceEpoch(p.updatedAt).toString(QStringLiteral("dd/MM"))));
        it->setData(Qt::UserRole, p.id);
        m_list->addItem(it);
        if (p.id == m_id) { m_list->setCurrentItem(it); keep = p.id; }
    }
    m_loading = false;
    if (!keep) {
        if (!pages.isEmpty()) { m_list->setCurrentRow(0); }
        else { m_id = 0; m_loading = true; m_title->clear(); m_body->clear(); m_loading = false; }
    }
}

void Libreta::select(qint64 id)
{
    const notebook::Page p = notebook::get(*m_db, id);
    m_loading = true;
    m_id = p.id;
    m_title->setText(p.title);
    m_body->setPlainText(p.body);
    m_pin->setText(p.pinned ? QStringLiteral("Soltar") : QStringLiteral("Fijar"));
    m_loading = false;
    m_dirty = false;
}

void Libreta::flush()
{
    m_timer->stop();
    if (!m_dirty || !m_id) return;
    m_dirty = false;
    notebook::Page p = notebook::get(*m_db, m_id);
    if (!p.id) return;
    if (p.title == m_title->text() && p.body == m_body->toPlainText()) return;
    p.title = m_title->text();
    p.body = m_body->toPlainText();
    notebook::save(*m_db, p);
    // El título cambió: la lista se refresca sin mover lo que estás escribiendo.
    if (QListWidgetItem* it = m_list->currentItem())
        it->setText(QStringLiteral("%1%2  ·  %3").arg(p.pinned ? QStringLiteral("● ") : QString(), p.title.isEmpty() ? QStringLiteral("Sin título") : p.title,
                                                       QDateTime::currentDateTime().toString(QStringLiteral("dd/MM"))));
}

void Libreta::newPage()
{
    flush();
    m_search->clear();
    notebook::Page p;
    p.title = QString();
    m_id = notebook::save(*m_db, p);
    reload();
    select(m_id);
    m_title->setFocus();
}

void Libreta::removePage()
{
    if (!m_id) return;
    if (QMessageBox::question(this, QStringLiteral("Borrar página"), QStringLiteral("¿Borrar esta página de la libreta? No se puede deshacer.")) != QMessageBox::Yes) return;
    notebook::remove(*m_db, m_id);
    m_id = 0;
    m_dirty = false;
    reload();
}

void Libreta::togglePin()
{
    if (!m_id) return;
    flush();
    notebook::Page p = notebook::get(*m_db, m_id);
    p.pinned = !p.pinned;
    notebook::save(*m_db, p);
    reload();
}

void Libreta::exportAll()
{
    flush();
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar la libreta"), QStringLiteral("libreta.txt"), QStringLiteral("Texto (*.txt)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(notebook::exportText(notebook::pages(*m_db)).toUtf8());
}

void Libreta::insertCheckbox()
{
    if (!m_id) newPage();
    QTextCursor c = m_body->textCursor();
    c.movePosition(QTextCursor::StartOfBlock);
    c.insertText(QStringLiteral("- [ ] "));
    m_body->setTextCursor(c);
    m_body->setFocus();
}

void Libreta::focusPage() { if (m_id) m_body->setFocus(); else m_search->setFocus(); }

// Click sobre «- [ ]» o «- [x]» lo tilda o destilda.
bool Libreta::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_body->viewport() && e->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton && !m_body->textCursor().hasSelection()) {
            QTextCursor c = m_body->cursorForPosition(me->position().toPoint());
            const QString t = c.block().text();
            const int col = c.positionInBlock();
            if ((t.startsWith(QLatin1String("- [ ] ")) || t.startsWith(QLatin1String("- [x] "))) && col <= 5) {
                QTextCursor w(c.block());
                w.setPosition(c.block().position() + 3);
                w.setPosition(c.block().position() + 4, QTextCursor::KeepAnchor);
                w.insertText(t.at(3) == QLatin1Char(' ') ? QStringLiteral("x") : QStringLiteral(" "));
            }
        }
    }
    return QWidget::eventFilter(o, e);
}
