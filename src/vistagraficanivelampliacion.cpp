// ============================================================================
// vistagraficanivelampliacion.cpp
// ============================================================================
#include "vistagraficanivelampliacion.h"

#include <QWheelEvent>
#include <cmath>

VistaGraficaNivelAmpliacion::VistaGraficaNivelAmpliacion(QWidget *padre)
    : QGraphicsView(padre)
{
}

void VistaGraficaNivelAmpliacion::wheelEvent(QWheelEvent *evento)
{
    // Un "paso" estándar de rueda son 120 unidades de angleDelta.
    // En la mayoría de ratones, un click de rueda = 1 paso.
    const int pasos = evento->angleDelta().y() / 120;
    if (pasos == 0) {
        evento->ignore();
        return;
    }

    // 2% por paso de rueda, tal como se ha acordado.
    emit pasoNivelAmpliacionSolicitado(pasos * 2);
    evento->accept();
}
