#include "graphicsviewzoom.h"

#include <QWheelEvent>
#include <cmath>

GraphicsViewZoom::GraphicsViewZoom(QWidget *parent)
    : QGraphicsView(parent)
{
}

void GraphicsViewZoom::wheelEvent(QWheelEvent *event)
{
    // Un "paso" estándar de rueda son 120 unidades de angleDelta.
    // En la mayoría de ratones, un click de rueda = 1 paso.
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }

    // 2% por paso de rueda, tal como se ha acordado.
    emit zoomStepRequested(steps * 2);
    event->accept();
}
