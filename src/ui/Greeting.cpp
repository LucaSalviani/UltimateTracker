#include "ui/Greeting.h"
#include "core/Database.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Goals.h"
#include "core/Stats.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Frases simples para arrancar (una al azar cada día).
const char* const kPhrases[] = {
    "Un pomodoro a la vez.",
    "Lo difícil es arrancar. Después fluye.",
    "Hoy suma, aunque sea poco.",
    "La constancia le gana a la intensidad.",
    "Las 10.000 horas empiezan con 25 minutos.",
    "No hacen falta ganas, hace falta empezar.",
    "El vos de mañana te lo va a agradecer.",
    "Poquito, pero todos los días.",
    "Primero lo más difícil.",
    "Celular lejos, cabeza cerca.",
    "Un cuadradito más en la pared.",
    "Hoy es un buen día para avanzar.",
    "Lo que se mide, mejora.",
    "Enfocate en el próximo pomodoro, nada más.",
    "La racha no se cuida sola.",
    "Despacio, pero sin parar.",
    "Hacelo aunque salga mal; después se mejora.",
    "Cada hora cuenta para el experto que vas a ser.",
    "Arrancá por cinco minutos y vemos.",
    "Menos pensar, más hacer.",
    "Elegí una cosa y terminala.",
    "El progreso es aburrido. Por eso funciona.",
};

QString hello()
{
    const int h = QTime::currentTime().hour();
    if (h >= 5 && h < 13) return QStringLiteral("Buen día");
    if (h >= 13 && h < 20) return QStringLiteral("Buenas tardes");
    return QStringLiteral("Buenas noches");
}

QDate lastStudied(Database& db, const QDate& today)
{
    QDate last;
    for (const QDate& d : db.studiedDays())
        if (d < today && (!last.isValid() || d > last)) last = d;
    return last;
}

QLabel* line(const QString& text, const char* name, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(name);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    return l;
}
} // namespace

bool Greeting::shouldShow(Database& db)
{
    if (!db.setting("greeting", 0).toBool()) return false;   // B2: apagado para la gente nueva
    return db.setting("greeting/last").toString() != fmt::dayKey(dayclock::today());
}

bool Greeting::summaryPending(Database& db)
{
    const QDate last = lastStudied(db, dayclock::today());
    if (!last.isValid()) return false;
    const QDate shown = QDate::fromString(db.setting("summary/last_shown").toString(), Qt::ISODate);
    return !shown.isValid() || shown < last;
}

Greeting::Greeting(Database* db, bool full, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    const QDate today = dayclock::today();
    if (full) m_db->setSetting("greeting/last", fmt::dayKey(today));

    m_card = new QWidget(this);
    m_card->setObjectName("GreetCard");
    m_card->setAttribute(Qt::WA_StyledBackground);

    // ---- Título (+ frase si es el saludo)
    const QDate last = lastStudied(*m_db, today);
    QString titleText;
    if (full) {
        titleText = QStringLiteral("%1, ¿listo para ser productivo?").arg(hello());
    } else {
        titleText = last == today.addDays(-1) ? QStringLiteral("Así te fue ayer")
                                              : QStringLiteral("Así te fue el %1").arg(fmt::longDate(last));
    }
    auto* title = line(titleText.toHtmlEscaped(), "GreetTitle", m_card);
    QLabel* quote = nullptr;
    if (full) {
        const int n = int(sizeof(kPhrases) / sizeof(kPhrases[0]));
        QRandomGenerator rng(quint32(today.toJulianDay()));   // la misma frase todo el día
        quote = line(QString::fromUtf8(kPhrases[rng.bounded(n)]).toHtmlEscaped(), "GreetPhrase", m_card);
    }

    // ---- El día anterior (lo que antes era el cartel de resumen)
    QStringList facts;
    if (last.isValid()) {
        const auto t = stats::totals(*m_db, last, last, nullptr, dayclock::dayStart(last).addSecs(86399));
        int pomos = 0;
        for (const Pomodoro& p : m_db->pomodorosBetween(last, last))
            if (p.status == PomoStatus::Completed || p.status == PomoStatus::Final) ++pomos;
        const QString when = last == today.addDays(-1) ? QStringLiteral("Ayer") : (full ? QStringLiteral("La última vez") : QString());
        QString s = (when.isEmpty() ? QString() : QStringLiteral("<b>%1:</b> ").arg(when)) +
                    QStringLiteral("%1 · %2 %3 · recreos %4").arg(fmt::hours(t.study)).arg(pomos)
                        .arg(pomos == 1 ? "pomodoro" : "pomodoros", fmt::hours(t.breaks));
        if (t.undue > 0) s += QStringLiteral(" · indebidos %1").arg(fmt::hours(t.undue));
        facts << s;
        m_db->setSetting("summary/last_shown", last.toString(Qt::ISODate));   // no sale otro cartel
    }

    // ---- Racha y comodines (Mejoras 2.0)
    const int jokers = m_db->setting("jokers_per_week", 2).toInt();
    const auto st = stats::streak(m_db->studiedDays(), today, jokers);
    const int left = qMax(0, jokers - st.jokersThisWeek);
    if (st.days > 0) {
        QString s = QStringLiteral("<b>Racha:</b> %1 %2").arg(st.days).arg(st.days == 1 ? "día" : "días");
        if (jokers > 0)
            s += left == 0 ? QStringLiteral(" · sin comodines esta semana")
                           : QStringLiteral(" · te %1 %2 %3 esta semana").arg(left == 1 ? "queda" : "quedan").arg(left).arg(left == 1 ? "comodín" : "comodines");
        facts << s;
        if (!st.todayDone) {
            const QString warn = left == 0 ? QStringLiteral("Si hoy no estudiás, se corta la racha.")
                                           : QStringLiteral("Si hoy no estudiás, se usa un comodín.");
            facts << QStringLiteral("<span style='color:%1'>%2</span>").arg(left == 0 ? theme::pomoAccent.name() : theme::textDim.name(), warn);
        }
    }
    if (const auto g = goals::resolve(m_db->goals(), today); g && !g->spec.isEmpty())
        facts << QStringLiteral("<b>Objetivo de hoy:</b> %1").arg(goals::describe(g->spec).toHtmlEscaped());
    auto* info = line(facts.join(QStringLiteral("<br>")), "GreetFacts", m_card);
    info->setVisible(!facts.isEmpty());

    // ---- Con qué actividad arrancás (Mejoras 2.0 · 11)
    auto* actLabel = new QLabel(QStringLiteral("Hoy arrancás con"), m_card);
    actLabel->setObjectName("GreetFacts");
    m_activity = new QPushButton(m_card);
    m_activity->setObjectName("GreetActivity");
    m_activity->setCursor(Qt::PointingHandCursor);
    m_activity->setToolTip(QStringLiteral("¿No es esa? Cambiala acá"));
    connect(m_activity, &QPushButton::clicked, this, [this] {
        emit activityMenuRequested(m_activity->mapToGlobal(QPoint(0, m_activity->height())));
    });
    auto* actRow = new QHBoxLayout;
    actRow->setSpacing(8);
    actRow->addWidget(actLabel);
    actRow->addWidget(m_activity);
    actRow->addStretch();

    auto* go = new QPushButton(QStringLiteral("A trabajar"), m_card);
    go->setObjectName("GreetGo");
    go->setCursor(Qt::PointingHandCursor);
    go->setDefault(true);
    auto* later = new QPushButton(QStringLiteral("Ahora no"), m_card);
    later->setObjectName("GreetLater");
    later->setCursor(Qt::PointingHandCursor);
    connect(go, &QPushButton::clicked, this, [this] { finish(true); });
    connect(later, &QPushButton::clicked, this, [this] { finish(false); });
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(later);
    buttons->addWidget(go);

    auto* lay = new QVBoxLayout(m_card);
    lay->setContentsMargins(24, 20, 24, 18);
    lay->setSpacing(8);
    lay->addWidget(title);
    if (quote) lay->addWidget(quote);
    lay->addSpacing(4);
    lay->addWidget(info);
    lay->addSpacing(4);
    lay->addLayout(actRow);
    lay->addSpacing(6);
    lay->addLayout(buttons);

    m_fade = new QGraphicsOpacityEffect(this);
    m_fade->setOpacity(0.0);
    setGraphicsEffect(m_fade);
    go->setFocus();
}

void Greeting::setActivity(const QString& name, const QColor& color)
{
    QPixmap dot(10, 10);
    dot.fill(Qt::transparent);
    QPainter p(&dot);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color.isValid() ? color : theme::textFaint);
    p.drawEllipse(QRectF(1, 1, 8, 8));
    p.end();
    m_activity->setIcon(QIcon(dot));
    m_activity->setText(QStringLiteral("%1  ▾").arg(name.isEmpty() ? QStringLiteral("Sin actividad") : name));
}

void Greeting::popIn()
{
    if (parentWidget()) setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
    auto* a = new QPropertyAnimation(m_fade, "opacity", this);
    a->setDuration(220);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

void Greeting::finish(bool accepted)
{
    if (m_closing) return;
    m_closing = true;
    auto* a = new QPropertyAnimation(m_fade, "opacity", this);
    a->setDuration(160);
    a->setEndValue(0.0);
    connect(a, &QAbstractAnimation::finished, this, [this, ok = accepted] {
        if (ok) emit this->accepted(); else emit declined();
        close();
    });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

void Greeting::resizeEvent(QResizeEvent*)
{
    const int w = qMin(width() - 32, 440);
    m_card->setFixedWidth(qMax(260, w));
    m_card->adjustSize();
    m_card->move((width() - m_card->width()) / 2, qMax(16, (height() - m_card->height()) / 2));
}

void Greeting::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor dim = theme::bg;
    dim.setAlphaF(0.82);
    p.fillRect(rect(), dim);
    // La tarjeta: fondo, borde y un brillo muy suave arriba.
    const QRectF r = QRectF(m_card->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::mix(theme::bg, theme::text, 0.16), 1));
    p.setBrush(theme::mix(theme::bg, theme::text, 0.04));
    p.drawRoundedRect(r, 12, 12);
    QLinearGradient glow(r.topLeft(), QPointF(r.left(), r.top() + 110));
    QColor a = theme::pomoAccent;
    a.setAlphaF(0.08);
    glow.setColorAt(0, a);
    glow.setColorAt(1, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 11, 11);
}

void Greeting::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { finish(false); return; }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { finish(true); return; }
    QWidget::keyPressEvent(e);
}

void Greeting::mousePressEvent(QMouseEvent* e)
{
    // Click afuera de la tarjeta: como "Ahora no". Adentro no hace nada (ni arrastra la ventana).
    if (!m_card->geometry().contains(e->position().toPoint())) finish(false);
    e->accept();
}
