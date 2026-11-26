#pragma once

#include "core/Notebook.h"

#include <QWidget>

class Database;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTimer;

// Mejoras 6.0: la libreta. Páginas permanentes (título + texto), con buscador, fijar,
// casillas clickeables («- [ ]» → «- [x]») y exportar. Se guarda sola.
class Libreta : public QWidget {
    Q_OBJECT
public:
    Libreta(Database* db, QWidget* parent = nullptr);
    void reload();
    void flush();
    void focusPage();

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void select(qint64 id);
    void newPage();
    void removePage();
    void togglePin();
    void exportAll();
    void insertCheckbox();

    Database*       m_db;
    QLineEdit*      m_search;
    QListWidget*    m_list;
    QLineEdit*      m_title;
    QPlainTextEdit* m_body;
    QPushButton*    m_pin;
    QTimer*         m_timer;
    qint64          m_id = 0;
    bool            m_dirty = false;
    bool            m_loading = false;
};
