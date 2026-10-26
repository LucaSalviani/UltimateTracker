#include "ui/FoldersDialog.h"
#include "core/FolderTree.h"
#include "core/Format.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

FoldersDialog::FoldersDialog(Database* db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    setWindowTitle(QStringLiteral("Carpetas"));
    resize(420, 420);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({QStringLiteral("Carpeta"), QStringLiteral("Horas")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setIndentation(16);
    m_tree->setRootIsDecorated(true);

    auto* addRoot = new QPushButton(QStringLiteral("Nueva carpeta"), this);
    m_addChild    = new QPushButton(QStringLiteral("Nueva subcarpeta"), this);
    m_rename      = new QPushButton(QStringLiteral("Renombrar"), this);
    m_delete      = new QPushButton(QStringLiteral("Eliminar"), this);
    auto* close   = new QPushButton(QStringLiteral("Listo"), this);
    close->setDefault(true);
    m_delete->setProperty("danger", true);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(addRoot);
    buttons->addWidget(m_addChild);
    buttons->addWidget(m_rename);
    buttons->addWidget(m_delete);

    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    bottom->addWidget(close);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(m_tree);
    lay->addLayout(buttons);
    lay->addLayout(bottom);

    connect(addRoot, &QPushButton::clicked, this, [this] { addFolder(false); });
    connect(m_addChild, &QPushButton::clicked, this, [this] { addFolder(true); });
    connect(m_rename, &QPushButton::clicked, this, &FoldersDialog::renameSelected);
    connect(m_delete, &QPushButton::clicked, this, &FoldersDialog::deleteSelected);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &FoldersDialog::renameSelected);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool has = selectedId() != 0;
        m_addChild->setEnabled(has);
        m_rename->setEnabled(has);
        m_delete->setEnabled(has);
    });

    reload();
}

void FoldersDialog::reload(qint64 selectId)
{
    m_tree->clear();
    const FolderTree tree(m_db->folders());
    const auto totals = tree.rollUp(m_db->studyByFolder());

    std::function<void(QTreeWidgetItem*, qint64)> add = [&](QTreeWidgetItem* parentItem, qint64 parentId) {
        for (qint64 id : tree.children(parentId)) {
            auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
            item->setText(0, tree.find(id)->name);
            item->setText(1, fmt::hours(totals.value(id)));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            item->setData(0, Qt::UserRole, id);
            if (id == selectId) m_tree->setCurrentItem(item);
            add(item, id);
        }
    };
    add(nullptr, 0);
    m_tree->expandAll();

    const bool has = selectedId() != 0;
    m_addChild->setEnabled(has);
    m_rename->setEnabled(has);
    m_delete->setEnabled(has);
}

qint64 FoldersDialog::selectedId() const
{
    const QTreeWidgetItem* it = m_tree->currentItem();
    return it && it->isSelected() ? it->data(0, Qt::UserRole).toLongLong() : 0;
}

void FoldersDialog::addFolder(bool asChild)
{
    const qint64 parent = asChild ? selectedId() : 0;
    if (asChild && !parent) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, asChild ? QStringLiteral("Nueva subcarpeta")
                                                             : QStringLiteral("Nueva carpeta"),
                                               QStringLiteral("Nombre:"), QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const qint64 id = m_db->addFolder(parent, name);
    reload(id);
    emit foldersChanged();
}

void FoldersDialog::renameSelected()
{
    const qint64 id = selectedId();
    if (!id) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Renombrar"), QStringLiteral("Nombre:"),
                                               QLineEdit::Normal, m_tree->currentItem()->text(0), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    m_db->renameFolder(id, name);
    reload(id);
    emit foldersChanged();
}

void FoldersDialog::deleteSelected()
{
    const qint64 id = selectedId();
    if (!id) return;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Eliminar carpeta"),
        QStringLiteral("¿Eliminar \"%1\" y todas sus subcarpetas?\n\n"
                       "Las horas no se pierden: quedan en el total general como \"Sin carpeta\".")
            .arg(m_tree->currentItem()->text(0)));
    if (answer != QMessageBox::Yes) return;
    m_db->deleteFolder(id);
    reload();
    emit foldersChanged();
}
