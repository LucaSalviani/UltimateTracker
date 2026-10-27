#include "ui/Greeting.h"
#include "core/Database.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Goals.h"
#include "core/Stats.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
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
} // namespace

bool Greeting::shouldShow(Database& db)
{
    if (!db.setting("greeting", 1).toBool()) return false;
    const QString last = db.setting("greeting/last").toString();
    return last != fmt::dayKey(dayclock::today());
}

Greeting::Greeting(Database* db, const QRect& appGeometry)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_db(db)
    , m_target(appGeometry)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("UltimateTracker"));
    setWindowIcon(icons::get(icons::Id::AppLogo));
    m_db->setSetting("greeting/last", fmt::dayKey(dayclock::today()));

    // ---- Datos para el saludo
    const QDate today = dayclock::today();
    const int n = int(sizeof(kPhrases) / sizeof(kPhrases[0]));
    // La misma frase todo el día; cambia de un día a otro.
    QRandomGenerator rng(quint32(today.toJulianDay()));
    const QString phrase = QString::fromUtf8(kPhrases[rng.bounded(n)]);

    QStringList facts;
    const auto st = stats::streak(m_db->studiedDays(), today, m_db->setting("jokers_per_week", 2).toInt());
    if (st.days > 0) facts << QStringLiteral("Racha: %1 %2").arg(st.days).arg(st.days == 1 ? "día" : "días");
    QDate last;
    for (const QDate& d : m_db->studiedDays())
        if (d < today && (!last.isValid() || d > last)) last = d;
    if (last.isValid()) {
        qint64 sec = 0;
        for (const Pomodoro& p : m_db->pomodorosBetween(last, last)) sec += p.studiedSec;
        facts << (last == today.addDays(-1) ? QStringLiteral("Ayer: %1").arg(fmt::duration(int(sec)))
                                            : QStringLiteral("Última vez: %1").arg(fmt::duration(int(sec))));
        // El resumen del día anterior ya va acá: que no salte otro cartel después.
        m_db->setSetting("summary/last_shown", last.toString(Qt::ISODate));
    }
    if (const auto g = goals::resolve(m_db->goals(), today); g && !g->spec.isEmpty())
        facts << QStringLiteral("Objetivo de hoy: %1").arg(goals::describe(g->spec));

    // ---- Contenido
    m_content = new QWidget(this);
    auto* logo = new QLabel(m_content);
    logo->setPixmap(icons::get(icons::Id::AppLogo).pixmap(28, 28));
    auto* title = new QLabel(QStringLiteral("%1, ¿estás listo para ser productivo?").arg(hello()), m_content);
    title->setObjectName("GreetTitle");
    title->setWordWrap(true);
    auto* quote = new QLabel(phrase, m_content);
    quote->setObjectName("GreetPhrase");
    quote->setWordWrap(true);
    auto* info = new QLabel(facts.join(QStringLiteral("   ·   ")), m_content);
    info->setObjectName("GreetFacts");
    info->setWordWrap(true);
    info->setVisible(!facts.isEmpty());

    auto* go = new QPushButton(QStringLiteral("A trabajar"), m_content);
    go->setObjectName("GreetGo");
    go->setCursor(Qt::PointingHandCursor);
    go->setDefault(true);
    auto* later = new QPushButton(QStringLiteral("Por ahora no"), m_content);
    later->setObjectName("GreetLater");
    later->setCursor(Qt::PointingHandCursor);
    connect(go, &QPushButton::clicked, this, &Greeting::expandAndOpen);
    connect(later, &QPushButton::clicked, this, [this] {
        emit declined();
        close();
    });

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(later);
    buttons->addWidget(go);

    auto* lay = new QVBoxLayout(m_content);
    lay->setContentsMargins(28, 24, 28, 22);
    lay->setSpacing(10);
    lay->addWidget(logo);
    lay->addSpacing(2);
    lay->addWidget(title);
    lay->addWidget(quote);
    lay->addStretch();
    lay->addWidget(info);
    lay->addSpacing(6);
    lay->addLayout(buttons);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_content);
    m_fade = new QGraphicsOpacityEffect(m_content);
    m_fade->setOpacity(1.0);
    m_content->setGraphicsEffect(m_fade);

    // Centrada en la pantalla donde va a estar la app.
    resize(460, 270);
    QScreen* scr = QGuiApplication::screenAt(m_target.center());
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr->availableGeometry();
    move(avail.center() - QPoint(width() / 2, height() / 2));
    go->setFocus();
}

void Greeting::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::mix(theme::bg, theme::text, 0.14), 1));
    p.setBrush(theme::bg);
    p.drawRoundedRect(r, 12, 12);
    // Un brillo muy suave arriba
    QLinearGradient glow(r.topLeft(), QPointF(r.left(), r.top() + 120));
    QColor a = theme::pomoAccent;
    a.setAlphaF(m_expanding ? 0.0 : 0.07);
    glow.setColorAt(0, a);
    glow.setColorAt(1, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 11, 11);
}

void Greeting::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        emit declined();
        close();
        return;
    }
    QWidget::keyPressEvent(e);
}

void Greeting::mousePressEvent(QMouseEvent* e)
{
    m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
}

void Greeting::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton) move(e->globalPosition().toPoint() - m_dragOffset);
}

// La tarjeta se "convierte" en la ventana: el texto se desvanece y el rectángulo
// crece hasta el lugar de la app. Recién ahí aparece la app de verdad.
void Greeting::expandAndOpen()
{
    if (m_expanding) return;
    m_expanding = true;
    auto* group = new QParallelAnimationGroup(this);
    auto* fade = new QPropertyAnimation(m_fade, "opacity", group);
    fade->setDuration(160);
    fade->setEndValue(0.0);
    auto* grow = new QPropertyAnimation(this, "geometry", group);
    grow->setDuration(340);
    grow->setEasingCurve(QEasingCurve::InOutCubic);
    grow->setStartValue(geometry());
    grow->setEndValue(m_target);
    group->addAnimation(fade);
    group->addAnimation(grow);
    connect(group, &QAbstractAnimation::finished, this, [this] {
        emit accepted();
        // Un instante para que la app se pinte debajo antes de cerrar la tarjeta.
        QTimer::singleShot(60, this, &QWidget::close);
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}
