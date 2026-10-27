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
#include <QInputDialog>
#include <QPainter>
#include <QColorDialog>
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
    const auto prior = m_db->priorByFolder();
    const auto colors = colorsFor(m_db, tree);

    std::function<void(QTreeWidgetItem*, qint64)> add = [&](QTreeWidgetItem* parentItem, qint64 parentId) {
        for (qint64 id : tree.children(parentId)) {
            auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
            item->setText(0, tree.find(id)->name);
            if (colors.contains(id)) {   // puntito del color de la actividad
                QPixmap dot(12, 12);
                dot.fill(Qt::transparent);
                QPainter g(&dot);
                g.setRenderHint(QPainter::Antialiasing);
                g.setPen(Qt::NoPen);
                g.setBrush(colors.value(id));
                g.drawEllipse(QRectF(2, 2, 8, 8));
                g.end();
                item->setIcon(0, QIcon(dot));
            }
            item->setText(1, fmt::hours(totals.value(id)));
            item->setForeground(1, theme::textDim);
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            if (prior.value(id) > 0)
                item->setToolTip(1, QStringLiteral("Incluye %1 previas").arg(fmt::hours(prior.value(id))));
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

// M11: horas que ya tenías antes de la app (por ejemplo, las de la hoja en la pared).
void ActivitiesWidget::editPriorHours(QTreeWidgetItem* item)
{
    const qint64 id = item->data(0, kIdRole).toLongLong();
    const qint64 cur = m_db->priorByFolder().value(id);
    QInputDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Horas previas"));
    dlg.setLabelText(QStringLiteral("¿Cuántas horas de \"%1\" tenías antes de usar la app?\n"
                                    "Se suman a tu total y a los hitos (no aparecen en los días).")
                         .arg(item->text(0)));
    dlg.setInputMode(QInputDialog::DoubleInput);
    dlg.setDoubleRange(0, 10000);
    dlg.setDoubleDecimals(1);
    dlg.setDoubleStep(0.5);
    dlg.setDoubleValue(cur / 3600.0);
    if (dlg.exec() != QDialog::Accepted) return;
    m_db->setFolderPriorSeconds(id, qint64(dlg.doubleValue() * 3600));
    refresh(id);
    emit activitiesChanged();
}

// ---------------------------------------------------------------------------
// M25: colores de las actividades
// ---------------------------------------------------------------------------
const QVector<QColor>& ActivitiesWidget::autoColors()
{
    static const QVector<QColor> c = {
        {0x6f, 0x9b, 0xd1}, {0xd1, 0x8f, 0x6f}, {0x8f, 0xc2, 0x8a}, {0xc7, 0x9b, 0xd6},
        {0xd6, 0xc0, 0x6f}, {0x6f, 0xc2, 0xbd}, {0xd1, 0x6f, 0x8f}, {0xa8, 0xa8, 0xa8}};
    return c;
}

QHash<qint64, QColor> ActivitiesWidget::colorsFor(Database* db, const FolderTree& tree)
{
    QHash<qint64, QColor> out;
    if (!db->setting("activity_colors", 1).toBool()) return out;   // apagados en Ajustes
    const auto chosen = db->folderColors();
    int i = 0;
    for (const Folder& f : tree.all()) {
        const QColor c(chosen.value(f.id));
        out.insert(f.id, c.isValid() ? c : autoColors()[i % autoColors().size()]);
        ++i;
    }
    return out;
}

void ActivitiesWidget::fillColorMenu(QMenu* menu, qint64 id)
{
    auto swatch = [](const QColor& c) {
        QPixmap pm(14, 14);
        pm.fill(Qt::transparent);
        QPainter g(&pm);
        g.setRenderHint(QPainter::Antialiasing);
        g.setPen(Qt::NoPen);
        g.setBrush(c);
        g.drawEllipse(QRectF(1, 1, 12, 12));
        return QIcon(pm);
    };
    const QString cur = m_db->folderColors().value(id);
    QAction* autoAct = menu->addAction(QStringLiteral("Automático"), this, [this, id] {
        m_db->setFolderColor(id, QString());
        refresh(id);
        emit activitiesChanged();
    });
    autoAct->setCheckable(true);
    autoAct->setChecked(cur.isEmpty());
    menu->addSeparator();
    const QVector<QColor> more = {{0xe0, 0x48, 0x3f}, {0xff, 0x8a, 0x00}, {0x2f, 0x6f, 0xe8}, {0x22, 0xa0, 0x6b}};
    for (const QColor& c : autoColors() + more) {
        QAction* a = menu->addAction(swatch(c), c.name(), this, [this, id, c] {
            m_db->setFolderColor(id, c.name());
            refresh(id);
            emit activitiesChanged();
        });
        a->setCheckable(true);
        a->setChecked(cur.compare(c.name(), Qt::CaseInsensitive) == 0);
    }
    menu->addSeparator();
    menu->addAction(QStringLiteral("Elegir otro…"), this, [this, id, cur] {
        const QColor c = QColorDialog::getColor(QColor(cur.isEmpty() ? "#6f9bd1" : cur), this,
                                                QStringLiteral("Color de la actividad"));
        if (!c.isValid()) return;
        m_db->setFolderColor(id, c.name());
        refresh(id);
        emit activitiesChanged();
    });
}

void ActivitiesWidget::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    if (item) {
        m_tree->setCurrentItem(item);
        menu.addAction(QStringLiteral("Agregar subactividad"), this, [this, item] { addChild(item); });
        menu.addAction(QStringLiteral("Renombrar"), this, [this, item] { m_tree->editItem(item, 0); });
        menu.addAction(QStringLiteral("Horas previas…"), this, [this, item] { editPriorHours(item); });
        auto* colorMenu = menu.addMenu(QStringLiteral("Color"));
        fillColorMenu(colorMenu, item->data(0, kIdRole).toLongLong());
        menu.addSeparator();
        menu.addAction(QStringLiteral("Eliminar…"), this, [this, item] { deleteItem(item); });
    } else {
        menu.addAction(QStringLiteral("Nueva actividad"), this, [this] { m_newEdit->setFocus(); });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

#include "ActivitiesWidget.moc"
