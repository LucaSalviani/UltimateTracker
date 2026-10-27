#pragma once

#include <QColor>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QWidget>

class QPainter;

// M18: cómo se dibuja un cuadradito de pomodoro. La grilla decide dónde va cada
// uno; acá se decide cómo se ve. Todo se elige en Ajustes › Cuadraditos.
namespace tiles {

enum class Shape    { Classic = 0, Slant = 1, Card = 2, Bevel = 3 };     // forma
enum class Mark     { None = 0, Diagonal = 1, Cross = 2, Stamp = 3, SquareTick = 4 };  // marca de "hecho"
enum class Fill     { Bar = 0, Liquid = 1, Segments = 2, Clock = 3 };    // cómo se llena el que corre
enum class Complete { Pop = 0, Seal = 1, Shine = 2, Flip = 3 };          // animación al completarse

struct Style {
    Shape    shape = Shape::Classic;
    Mark     mark = Mark::Cross;
    Fill     fill = Fill::Bar;
    Complete complete = Complete::Shine;
    // M23
    bool     inverted = true;   // fondo gris que se va llenando de negro
    bool     stripe = false;    // rayita del color de la actividad
    bool     caption = false;   // nombre de la actividad dentro del cuadradito
    int      border = 0;        // 0 fino · 1 normal · 2 grueso
    int      palette = 0;       // 0 = la del tema; ver paletteNames()
    // M29
    int      numberPos = 1;     // 0 adentro · 1 afuera (arriba) · 2 oculto
    bool     numberPrefix = true;    // "Nº3" en vez de "3"
    int      highlight = 1;     // 0 nada · 1 el que está corriendo (acento) · 2 el último del día (rojo, como antes)
};
// "3" o "Nº3" según el ajuste.
QString numberText(int n);
// La fuente de los números del cronómetro (Bahnschrift).
QFont font(qreal px, int weight);
Style& style();

QStringList shapeNames();
QStringList markNames();
QStringList fillNames();
QStringList completeNames();
QStringList paletteNames();   // M23: colores solo para los cuadraditos
QStringList borderNames();

enum class State { Done, Final, Running, Parked, Ghost };

struct Tile {
    State   state = State::Done;
    double  progress = 0;      // 0..1 (los que corren)
    QString label;             // "25m"
    QString caption;           // nombre corto de la actividad
    int     number = 0;        // número del día (0 = no mostrar)
    QColor  stripe;            // color de la actividad
    double  completeT = -1;    // 0..1 mientras dura la animación de completado
    bool    accent = false;    // M29: el que está corriendo, resaltado con el color del pomodoro
};

// Ancho / alto del cuadradito (la tarjeta es vertical).
double aspect();
QPainterPath shapePath(const QRectF& r);
void paint(QPainter& p, const QRectF& r, const Tile& t, qint64 nowMs);
// ¿Hay que repintar seguido? (líquido con olas, segmento que late)
bool animatesWhileRunning();
constexpr int kCompleteMs = 750;

} // namespace tiles

// Muestra en vivo del estilo elegido (para Ajustes).
class TilePreview : public QWidget {
    Q_OBJECT
public:
    explicit TilePreview(QWidget* parent = nullptr);
    QSize sizeHint() const override { return {360, 86}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QTimer        m_timer;
    QElapsedTimer m_clock;
};
