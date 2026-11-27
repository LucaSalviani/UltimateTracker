#include "ui/AppUsageDialog.h"

#include "core/Database.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QToolTip>
#include <QVBoxLayout>

namespace {
const QColor kPalette[] = {{0x4f, 0x9d, 0xde}, {0xe0, 0x8a, 0x3c}, {0x6c, 0xc0, 0x7a}, {0xb0, 0x7c, 0xd6},
                           {0xd9, 0x5f, 0x7a}, {0xd4, 0xc0, 0x4a}, {0x4f, 0xc4, 0xc4}};
const QColor kOther{0x7a, 0x7a, 0x7a};
constexpr int kMaxSlices = 7;
}

// Torta + leyenda. Los programas más usados con color; el resto junto en «Otros».
class AppUsageDialog::Pie : public QWidget {
public:
    struct Slice { QString name; qint64 sec = 0; QColor color; QStringList detail; };
    Pie(const QString& heading, QWidget* parent) : QWidget(parent), m_heading(heading)
    {
        setMinimumSize(300, 330);
        setMouseTracking(true);
    }
    void setData(const QVector<Slice>& s)
    {
        m_slices = s;
        m_total = 0;
        for (const Slice& x : s) m_total += x.sec;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(theme::text);
        QFont f = font();
        f.setPixelSize(13);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, width(), 22), Qt::AlignLeft | Qt::AlignVCenter, m_heading);
        f.setBold(false);
        f.setPixelSize(12);
        p.setFont(f);
        p.setPen(theme::textDim);
        p.drawText(QRect(0, 22, width(), 18), Qt::AlignLeft | Qt::AlignVCenter, m_total > 0 ? fmt::hours(m_total) : QStringLiteral("Sin datos"));
        const QRect pie(0, 48, 150, 150);
        m_zones.clear();
        if (m_total <= 0) {
            p.setPen(QPen(theme::line, 14));
            p.drawArc(pie.adjusted(7, 7, -7, -7), 0, 360 * 16);
            return;
        }
        double start = 90.0;
        for (const Slice& s : m_slices) {
            const double span = 360.0 * s.sec / m_total;
            p.setPen(QPen(theme::bg, 2));
            p.setBrush(s.color);
            p.drawPie(pie, int(start * 16), int(-span * 16));
            start -= span;
        }
        // agujero: dona
        p.setPen(Qt::NoPen);
        p.setBrush(theme::bg);
        p.drawEllipse(pie.center(), 40, 40);
        int y = 48;
        const int x = 166;
        for (int i = 0; i < m_slices.size(); ++i) {
            const Slice& s = m_slices[i];
            const QRect row(x, y, width() - x, 22);
            m_zones.append({row, i});
            p.setPen(Qt::NoPen);
            p.setBrush(s.color);
            p.drawRoundedRect(QRect(x, y + 6, 10, 10), 2, 2);
            p.setPen(theme::text);
            const QString pct = QStringLiteral("%1 %").arg(qRound(100.0 * s.sec / m_total));
            const QString txt = QFontMetrics(f).elidedText(s.name, Qt::ElideRight, width() - x - 90);
            p.drawText(QRect(x + 16, y, width() - x - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, txt);
            p.setPen(theme::textDim);
            p.drawText(QRect(x, y, width() - x, 22), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1 · %2").arg(fmt::hours(s.sec), pct));
            y += 24;
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        for (const auto& z : m_zones) {
            if (z.first.contains(e->pos())) {
                const Slice& s = m_slices[z.second];
                QString t = QStringLiteral("<b>%1</b> · %2").arg(s.name.toHtmlEscaped(), fmt::hours(s.sec));
                for (const QString& d : s.detail) t += QStringLiteral("<br>%1").arg(d.toHtmlEscaped());
                QToolTip::showText(e->globalPosition().toPoint(), t, this);
                setCursor(Qt::PointingHandCursor);
                return;
            }
        }
        unsetCursor();
        QToolTip::hideText();
    }
private:
    QString m_heading;
    QVector<Slice> m_slices;
    qint64 m_total = 0;
    QVector<QPair<QRect, int>> m_zones;
};

AppUsageDialog::AppUsageDialog(Database* db, const QDate& today, QWidget* parent)
    : QDialog(parent), m_db(db), m_today(today), m_anchor(today)
{
    setWindowTitle(QStringLiteral("Uso de apps"));
    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(10);

    auto* top = new QHBoxLayout;
    const char* names[] = {"Día", "Semana", "Mes"};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(QString::fromUtf8(names[i]), this);
        b->setCheckable(true);
        b->setAutoDefault(false);
        b->setObjectName("GreetLater");
        connect(b, &QPushButton::clicked, this, [this, i] { m_mode = i; reload(); });
        m_modeBtns.append(b);
        top->addWidget(b);
    }
    top->addStretch();
    auto* prev = new QPushButton(QStringLiteral("<"), this);
    auto* next = new QPushButton(QStringLiteral(">"), this);
    for (auto* b : {prev, next}) { b->setAutoDefault(false); b->setObjectName("GreetLater"); b->setFixedWidth(44); }
    connect(prev, &QPushButton::clicked, this, [this] {
        m_anchor = m_mode == 0 ? m_anchor.addDays(-1) : m_mode == 1 ? m_anchor.addDays(-7) : m_anchor.addMonths(-1);
        reload();
    });
    connect(next, &QPushButton::clicked, this, [this] {
        m_anchor = m_mode == 0 ? m_anchor.addDays(1) : m_anchor.addDays(m_mode == 1 ? 7 : 0);
        if (m_mode == 2) m_anchor = m_anchor.addMonths(1);
        if (m_anchor > m_today) m_anchor = m_today;
        reload();
    });
    m_title = new QLabel(this);
    top->addWidget(prev);
    top->addWidget(m_title);
    top->addWidget(next);
    lay->addLayout(top);

    auto* pies = new QHBoxLayout;
    pies->setSpacing(24);
    m_pomo = new Pie(QStringLiteral("Con un pomodoro corriendo"), this);
    m_out = new Pie(QStringLiteral("Fuera de un pomodoro"), this);
    pies->addWidget(m_pomo);
    pies->addWidget(m_out);
    lay->addLayout(pies);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color:%1").arg(theme::textDim.name()));
    lay->addWidget(m_note);
    resize(760, 460);
    reload();
}

QPair<QDate, QDate> AppUsageDialog::range() const
{
    if (m_mode == 0) return {m_anchor, m_anchor};
    if (m_mode == 1) { const QDate mon = fmt::mondayOf(m_anchor); return {mon, mon.addDays(6)}; }
    const QDate first(m_anchor.year(), m_anchor.month(), 1);
    return {first, first.addMonths(1).addDays(-1)};
}

void AppUsageDialog::reload()
{
    for (int i = 0; i < m_modeBtns.size(); ++i) m_modeBtns[i]->setChecked(i == m_mode);
    const auto [from, to] = range();
    m_title->setText(m_mode == 0 ? fmt::longDate(from)
                                 : m_mode == 1 ? QStringLiteral("%1 – %2").arg(fmt::shortDate(from), fmt::shortDate(to))
                                               : from.toString(QStringLiteral("MMMM yyyy")));
    const auto rows = appusage::byApp(*m_db, from, to);
    auto build = [&](bool pomo) {
        QVector<Pie::Slice> out;
        QVector<QPair<QString, qint64>> list;
        for (const auto& r : rows) if (const qint64 s = pomo ? r.pomoSec : r.outSec; s > 0) list.append({r.exe, s});
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        qint64 rest = 0;
        for (int i = 0; i < list.size(); ++i) {
            if (i < kMaxSlices - 1 || list.size() == kMaxSlices) {
                Pie::Slice s{list[i].first, list[i].second, kPalette[i % 7], {}};
                for (const auto& t : appusage::titles(*m_db, from, to, list[i].first, 5))
                    s.detail << QStringLiteral("%1 — %2").arg(t.first.left(60), fmt::hours(t.second));
                out.append(s);
            } else rest += list[i].second;
        }
        if (rest > 0) out.append({QStringLiteral("Otros"), rest, kOther, {}});
        return out;
    };
    m_pomo->setData(build(true));
    m_out->setData(build(false));
    m_note->setText(appusage::any(*m_db)
                        ? QStringLiteral("Cuenta la ventana que estaba al frente mientras tocabas la compu (3 min sin teclado ni mouse no suman) y solo con UltimateTracker abierto, aunque esté en segundo plano. Se guarda solo en esta PC. Pasá el mouse por un programa para ver sus ventanas.")
                        : QStringLiteral("Todavía no hay datos. Prendé el seguimiento en Ajustes › Sistema › Uso de apps."));
}
