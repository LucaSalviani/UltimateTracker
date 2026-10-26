#pragma once

#include <QWidget>

// Stats del día en vivo, abajo de la grilla:  4 POMOS · 240 MIN // 4 H · …
// Se puede ver solo la actividad actual (por defecto) o todo el día.
class LiveStats : public QWidget {
    Q_OBJECT
public:
    struct Data {
        bool    activityScope = true;  // true = esta actividad · false = todo el día
        QString scopeName;             // nombre de la actividad (si activityScope)
        int     pomos = 0;
        qint64  seconds = 0;
        // Tercer dato: en modo actividad, total histórico de la actividad;
        // en modo día, cuántas actividades distintas tocaste hoy.
        qint64  activityTotalSec = 0;
        int     activities = 0;
    };

    explicit LiveStats(QWidget* parent = nullptr);
    void setData(const Data& d);
    QSize sizeHint() const override { return {320, 64}; }

signals:
    void scopeToggled();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF toggleRect() const;

    Data m_data;
    bool m_hoverToggle = false;
};
