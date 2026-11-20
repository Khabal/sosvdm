// ============================================================================
// vistagraficanivelampliacion.h - Vista gráfica con control de nivel de ampliación
// ============================================================================
#pragma once

#include <QGraphicsView>

/**
 * @brief QGraphicsView que convierte la rueda del ratón en una petición de nivel de ampliación.
 *
 * No aplica el nivel de ampliación directamente: emite `pasoNivelAmpliacionSolicitado(variacionPorcentaje)`
 * para que la ventana decida cómo aplicar el cambio, manteniendo la lógica
 * de nivel de ampliación en un único sitio (VentanaPrincipal::aplicarPasoNivelAmpliacion).
 *
 * El scroll vertical normal de la rueda se desactiva para evitar que la vista
 * se desplace al mismo tiempo que se cambia el nivel de ampliación. El desplazamiento
 * sigue disponible arrastrando con el botón izquierdo (ScrollHandDrag).
 */
class VistaGraficaNivelAmpliacion : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VistaGraficaNivelAmpliacion(QWidget *padre = nullptr);

signals:
    /**
     * @brief Petición de cambio de nivel de ampliación expresada en puntos porcentuales.
     * @param variacionPorcentaje Variación a aplicar sobre el porcentaje actual.
     *                            Positivo acerca, negativo aleja.
     */
    void pasoNivelAmpliacionSolicitado(int variacionPorcentaje);

protected:
    // Se mantiene el nombre original de Qt para que el framework
    // invoque correctamente este método virtual.
    void wheelEvent(QWheelEvent *evento) override;
};
