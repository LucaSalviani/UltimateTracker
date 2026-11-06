#pragma once

#include <QObject>

// Mejoras 2.0: la rueda del mouse no cambia números, listas ni deslizadores
// mientras recorrés una página. Solo los cambia si antes les hiciste click
// (tienen el foco) y el cursor está encima; si no, la rueda desplaza la página.
class WheelGuard : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
};
