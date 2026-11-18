// ============================================================================
// imagehandler.h - Carga de imágenes con OpenCV
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
 *   - loadAsQImage() es una función de conveniencia para la GUI. La
 *     conversión BGR->RGB la hace esta clase, no la ventana.
 */
class ImageHandler
{
    // Añade tr() a esta clase sin necesidad de heredar de QObject.
    Q_DECLARE_TR_FUNCTIONS(ImageHandler)

public:
    ImageHandler() = default;
    ~ImageHandler() = default;

    // No copiable: la imagen puede ser grande y no queremos duplicarla
    // accidentalmente. Sí movible.
    ImageHandler(const ImageHandler &)            = delete;
    ImageHandler &operator=(const ImageHandler &) = delete;
    ImageHandler(ImageHandler &&)                 = default;
    ImageHandler &operator=(ImageHandler &&)      = default;

    /**
     * @brief Carga una imagen desde disco.
     * @param filePath Ruta al archivo.
     * @return cv::Mat con la imagen en formato BGR, o un mensaje de error.
     */
    [[nodiscard]] std::expected<cv::Mat, QString>
    load(const QString &filePath);

    /**
     * @brief Carga una imagen y la devuelve lista para mostrar en Qt.
     * @param filePath Ruta al archivo.
     * @return QImage en formato RGB, o un mensaje de error.
     *
     * Este método combina load() con la conversión a QImage para
     * simplificar el uso desde la GUI.
     */
    [[nodiscard]] std::expected<QImage, QString>
    loadAsQImage(const QString &filePath);

    /// Devuelve la última imagen cargada (puede estar vacía).
    [[nodiscard]] const cv::Mat &currentImage() const noexcept { return m_currentImage; }

    /**
     * @brief Extensiones de archivo soportadas, con patrón glob.
     * @return QStringList con patrones tipo "*.jpg", "*.png", etc.
     *
     * Fuente única de verdad para los filtros de QFileDialog y para la
     * construcción de la lista de imágenes del directorio. Si OpenCV
     * añade soporte para un formato nuevo, basta con añadirlo aquí.
     */
    [[nodiscard]] static const QStringList &supportedExtensions();

private:
    cv::Mat m_currentImage;
};
