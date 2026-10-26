#include "ui/ActivitiesWidget.h"
#include "core/FolderTree.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

// Árbol que avisa cuando terminás de arrastrar algo.
class ActivityTree : public QTreeWidget {
    Q_OBJECT
public:
    using QTreeWidget::QTreeWidget;
signals:
    void structureChanged();
protected:
    void dropEvent(QDropEvent* e) override
    {
        QTreeWidget::dropEvent(e);
        emit structureChanged();
    }
};

namespace {
constexpr int kIdRole = Qt::UserRole;

int countDescendants(QTreeWidgetItem* item)
{
    int n = item->childCount();
    for (int i = 0; i < item->childCount(); ++i) n += countDescendants(item->child(i));
    return n;
}
} // namespace

ActivitiesWidget::ActivitiesWidget(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    auto* back = new QToolButton(this);
    back->setIcon(icons::get(icons::Id::Back));
    back->setToolTip(QStringLiteral("Volver"));
    back->setAutoRaise(true);
    connect(back, &QToolButton::clicked, this, &ActivitiesWidget::backRequested);

    auto* title = new QLabel(QStringLiteral("Actividades"), this);
    title->setProperty("heading", true);

    auto* top = new QHBoxLayout;
    top->setContentsMargins(10, 8, 12, 2);
    top->addWidget(back);
    top->addWidget(title);
    top->addStretch();

    m_tree = new ActivityTree(this);
    m_tree->setObjectName("Activities");
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setIndentation(18);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ActivitiesWidget::showContextMenu);
    connect(m_tree, &QTreeWidget::itemChanged, this, &ActivitiesWidget::onItemChanged);
    connect(m_tree, &ActivityTree::structureChanged, this, &ActivitiesWidget::persistStructure);

    m_newEdit = new QLineEdit(this);
    m_newEdit->setObjectName("NewActivity");
    m_newEdit->setPlaceholderText(QStringLiteral("+  Nueva actividad (Enter para agregar)"));
    connect(m_newEdit, &QLineEdit::returnPressed, this, &ActivitiesWidget::addRoot);

    auto* hint = new QLabel(QStringLiteral("Doble click para renombrar  ·  Arrastrá para ordenar o meter una "
                                           "dentro de otra  ·  Click derecho para más opciones"), this);
    hint->setProperty("dim", true);
    hint->setWordWrap(true);

    auto* body = new QVBoxLayout;
    body->setContentsMargins(14, 4, 14, 12);
    body->setSpacing(8);
    body->addWidget(m_tree, 1);
    body->addWidget(m_newEdit);
    body->addWidget(hint);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addLayout(top);
    lay->addLayout(body, 1);
}

void ActivitiesWidget::refresh(qint64 selectId)
{
    m_loading = true;
    m_tree->clear();
    const FolderTree tree(m_db->folders());
    const auto totals = tree.rollUp(m_db->studyByFolder());

    std::function<void(QTreeWidgetItem*, qint64)> add = [&](QTreeWidgetItem* parentItem, qint64 parentId) {
        for (qint64 id : tree.children(parentId)) {
            auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
            item->setText(0, tree.find(id)->name);
            item->setText(1, fmt::hours(totals.value(id)));
            item->setForeground(1, theme::textDim);
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            item->setData(0, kIdRole, id);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable |
                           Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            if (parentId == 0) {
                QFont f = item->font(0);
                f.setWeight(QFont::DemiBold);
                item->setFont(0, f);
            }
            if (id == selectId) m_tree->setCurrentItem(item);
            add(item, id);
        }
    };
    add(nullptr, 0);
    m_tree->expandAll();
    m_loading = false;
}

void ActivitiesWidget::addRoot()
{
    const QString name = m_newEdit->text().trimmed();
    if (name.isEmpty()) return;
    const qint64 id = m_db->addFolder(0, name);
    m_newEdit->clear();
    refresh(id);
    emit activitiesChanged();
}

void ActivitiesWidget::addChild(QTreeWidgetItem* parent)
{
    if (!parent) return;
    const qint64 parentId = parent->data(0, kIdRole).toLongLong();
    const qint64 id = m_db->addFolder(parentId, QStringLiteral("Nueva subactividad"));
    refresh(id);
    emit activitiesChanged();
    // Queda lista para escribirle el nombre.
    if (QTreeWidgetItem* it = m_tree->currentItem()) m_tree->editItem(it, 0);
}

void ActivitiesWidget::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_loading || column != 0) return;
    const qint64 id = item->data(0, kIdRole).toLongLong();
    const QString name = item->text(0).trimmed();
    if (name.isEmpty()) {
        refresh(id);   // nombre vacío: se deshace
        return;
    }
    m_db->renameFolder(id, name);
    emit activitiesChanged();
}

void ActivitiesWidget::deleteItem(QTreeWidgetItem* item)
{
    if (!item) return;
    const int subs = countDescendants(item);
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Eliminar actividad"));
    box.setText(subs == 0
        ? QStringLiteral("¿Eliminar «%1»?").arg(item->text(0))
        : QStringLiteral("¿Eliminar «%1» y sus %2 subactividades?").arg(item->text(0)).arg(subs));
    box.setInformativeText(QStringLiteral("Las horas no se pierden: siguen contando en el total y pasan a «Sin actividad»."));
    QPushButton* yes = box.addButton(QStringLiteral("Eliminar"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != yes) return;
    m_db->deleteFolder(item->data(0, kIdRole).toLongLong());
    refresh();
    emit activitiesChanged();
}

// Después de arrastrar: se guarda el padre y el orden de todas las actividades.
void ActivitiesWidget::persistStructure()
{
    std::function<void(QTreeWidgetItem*, qint64)> walk = [&](QTreeWidgetItem* parent, qint64 parentId) {
        const int n = parent ? parent->childCount() : m_tree->topLevelItemCount();
        for (int i = 0; i < n; ++i) {
            QTreeWidgetItem* it = parent ? parent->child(i) : m_tree->topLevelItem(i);
            const qint64 id = it->data(0, kIdRole).toLongLong();
            m_db->setFolderPlacement(id, parentId, i);
            walk(it, id);
        }
    };
    walk(nullptr, 0);
    const qint64 sel = m_tree->currentItem() ? m_tree->currentItem()->data(0, kIdRole).toLongLong() : 0;
    refresh(sel);
    emit activitiesChanged();
}

void ActivitiesWidget::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    if (item) {
        m_tree->setCurrentItem(item);
        menu.addAction(QStringLiteral("Agregar subactividad"), this, [this, item] { addChild(item); });
        menu.addAction(QStringLiteral("Renombrar"), this, [this, item] { m_tree->editItem(item, 0); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Eliminar…"), this, [this, item] { deleteItem(item); });
    } else {
        menu.addAction(QStringLiteral("Nueva actividad"), this, [this] { m_newEdit->setFocus(); });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

#include "ActivitiesWidget.moc"
