#pragma once

#include <QGraphicsView>

/**
 * @brief QGraphicsView que convierte la rueda del ratón en una petición de zoom.
 *
 * No aplica el zoom directamente: emite `zoomStepRequested(deltaPercent)`
 * para que la ventana decida cómo aplicar el cambio, manteniendo la lógica
 * de zoom en un único sitio (MainWindow::applyZoomStep).
 *
 * El scroll vertical normal de la rueda se desactiva para evitar que la vista
 * se desplace al mismo tiempo que se hace zoom. El desplazamiento sigue
 * disponible arrastrando con el botón izquierdo (ScrollHandDrag).
 */
class GraphicsViewZoom : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GraphicsViewZoom(QWidget *parent = nullptr);

signals:
    /**
     * @brief Petición de cambio de zoom expresada en puntos porcentuales.
     * @param deltaPercent Variación a aplicar sobre el porcentaje actual.
     *                     Positivo acerca, negativo aleja.
     */
    void zoomStepRequested(int deltaPercent);

protected:
    void wheelEvent(QWheelEvent *event) override;
};
