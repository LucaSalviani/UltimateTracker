#include "core/Kinds.h"

namespace kinds {

QVector<Kind> builtin()
{
    return {
        {"estudio",  "Estudio",  "estudio",  "estudiado",  "estudiadas",  "estudiás",  "estudiaste",  "estudiar"},
        {"trabajo",  "Trabajo",  "trabajo",  "trabajado",  "trabajadas",  "trabajás",  "trabajaste",  "trabajar"},
        {"lectura",  "Lectura",  "lectura",  "leído",      "leídas",      "leés",      "leíste",      "leer"},
        {"practica", "Práctica", "práctica", "practicado", "practicadas", "practicás", "practicaste", "practicar"},
    };
}

Kind neutral()
{
    return {"", "Foco", "foco", "de foco", "de foco", "hacés un pomodoro", "hiciste", "hacer pomodoros"};
}

QString customId(const QString& name, const QString& participle)
{
    QString n = name.trimmed(), p = participle.trimmed().toLower();
    n.remove('|'); p.remove('|');
    return n.isEmpty() ? QString() : QStringLiteral("otro:%1|%2").arg(n, p);
}

Kind byId(const QString& id)
{
    if (id.isEmpty()) return builtin().first();
    for (const Kind& k : builtin()) if (k.id == id) return k;
    if (id.startsWith(QLatin1String("otro:"))) {
        const QStringList parts = id.mid(5).split('|');
        Kind k = neutral();
        k.id = id;
        k.name = parts.value(0);
        k.noun = k.name.toLower();
        if (!parts.value(1).isEmpty()) {
            k.done = parts.value(1);                 // "entrenado"
            k.doneFem = k.done.endsWith('o') ? k.done.left(k.done.size() - 1) + QStringLiteral("as") : k.done;
        } else {
            k.done = k.doneFem = QStringLiteral("de %1").arg(k.noun);
        }
        return k;
    }
    return builtin().first();
}

QString idOf(const FolderTree& tree, qint64 folderId)
{
    for (qint64 id : tree.ancestorsAndSelf(folderId)) {
        const Folder* f = tree.find(id);
        if (f && !f->kind.isEmpty()) return f->kind;
    }
    return QStringLiteral("estudio");
}

Kind of(const FolderTree& tree, qint64 folderId) { return byId(idOf(tree, folderId)); }

Kind common(const FolderTree& tree, const QSet<qint64>& folderIds)
{
    QString id;
    bool first = true;
    for (qint64 f : folderIds) {
        const QString k = idOf(tree, f);
        if (first) { id = k; first = false; }
        else if (k != id) return neutral();
    }
    return first ? general(tree) : byId(id);
}

Kind general(const FolderTree& tree)
{
    QSet<qint64> all;
    for (const Folder& f : tree.all()) all.insert(f.id);
    if (all.isEmpty()) return builtin().first();
    return common(tree, all);
}

} // namespace kinds
