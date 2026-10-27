#include "ui/Fade.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QWidget>

namespace fade {

void cross(QWidget* w, int ms)
{
    if (!w || !w->isVisible() || w->width() < 2 || w->height() < 2) return;
    // Si ya hay un fundido en curso sobre este widget, se lo saca: la foto nueva lo reemplaza.
    for (QLabel* old : w->findChildren<QLabel*>(QStringLiteral("_fadeShot"), Qt::FindDirectChildrenOnly))
        old->deleteLater();

    auto* shot = new QLabel(w);
    shot->setObjectName(QStringLiteral("_fadeShot"));
    shot->setAttribute(Qt::WA_TransparentForMouseEvents);
    shot->setPixmap(w->grab());
    shot->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    shot->setGeometry(0, 0, w->width(), w->height());
    shot->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* eff = new QGraphicsOpacityEffect(shot);
    shot->setGraphicsEffect(eff);
    shot->show();
    shot->raise();

    auto* anim = new QPropertyAnimation(eff, "opacity", shot);
    anim->setDuration(ms);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    QObject::connect(anim, &QPropertyAnimation::finished, shot, &QObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace fade
