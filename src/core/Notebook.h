#pragma once

#include "core/Database.h"

#include <QString>
#include <QVector>

// Mejoras 6.0: la libreta, un anotador permanente en páginas. Aparte del diario (que es de un día):
// lo que escribís acá queda hasta que lo borres vos. Solo local por ahora; entra en la copia de seguridad.
namespace notebook {

struct Page {
    qint64  id = 0;
    QString title;
    QString body;
    bool    pinned = false;
    qint64  createdAt = 0, updatedAt = 0;   // segundos epoch
};

// Fijadas primero, después las tocadas más recientemente. `query` filtra por título o texto.
QVector<Page> pages(const Database& db, const QString& query = QString());
Page   get(const Database& db, qint64 id);
qint64 save(Database& db, const Page& p);   // id == 0 crea una página nueva
void   remove(Database& db, qint64 id);
QString exportText(const QVector<Page>& pages);   // todo el libro en texto plano

} // namespace notebook
