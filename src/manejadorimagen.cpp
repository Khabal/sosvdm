// ============================================================================
// manejadorimagen.cpp
// ============================================================================
#include "manejadorimagen.h"

#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp> // cv::cvtColor vive aquí

std::expected<cv::Mat, QString> ManejadorImagen::cargar(const QString &rutaArchivo) {
    // Validación temprana: que el archivo exista y sea legible.
    const QFileInfo infoArchivo(rutaArchivo);
    if (!infoArchivo.exists() || !infoArchivo.isFile()) {
        return std::unexpected(tr("El archivo no existe o no es un fichero regular."));
    }

    // OpenCV espera std::string con la ruta. En Linux, toStdString() usa UTF-8,
    // que es lo correcto. En Windows habrá que revisar la codificación (fase 2).
    const std::string ruta = rutaArchivo.toStdString();

    // cv::IMREAD_UNCHANGED preserva el canal alfa si existe, lo cual es
    // relevante para PNG con transparencia y otros formatos con alfa.
    cv::Mat imagen = cv::imread(ruta, cv::IMREAD_UNCHANGED);

    if (imagen.empty()) {
        return std::unexpected(tr("OpenCV no pudo decodificar la imagen. "
                                  "¿Formato no soportado o archivo corrupto?"));
    }

    m_imagenActual = std::move(imagen);
    return m_imagenActual;
}

std::expected<QImage, QString> ManejadorImagen::cargarComoImagenQ(const QString &rutaArchivo) {
    auto resultado = cargar(rutaArchivo);
    if (!resultado.has_value()) {
        return std::unexpected(resultado.error());
    }

    const cv::Mat &matriz = resultado.value();

    // --- Conversión cv::Mat -> QImage ---
    // OpenCV carga en BGR (o BGRA). Qt usa RGB (o RGBA).
    // QImage::Format_BGR888 existe desde Qt 5.14 y evita la conversión manual,
    // pero necesitamos intercambiar canales para que Qt lo interprete bien.
    //
    // Estrategia: convertimos explícitamente a RGB con cv::cvtColor, y luego
    // construimos el QImage. Así el formato es predecible.
    cv::Mat rgb;
    switch (matriz.channels()) {
    case 1:
        cv::cvtColor(matriz, rgb, cv::COLOR_GRAY2RGB);
        break;
    case 3:
        cv::cvtColor(matriz, rgb, cv::COLOR_BGR2RGB);
        break;
    case 4:
        cv::cvtColor(matriz, rgb, cv::COLOR_BGRA2RGBA);
        break;
    default:
        return std::unexpected(tr("Número de canales no soportado: %1").arg(matriz.channels()));
    }

    // Importante: QImage NO copia los datos del buffer. Hay que asegurarse
    // de que 'rgb' siga vivo mientras se use el QImage. Como 'rgb' es local,
    // hacemos una copia con .copy() para que QImage gestione su propia memoria.
    QImage imagenQ(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                   rgb.channels() == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888);

    return imagenQ.copy(); // Copia profunda: seguro para devolver por valor.
}

const QStringList &ManejadorImagen::extensionesSoportadas() {
    // Orden agrupado por familia de formato, para que los filtros
    // del QFileDialog salgan legibles.
    static const QStringList extensiones = {
        // JPEG
        "*.jpg",
        "*.jpeg",
        "*.jpe",
        // PNG
        "*.png",
        // TIFF
        "*.tif",
        "*.tiff",
        // WebP
        "*.webp",
        // JPEG 2000
        "*.jp2",
        // Windows Bitmap
        "*.bmp",
        "*.dib",
        // GIF
        "*.gif",
        // Targa
        "*.tga",
        // Netpbm
        "*.pbm",
        "*.pgm",
        "*.ppm",
        "*.pnm",
        "*.pxm",
        // Radiance HDR
        "*.hdr",
        "*.pic",
        // OpenEXR
        "*.exr",
        // Sun Raster
        "*.sr",
        "*.ras",
        // PFM
        "*.pfm",
    };
    return extensiones;
}
