#include "ui/WheelGuard.h"

#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QWidget>

namespace {
QWidget* guarded(QObject* obj)
{
    for (QWidget* w = qobject_cast<QWidget*>(obj); w; w = w->parentWidget()) {
        if (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QComboBox*>(w) || qobject_cast<QAbstractSlider*>(w)) {
            // Las barras de desplazamiento sí van con la rueda.
            if (w->inherits("QScrollBar")) return nullptr;
            return w;
        }
        if (w->isWindow()) break;
    }
    return nullptr;
}
} // namespace

bool WheelGuard::eventFilter(QObject* obj, QEvent* e)
{
    if (e->type() == QEvent::Polish) {
        // Que la rueda no les dé el foco (si no, el primer giro ya cambia el valor).
        if (QWidget* w = qobject_cast<QWidget*>(obj))
            if (guarded(w) == w && w->focusPolicy() == Qt::WheelFocus) w->setFocusPolicy(Qt::StrongFocus);
    } else if (e->type() == QEvent::Wheel) {
        if (QWidget* w = guarded(obj)) {
            if (!w->hasFocus() && !(w->focusProxy() && w->focusProxy()->hasFocus())) {
                // Sigue de largo hacia la página (el área desplazable de arriba).
                if (QWidget* p = w->parentWidget()) QApplication::sendEvent(p, e);
                return true;
            }
        }
    }
    return QObject::eventFilter(obj, e);
}
