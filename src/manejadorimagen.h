// ============================================================================
// manejadorimagen.h - Carga de imágenes con OpenCV
// ============================================================================
#pragma once

#include <QCoreApplication>   // Necesario para Q_DECLARE_TR_FUNCTIONS
#include <QImage>
#include <QString>
#include <QStringList>

#include <expected>
#include <opencv2/core.hpp>

/**
 * @brief Encapsula la carga y conversión de imágenes usando OpenCV.
 *
 * Decisiones de diseño:
 *   - Devolvemos std::expected en lugar de excepciones: OpenCV no usa
 *     excepciones por defecto (salvo con cv::Exception en algunos casos),
 *     y std::expected hace explícito el manejo de errores en la firma.
 *   - Mantenemos un cv::Mat interno con la última imagen cargada, de
 *     forma que los píxeles en bruto estén disponibles para cualquier
 *     procesamiento adicional que se requiera.
 *   - cargarComoImagenQ() es una función de conveniencia para la GUI.
 *     La conversión BGR->RGB la hace esta clase, no la ventana.
 */
class ManejadorImagen
{
    // Añade tr() a esta clase sin necesidad de heredar de QObject.
    Q_DECLARE_TR_FUNCTIONS(ManejadorImagen)

public:
    ManejadorImagen()  = default;
    ~ManejadorImagen() = default;

    // No copiable: la imagen puede ser grande y no queremos duplicarla
    // accidentalmente. Sí movible.
    ManejadorImagen(const ManejadorImagen &)            = delete;
    ManejadorImagen &operator=(const ManejadorImagen &) = delete;
    ManejadorImagen(ManejadorImagen &&)                 noexcept = default;
    ManejadorImagen &operator=(ManejadorImagen &&)      noexcept = default;

    /**
     * @brief Carga una imagen desde disco.
     * @param rutaArchivo Ruta al archivo.
     * @return cv::Mat con la imagen en formato BGR, o un mensaje de error.
     */
    [[nodiscard]] std::expected<cv::Mat, QString>
    cargar(const QString &rutaArchivo);

    /**
     * @brief Carga una imagen y la devuelve lista para mostrar en Qt.
     * @param rutaArchivo Ruta al archivo.
     * @return QImage en formato RGB, o un mensaje de error.
     *
     * Este método combina cargar() con la conversión a QImage para
     * simplificar el uso desde la GUI.
     */
    [[nodiscard]] std::expected<QImage, QString>
    cargarComoImagenQ(const QString &rutaArchivo);

    /// Devuelve la última imagen cargada (puede estar vacía).
    [[nodiscard]] const cv::Mat &imagenActual() const noexcept { return m_imagenActual; }

    /**
     * @brief Extensiones de archivo soportadas, con patrón glob.
     * @return QStringList con patrones tipo "*.jpg", "*.png", etc.
     *
     * Fuente única de verdad para los filtros de QFileDialog y para la
     * construcción de la lista de imágenes del directorio. Si OpenCV
     * añade soporte para un formato nuevo, basta con añadirlo aquí.
     */
    [[nodiscard]] static const QStringList &extensionesSoportadas();

private:
    cv::Mat m_imagenActual;
};
