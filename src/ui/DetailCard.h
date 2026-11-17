#pragma once

#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;

// Mejoras 3.0: mini ventana dentro de la app con el detalle de un dato
// (Estadísticas: racha, récords, objetivos…). Se cierra con Esc, con «Cerrar»
// o tocando afuera.
class DetailCard : public QWidget {
    Q_OBJECT
public:
    explicit DetailCard(QWidget* parent);
    // html: texto con formato. action: botón opcional a la izquierda (ej. «Reiniciar racha…»).
    void showDetail(const QString& title, const QString& html,
                    const QString& actionText = QString(), std::function<void()> action = {});
    void closeCard();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void place();
    QWidget*     m_card;
    QLabel*      m_title;
    QLabel*      m_body;
    class QScrollArea* m_scroll;
    QPushButton* m_action;
    std::function<void()> m_onAction;
};
