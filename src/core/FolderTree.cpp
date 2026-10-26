#include "core/FolderTree.h"

#include <QStringList>
#include <algorithm>

FolderTree::FolderTree(const QVector<Folder>& folders)
    : m_folders(folders)
{
    for (int i = 0; i < m_folders.size(); ++i)
        m_byId.insert(m_folders[i].id, i);
    for (const Folder& f : m_folders) {
        const qint64 parent = m_byId.contains(f.parentId) ? f.parentId : 0;
        m_children[parent].append(f.id);
    }
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        std::sort(it->begin(), it->end(), [this](qint64 a, qint64 b) {
            const Folder& fa = m_folders[m_byId[a]];
            const Folder& fb = m_folders[m_byId[b]];
            if (fa.sort != fb.sort) return fa.sort < fb.sort;
            return fa.name.localeAwareCompare(fb.name) < 0;
        });
    }
}

const Folder* FolderTree::find(qint64 id) const
{
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &m_folders[*it];
}

QVector<qint64> FolderTree::children(qint64 parentId) const
{
    return m_children.value(parentId);
}

QSet<qint64> FolderTree::subtree(qint64 id) const
{
    QSet<qint64> out;
    QVector<qint64> stack{id};
    while (!stack.isEmpty()) {
        const qint64 cur = stack.takeLast();
        if (out.contains(cur)) continue;
        out.insert(cur);
        stack += m_children.value(cur);
    }
    return out;
}

QVector<qint64> FolderTree::ancestorsAndSelf(qint64 id) const
{
    QVector<qint64> out;
    const Folder* f = find(id);
    while (f && !out.contains(f->id)) {
        out.append(f->id);
        f = find(f->parentId);
    }
    return out;
}

QString FolderTree::path(qint64 id, const QString& sep) const
{
    const QVector<qint64> chain = ancestorsAndSelf(id);
    if (chain.isEmpty()) return QStringLiteral("Sin carpeta");
    QStringList parts;
    for (auto it = chain.crbegin(); it != chain.crend(); ++it)
        parts << find(*it)->name;
    return parts.join(sep);
}

int FolderTree::depth(qint64 id) const
{
    return qMax(0, int(ancestorsAndSelf(id).size()) - 1);
}

QHash<qint64, qint64> FolderTree::rollUp(const QHash<qint64, qint64>& direct) const
{
    QHash<qint64, qint64> total;
    for (auto it = direct.cbegin(); it != direct.cend(); ++it) {
        for (qint64 a : ancestorsAndSelf(it.key()))
            total[a] += it.value();
    }
    return total;
}
