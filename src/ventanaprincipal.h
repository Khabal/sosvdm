// ============================================================================
// ventanaprincipal.h - Ventana principal de "Sólo otro simple visor de metadatos"
// ============================================================================
#pragma once

#include "extractormetadatos.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class VentanaPrincipal;
}
QT_END_NAMESPACE

class ManejadorImagen;

/**
 * @brief Ventana principal de la aplicación.
 *
 * Responsabilidades:
 *   - Mostrar la imagen seleccionada y sus metadatos.
 *   - Gestionar la navegación, nivel de ampliación, rotación y modos de visualización.
 *   - Servir de punto de integración entre ManejadorImagen y ExtractorMetadatos.
 *
 * Nota de diseño: la lógica de carga de imágenes y extracción de metadatos
 * vive en sus propias clases. VentanaPrincipal actúa como orquestador y capa de
 * presentación.
 */
class VentanaPrincipal : public QMainWindow {
    Q_OBJECT

public:
    explicit VentanaPrincipal(QWidget *padre = nullptr);
    ~VentanaPrincipal() override;

    /**
     * @brief Carga una imagen desde disco y actualiza la UI.
     * @param rutaArchivo Ruta al archivo de imagen.
     */
    void cargarImagen(const QString &rutaArchivo);

private:
    // --- Métodos auxiliares ---
    void configurarInterfaz();
    void configurarConexiones();
    void actualizarBarraEstado(const QString &mensaje);
    void limpiarVista();
    void reconstruirListaDirectorio();
    void mostrarImagenEn(int indice);
    void rotarImagenActual(int grados);
    void actualizarEtiquetaNivelAmpliacion();
    [[nodiscard]] QString construirCabeceraImagen() const;

    // --- Miembros ---
    std::unique_ptr<Ui::VentanaPrincipal> m_interfaz;

    QGraphicsScene *m_escena = nullptr;
    QGraphicsPixmapItem *m_elementoPixmap = nullptr;
    QTimer *m_temporizadorPresentacion = nullptr;

    std::unique_ptr<ManejadorImagen> m_manejadorImagen;
    std::unique_ptr<ExtractorMetadatos> m_extractorMetadatos;

    ExtractorMetadatos::MapaMetadatos m_metadatosActuales;
    EstadoMetadatos m_estadoMetadatosActuales = EstadoMetadatos::SinMetadatos;

    QString m_rutaArchivoActual;
    QStringList m_imagenesDirectorio;
    int m_indiceActual = -1;

    QLabel *m_etiquetaNivelAmpliacion = nullptr;

    // Límites de nivel de ampliación, en porcentaje.
    static constexpr double kNivelAmpliacionMinimo = 2.0;
    static constexpr double kNivelAmpliacionMaximo = 2000.0;

    // Pasos de nivel de ampliación, en puntos porcentuales.
    static constexpr int kPasoNivelAmpliacionRueda = 2;
    static constexpr int kPasoNivelAmpliacionBoton = 5;

private slots: // NOLINT(readability-redundant-access-specifiers)
    // --- Menú Archivo ---
    void alAbrirArchivo();
    void alCerrarImagen();
    void alSalir();
    void alImprimirImagen();
    void alImprimirMetadatos();
    void alExportarMetadatos();

    // --- Menú Visualización ---
    void alSiguienteImagen();
    void alImagenAnterior();
    void alAcercar();
    void alAlejar();
    void alAjustarVentana();
    void alRotarDerecha();
    void alRotarIzquierda();
    void alAlternarPantallaCompleta();
    void alAlternarModoPresentacion(bool activo);
    void alAlternarPanelMetadatos(bool visible);

    // --- Menú Ayuda ---
    void alMostrarAtajos();
    void alAcercaDe();

    // --- Internos ---
    void alSiguienteDiapositiva();
    void alPulsarEscape();
    void alAtajoReservado();
    void aplicarPasoNivelAmpliacion(int variacionPorcentaje);
};
