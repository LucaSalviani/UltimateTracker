#include "ui/StatsWidget.h"
#include "core/FolderTree.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

// ===========================================================================
// Lienzo: todo lo que se dibuja en la página de estadísticas
// ===========================================================================
struct StatsData {
    StatsWidget::Period period = StatsWidget::Period::Week;
    QDate from, to, today;
    stats::Totals totals;
    QMap<QDate, qint64> byDay;
    QVector<Pomodoro> pomos;       // para la línea del día
    QVector<Interval> intervals;   // idem
    const QSet<qint64>* filter = nullptr;
    QSet<qint64> filterSet;
    stats::Streak streak;
    int jokers = 2;
    struct Row { QString name; int depth = 0; qint64 period = 0; qint64 total = 0; };
    QVector<Row> rows;
};

class StatsCanvas : public QWidget {
public:
    explicit StatsCanvas(QWidget* parent) : QWidget(parent) {}
    StatsData data;

    void relayout()
    {
        setMinimumHeight(contentHeight(width()));
        update();
    }

protected:
    void resizeEvent(QResizeEvent*) override { setMinimumHeight(contentHeight(width())); }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), theme::bg);
        const double L = 16, W = width() - 32;
        double y = 8;

        y = paintCards(p, L, y, W) + 10;
        y = paintStreak(p, L, y, W) + 18;
        y = paintChart(p, L, y, W) + 22;
        paintFolders(p, L, y, W);
    }

private:
    static constexpr double kCardH = 70;
    static constexpr double kRowH = 26;

    double chartHeight(double W) const
    {
        switch (data.period) {
        case StatsWidget::Period::Day:  return 24 + 66;
        case StatsWidget::Period::Year: return 24 + 7 * (heatCell(W) + 3) + 18;
        default:                        return 24 + 150;
        }
    }
    static double heatCell(double W) { return qBound(6.0, (W - 30) / 53.0 - 3, 14.0); }

    int contentHeight(int w) const
    {
        const double W = w - 32;
        return int(8 + kCardH + 10 + 20 + 18 + chartHeight(W) + 22 + 24 + data.rows.size() * kRowH + 24);
    }

    void sectionTitle(QPainter& p, double x, double y, const QString& t) const
    {
        p.setFont(theme::uiFont(9, QFont::DemiBold));
        p.setPen(theme::textDim);
        p.drawText(QPointF(x, y + 13), t.toUpper());
    }

    double paintCards(QPainter& p, double L, double y, double W)
    {
        struct Card { QString label; qint64 sec; QColor accent; };
        const Card cards[4] = {
            {QStringLiteral("Estudio"), data.totals.study, theme::text},
            {QStringLiteral("Recreos"), data.totals.breaks, theme::text},
            {QStringLiteral("Recreos indebidos"), data.totals.undue,
             data.totals.undue > 0 ? theme::squareFinal.lighter(125) : theme::text},
            {QStringLiteral("Horas inactivas"), data.totals.inactive, theme::textDim},
        };
        const double gap = 10;
        const double cw = (W - 3 * gap) / 4;
        for (int i = 0; i < 4; ++i) {
            const QRectF r(L + i * (cw + gap), y, cw, kCardH);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(r, 8, 8);
            p.setFont(theme::uiFont(8.5));
            p.setPen(theme::textDim);
            p.drawText(r.adjusted(12, 10, -8, 0), Qt::AlignLeft | Qt::AlignTop,
                       QFontMetrics(p.font()).elidedText(cards[i].label, Qt::ElideRight, int(cw - 20)));
            p.setFont(theme::uiFont(16, QFont::DemiBold));
            p.setPen(cards[i].accent);
            p.drawText(r.adjusted(12, 0, -8, -10), Qt::AlignLeft | Qt::AlignBottom, fmt::hours(cards[i].sec));
        }
        return y + kCardH;
    }

    double paintStreak(QPainter& p, double L, double y, double W)
    {
        p.setFont(theme::uiFont(9.5));
        QString s = QStringLiteral("Racha: %1 %2")
                        .arg(data.streak.days)
                        .arg(data.streak.days == 1 ? QStringLiteral("día") : QStringLiteral("días"));
        s += QStringLiteral("   ·   comodines esta semana: %1 de %2").arg(data.streak.jokersThisWeek).arg(data.jokers);
        if (!data.streak.todayDone) s += QStringLiteral("   ·   hoy todavía no arrancaste");
        p.setPen(theme::text);
        p.drawText(QRectF(L, y, W, 20), Qt::AlignLeft | Qt::AlignVCenter, s);
        return y + 20;
    }

    double paintChart(QPainter& p, double L, double y, double W)
    {
        switch (data.period) {
        case StatsWidget::Period::Day:  return paintTimeline(p, L, y, W);
        case StatsWidget::Period::Year: return paintHeatmap(p, L, y, W);
        default:                        return paintBars(p, L, y, W);
        }
    }

    // Día: línea de 0 a 24 h con estudio, recreos e indebidos.
    double paintTimeline(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Tu día"));
        y += 24;
        const QRectF bar(L, y, W, 26);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(bar, 5, 5);

        const qint64 dayStart = QDateTime(data.from, QTime(0, 0)).toSecsSinceEpoch();
        auto xAt = [&](qint64 t) { return bar.left() + qBound(0.0, double(t - dayStart) / 86400.0, 1.0) * bar.width(); };
        auto block = [&](qint64 a, qint64 b, const QColor& c) {
            const double x0 = xAt(a), x1 = qMax(x0 + 1.5, xAt(b));
            p.setBrush(c);
            p.drawRect(QRectF(x0, bar.top() + 3, x1 - x0, bar.height() - 6));
        };
        for (const Interval& i : data.intervals)
            block(i.startedAt, i.startedAt + i.seconds,
                  i.kind == IntervalKind::Break ? QColor(0x55, 0x55, 0x55) : theme::squareFinal.darker(140));
        for (const Pomodoro& pm : data.pomos) {
            if (data.filter && !data.filter->contains(pm.folderId)) continue;
            block(pm.startedAt, qMax(pm.endedAt, pm.startedAt + pm.studiedSec), theme::squareFill);
        }

        p.setFont(theme::uiFont(8));
        p.setPen(theme::textFaint);
        for (int h = 0; h <= 24; h += 3) {
            const double x = bar.left() + bar.width() * h / 24.0;
            p.drawText(QRectF(x - 20, bar.bottom() + 4, 40, 14), Qt::AlignCenter, QStringLiteral("%1h").arg(h));
        }
        // Leyenda
        const QPair<QString, QColor> legend[3] = {
            {QStringLiteral("estudio"), theme::squareFill},
            {QStringLiteral("recreo"), QColor(0x55, 0x55, 0x55)},
            {QStringLiteral("indebido"), theme::squareFinal.darker(140)},
        };
        double lx = L;
        const double ly = bar.bottom() + 24;
        for (const auto& [name, c] : legend) {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(lx, ly + 3, 9, 9), 2, 2);
            p.setPen(theme::textDim);
            p.drawText(QPointF(lx + 14, ly + 11), name);
            lx += 26 + QFontMetrics(p.font()).horizontalAdvance(name);
        }
        return ly + 16;
    }

    // Semana / mes: barras de horas por día.
    double paintBars(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Horas por día"));
        y += 24;
        const int n = int(data.from.daysTo(data.to)) + 1;
        const double chartH = 128;
        const double labelRoom = 18;   // espacio para el número arriba de la barra
        const double slot = W / n;
        const double bw = qMax(3.0, slot * 0.62);
        qint64 maxV = 3600;
        for (auto v : data.byDay) maxV = qMax(maxV, v);

        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(L, y + chartH), QPointF(L + W, y + chartH));

        for (int i = 0; i < n; ++i) {
            const QDate d = data.from.addDays(i);
            const qint64 v = data.byDay.value(d);
            const double h = (chartH - labelRoom) * double(v) / maxV;
            const QRectF r(L + i * slot + (slot - bw) / 2, y + chartH - h, bw, h);
            p.setPen(Qt::NoPen);
            p.setBrush(d == data.today ? theme::text : theme::squareFill);
            if (v > 0) p.drawRoundedRect(r, 2, 2);

            p.setFont(theme::uiFont(8));
            if (v > 0 && slot > 30) {
                p.setPen(theme::textDim);
                p.drawText(QRectF(r.left() - 10, r.top() - 16, bw + 20, 14), Qt::AlignCenter, fmt::duration(v));
            }
            p.setPen(d == data.today ? theme::text : theme::textFaint);
            const QString lab = data.period == StatsWidget::Period::Week ? fmt::shortDate(d) : QString::number(d.day());
            if (data.period == StatsWidget::Period::Week || slot > 16 || d.day() % 2 == 1)
                p.drawText(QRectF(L + i * slot - 6, y + chartH + 4, slot + 12, 14), Qt::AlignCenter, lab);
        }
        return y + chartH + 22;
    }

    // Año: mapa de calor, una celda por día (columnas = semanas).
    double paintHeatmap(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Mapa de calor"));
        y += 24;
        const double cell = heatCell(W);
        const double step = cell + 3;
        const QDate start = fmt::mondayOf(data.from);
        const double left = L + 26;

        p.setFont(theme::uiFont(7.5));
        p.setPen(theme::textFaint);
        const char* rows[] = {"lun", "", "mié", "", "vie", "", ""};
        for (int r = 0; r < 7; ++r)
            p.drawText(QRectF(L, y + r * step, 24, cell), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(rows[r]));

        auto shade = [](qint64 sec) {
            const double h = sec / 3600.0;
            if (sec <= 0) return theme::surface;
            if (h < 1)    return QColor(0x45, 0x45, 0x45);
            if (h < 2.5)  return QColor(0x6a, 0x6a, 0x6a);
            if (h < 4.5)  return QColor(0x95, 0x95, 0x95);
            if (h < 7)    return QColor(0xc4, 0xc4, 0xc4);
            return QColor(0xf2, 0xf2, 0xf2);
        };

        int lastMonth = -1;
        for (int w = 0; w < 54; ++w) {
            for (int r = 0; r < 7; ++r) {
                const QDate d = start.addDays(w * 7 + r);
                if (d < data.from || d > data.to) continue;
                const QRectF c(left + w * step, y + r * step, cell, cell);
                p.setPen(d == data.today ? QPen(theme::squareFinal, 1.5) : Qt::NoPen);
                p.setBrush(d > data.today ? theme::bg.lighter(115) : shade(data.byDay.value(d)));
                p.drawRoundedRect(c, 2, 2);
                if (r == 0 || d.day() == 1) {
                    if (d.month() != lastMonth && d.day() <= 7) {
                        lastMonth = d.month();
                        p.setPen(theme::textFaint);
                        p.drawText(QPointF(left + w * step, y + 7 * step + 12),
                                   fmt::monthYear(d).left(3));
                    }
                }
            }
        }
        return y + 7 * step + 18;
    }

    void paintFolders(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Actividades"));
        p.setFont(theme::uiFont(8));
        p.setPen(theme::textFaint);
        const double cPeriod = L + W * 0.42, cTotal = L + W * 0.56, cBar = L + W * 0.70;
        p.drawText(QRectF(cPeriod - 60, y, 60 + 40, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("período"));
        p.drawText(QRectF(cTotal - 60, y, 60 + 40, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("total"));
        p.drawText(QRectF(cBar + 10, y, W, 16), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("camino a las 10.000 h"));
        y += 24;

        if (data.rows.isEmpty()) {
            p.setPen(theme::textFaint);
            p.setFont(theme::uiFont(9));
            p.drawText(QPointF(L, y + 14), QStringLiteral("Todavía no hay horas registradas."));
            return;
        }
        for (const auto& row : data.rows) {
            const QRectF r(L, y, W, kRowH);
            p.setFont(theme::uiFont(9.5, row.depth == 0 ? QFont::DemiBold : QFont::Normal));
            p.setPen(row.depth == 0 ? theme::text : theme::textDim);
            const double indent = row.depth * 16;
            p.drawText(QRectF(L + indent, y, cPeriod - 70 - L - indent, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(row.name, Qt::ElideRight, int(cPeriod - 70 - L - indent)));
            p.setFont(theme::uiFont(9.5));
            p.setPen(row.period > 0 ? theme::text : theme::textFaint);
            p.drawText(QRectF(cPeriod - 60, y, 100, kRowH), Qt::AlignRight | Qt::AlignVCenter, fmt::hours(row.period));
            p.setPen(theme::textDim);
            p.drawText(QRectF(cTotal - 60, y, 100, kRowH), Qt::AlignRight | Qt::AlignVCenter, fmt::hours(row.total));

            // Barra hacia 10.000 h + próximo aviso
            const double bx = cBar + 10, bw = L + W - bx - 136;
            const double frac = qBound(0.0, row.total / (10000.0 * 3600.0), 1.0);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(QRectF(bx, y + kRowH / 2 - 3, bw, 6), 3, 3);
            p.setBrush(theme::squareFill);
            p.drawRoundedRect(QRectF(bx, y + kRowH / 2 - 3, qMax(frac > 0 ? 3.0 : 0.0, bw * frac), 6), 3, 3);

            const qint64 h = row.total / 3600;
            const qint64 next = h < 10 ? h + 1 : (h / 25 + 1) * 25;
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textFaint);
            p.drawText(QRectF(bx + bw + 8, y, 130, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("%1 %  · próx. %2 h").arg(QLocale().toString(frac * 100, 'f', frac < 0.1 ? 2 : 1)).arg(next));
            y += kRowH;
        }
    }
};

// ===========================================================================
// StatsWidget
// ===========================================================================
StatsWidget::StatsWidget(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
    , m_anchor(QDate::currentDate())
{
    auto* back = new QToolButton(this);
    back->setIcon(icons::get(icons::Id::Back));
    back->setToolTip(QStringLiteral("Volver a la grilla"));
    connect(back, &QToolButton::clicked, this, &StatsWidget::backRequested);

    auto* title = new QLabel(QStringLiteral("Estadísticas"), this);
    title->setProperty("heading", true);

    m_periodGroup = new QButtonGroup(this);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(0);
    const QString names[] = {QStringLiteral("Día"), QStringLiteral("Semana"), QStringLiteral("Mes"), QStringLiteral("Año")};
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(names[i], this);
        b->setCheckable(true);
        b->setProperty("segment", i == 0 ? "first" : (i == 3 ? "last" : "mid"));
        m_periodGroup->addButton(b, i);
        seg->addWidget(b);
    }
    m_periodGroup->button(int(m_period))->setChecked(true);
    connect(m_periodGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_period = Period(id);
        refresh();
    });

    auto* prev = new QToolButton(this);
    prev->setIcon(icons::get(icons::Id::ChevronLeft));
    auto* next = new QToolButton(this);
    next->setIcon(icons::get(icons::Id::ChevronRight));
    connect(prev, &QToolButton::clicked, this, [this] { shift(-1); });
    connect(next, &QToolButton::clicked, this, [this] { shift(+1); });
    m_label = new QLabel(this);
    m_label->setMinimumWidth(170);
    m_label->setAlignment(Qt::AlignCenter);

    m_folder = new QComboBox(this);
    m_folder->setMinimumWidth(180);
    connect(m_folder, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });

    auto* top = new QHBoxLayout;
    top->setContentsMargins(10, 8, 12, 4);
    top->addWidget(back);
    top->addWidget(title);
    top->addSpacing(14);
    top->addLayout(seg);
    top->addStretch();
    top->addWidget(prev);
    top->addWidget(m_label);
    top->addWidget(next);
    top->addSpacing(10);
    top->addWidget(m_folder);

    m_canvas = new StatsCanvas(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_canvas);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addLayout(top);
    lay->addWidget(scroll, 1);
}

void StatsWidget::resetToToday()
{
    m_anchor = QDate::currentDate();
}

QDate StatsWidget::periodStart() const
{
    switch (m_period) {
    case Period::Day:   return m_anchor;
    case Period::Week:  return fmt::mondayOf(m_anchor);
    case Period::Month: return QDate(m_anchor.year(), m_anchor.month(), 1);
    case Period::Year:  return QDate(m_anchor.year(), 1, 1);
    }
    return m_anchor;
}

QDate StatsWidget::periodEnd() const
{
    switch (m_period) {
    case Period::Day:   return m_anchor;
    case Period::Week:  return fmt::mondayOf(m_anchor).addDays(6);
    case Period::Month: return QDate(m_anchor.year(), m_anchor.month(), m_anchor.daysInMonth());
    case Period::Year:  return QDate(m_anchor.year(), 12, 31);
    }
    return m_anchor;
}

QString StatsWidget::periodLabel() const
{
    switch (m_period) {
    case Period::Day:   return fmt::longDate(m_anchor);
    case Period::Week:  return fmt::weekRange(fmt::mondayOf(m_anchor));
    case Period::Month: return fmt::monthYear(m_anchor);
    case Period::Year:  return QString::number(m_anchor.year());
    }
    return {};
}

void StatsWidget::shift(int dir)
{
    switch (m_period) {
    case Period::Day:   m_anchor = m_anchor.addDays(dir); break;
    case Period::Week:  m_anchor = m_anchor.addDays(7 * dir); break;
    case Period::Month: m_anchor = m_anchor.addMonths(dir); break;
    case Period::Year:  m_anchor = m_anchor.addYears(dir); break;
    }
    refresh();
}

void StatsWidget::rebuildFolderCombo()
{
    const qint64 current = m_folder->currentData().toLongLong();
    const FolderTree tree(m_db->folders());
    QSignalBlocker block(m_folder);
    m_folder->clear();
    m_folder->addItem(QStringLiteral("Todas las actividades"), qint64(0));
    std::function<void(qint64, int)> add = [&](qint64 parent, int depth) {
        for (qint64 id : tree.children(parent)) {
            m_folder->addItem(QString(depth * 3, QChar(' ')) + tree.find(id)->name, id);
            add(id, depth + 1);
        }
    };
    add(0, 0);
    const int idx = m_folder->findData(current);
    m_folder->setCurrentIndex(idx >= 0 ? idx : 0);
}

void StatsWidget::refresh()
{
    rebuildFolderCombo();
    m_periodGroup->button(int(m_period))->setChecked(true);
    m_label->setText(periodLabel());

    const QDateTime now = QDateTime::currentDateTime();
    const FolderTree tree(m_db->folders());
    const qint64 filterId = m_folder->currentData().toLongLong();

    StatsData& d = m_canvas->data;
    d = StatsData{};
    d.period = m_period;
    d.from = periodStart();
    d.to = periodEnd();
    d.today = now.date();
    d.jokers = m_jokers;
    if (filterId) {
        d.filterSet = tree.subtree(filterId);
        d.filter = &d.filterSet;
    }
    d.totals = stats::totals(*m_db, d.from, d.to, d.filter, now);
    d.byDay = stats::studyByDay(*m_db, d.from, d.to, d.filter);
    if (m_period == Period::Day) {
        d.pomos = m_db->pomodorosBetween(d.from, d.to);
        d.intervals = m_db->intervalsBetween(d.from, d.to);
    }
    d.streak = stats::streak(m_db->studiedDays(), d.today, m_jokers);

    const auto periodTotals = tree.rollUp(m_db->studyByFolder(d.from, d.to));
    const auto allTotals = tree.rollUp(m_db->studyByFolder());
    std::function<void(qint64, int)> addRows = [&](qint64 parent, int depth) {
        for (qint64 id : tree.children(parent)) {
            d.rows.append({tree.find(id)->name, depth, periodTotals.value(id), allTotals.value(id)});
            addRows(id, depth + 1);
        }
    };
    if (filterId) {
        d.rows.append({tree.find(filterId)->name, 0, periodTotals.value(filterId), allTotals.value(filterId)});
        addRows(filterId, 1);
    } else {
        addRows(0, 0);
        const qint64 orphanAll = m_db->studyByFolder().value(0);
        if (orphanAll > 0)
            d.rows.append({QStringLiteral("Sin actividad"), 0, m_db->studyByFolder(d.from, d.to).value(0), orphanAll});
    }
    m_canvas->relayout();
}
