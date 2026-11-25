#pragma once

#include "core/Database.h"
#include "core/Insights.h"

#include <QDate>
#include <QMap>
#include <QSet>
#include <QWidget>
#include <functional>

// Mejoras 4.0 · 12: la vista "Evolución" de las estadísticas. Gráficos a lo largo
// del tiempo para ver cómo mejoraste o empeoraste: horas con tendencia, recreo
// indebido, horas inactivas, metas semanales y marcas, y cómo te fue (diario).
class EvolutionView : public QWidget {
    Q_OBJECT
public:
    explicit EvolutionView(QWidget* parent = nullptr);

    // filter: actividades que cuentan para las horas (nullptr = todas).
    void reload(Database* db, const QSet<qint64>* filter);
    void setHoverTips(bool on) { m_hoverTips = on; }   // Mejoras 4.1: info al pasar el mouse (si no, con click)

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override { setMinimumHeight(contentHeight(width())); }
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Week {
        QDate monday;
        int days = 0;                 // días del rango dentro de la semana (hasta hoy)
        qint64 study = 0, undue = 0, inactive = 0, breaks = 0;
        int marks[3] = {0, 0, 0};     // días con marca de bronce / plata / oro
        int goalMin = -1;             // meta de esa semana (-1 = no quedó anotada)
    };
    bool m_hoverTips = false;
    struct Zone { QRectF rect; QString tip; QString key; };

    int contentHeight(int w) const;
    void setRange(int days);
    QString summaryHtml() const;

    // dibujo
    void card(QPainter& p, const QRectF& r, const QString& title, const QString& sentence) const;
    QRectF plotArea(const QRectF& card) const;
    void paintHours(QPainter& p, const QRectF& r);
    void paintWeeklyBars(QPainter& p, const QRectF& r, const QString& title, const QString& sentence,
                         const std::function<double(const Week&)>& value, const std::function<QString(double)>& label,
                         const QColor& ink, const QString& unit);
    void paintGoals(QPainter& p, const QRectF& r);
    void paintMood(QPainter& p, const QRectF& r);
    void paintTiles(QPainter& p, const QRectF& r);

    Database* m_db = nullptr;
    const QSet<qint64>* m_filter = nullptr;
    QSet<qint64> m_filterCopy;
    int m_range = 90;                 // días; 0 = todo
    QDate m_today, m_from;
    QVector<QDate> m_dates;           // un día por entrada
    QVector<insights::DayStat> m_days;
    QVector<Week> m_weeks;
    QHash<QDate, Database::DayDiary> m_diary;
    insights::Trend m_trend;
    QVector<Zone> m_zones;
    QString m_summary;
    QString m_studyLabel = QStringLiteral("Estudio");
};
