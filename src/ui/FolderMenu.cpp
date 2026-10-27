#include "ui/FolderMenu.h"

FolderMenu::FolderMenu(QWidget* parent)
    : QMenu(parent)
{
}

QString FolderMenu::label(const Folder* f) const
{
    const QString b = m_badges.value(f->id);
    return b.isEmpty() ? f->name : f->name + QStringLiteral("    ") + b;
}

void FolderMenu::rebuild(const FolderTree& tree, qint64 current, const QHash<qint64, QString>& badges)
{
    m_badges = badges;
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
        } else {
            QMenu* sub = menu->addMenu(f->name);
            QAction* self = sub->addAction(label(f) + QStringLiteral("  (general)"), this, choose);
            self->setCheckable(true);
            self->setChecked(id == current);
            sub->addSeparator();
            addLevel(sub, tree, id, current);
        }
    }
}
