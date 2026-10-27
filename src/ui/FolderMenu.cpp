#include "ui/FolderMenu.h"

#include <QPainter>
#include <QPixmap>

FolderMenu::FolderMenu(QWidget* parent)
    : QMenu(parent)
{
}

QString FolderMenu::label(const Folder* f) const
{
    const QString b = m_badges.value(f->id);
    return b.isEmpty() ? f->name : f->name + QStringLiteral("    ") + b;
}

QIcon FolderMenu::dot(qint64 id, bool current) const
{
    if (!m_colors.contains(id)) return {};
    QPixmap pm(24, 24);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing);
    g.setPen(Qt::NoPen);
    g.setBrush(m_colors.value(id));
    g.drawEllipse(current ? QRectF(3, 3, 6, 6) : QRectF(2, 2, 8, 8));
    if (current) {   // la actual: aro alrededor (con ícono, Qt no dibuja el tilde)
        g.setPen(QPen(m_colors.value(id), 1.2));
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(QRectF(0.6, 0.6, 10.8, 10.8));
    }
    g.end();
    return QIcon(pm);
}

void FolderMenu::rebuild(const FolderTree& tree, qint64 current, const QHash<qint64, QString>& badges,
                         const QHash<qint64, QColor>& colors)
{
    m_badges = badges;
    m_colors = colors;
    clear();
    addLevel(this, tree, 0, current);
    addSeparator();
    addAction(QStringLiteral("Administrar actividades…"), this, &FolderMenu::manageRequested);
}

void FolderMenu::addLevel(QMenu* menu, const FolderTree& tree, qint64 parent, qint64 current)
{
    for (qint64 id : tree.children(parent)) {
        const Folder* f = tree.find(id);
        const auto kids = tree.children(id);
        auto choose = [this, id] { emit folderChosen(id); };
        if (kids.isEmpty()) {
            QAction* a = menu->addAction(label(f), this, choose);
            a->setCheckable(true);
            a->setChecked(id == current);
            a->setIcon(dot(id, id == current));
        } else {
            QMenu* sub = menu->addMenu(dot(id, id == current || tree.subtree(id).contains(current)), f->name);
            QAction* self = sub->addAction(label(f) + QStringLiteral("  (general)"), this, choose);
            self->setCheckable(true);
            self->setChecked(id == current);
            self->setIcon(dot(id, id == current));
            sub->addSeparator();
            addLevel(sub, tree, id, current);
        }
    }
}
