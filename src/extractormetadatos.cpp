// ============================================================================
// extractormetadatos.cpp
// ============================================================================
#include "extractormetadatos.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <exiv2/exiv2.hpp>

#include <iostream>

namespace {

/**
 * @brief Comprueba si el formato del archivo admite metadatos.
 * @param rutaArchivo Ruta al archivo.
 * @return true si el formato puede contener EXIF, IPTC o XMP.
 *
 * La comprobación se hace por extensión. Es una simplificación consciente:
 * el caso marginal de un archivo renombrado (un .bmp que en realidad es
 * .jpg) se resuelve solo, porque Exiv2 abrirá el contenido real y
 * encontrará los metadatos si existen.
 */
bool formatoSoportaMetadatos(const QString &rutaArchivo)
{
    // Formatos que admiten metadatos según la documentación de Exiv2.
    // El resto (BMP, GIF, TGA, Netpbm, HDR, EXR, Sun Raster, PFM)
    // se reconocen pero no pueden contener EXIF/IPTC/XMP.
    static const QStringList soportados = {
        // Formatos comunes con soporte completo.
        "jpg", "jpeg", "jpe",
        "png",
        "tif", "tiff",
        "webp",
        "jp2",
        "psd",
        // Formatos RAW de cámaras (soporte total o parcial).
        "cr2", "crw", "nef", "dng", "arw", "orf",
        "pef", "rw2", "mrw", "raf", "sr2", "srw",
    };
    return soportados.contains(QFileInfo(rutaArchivo).suffix().toLower());
}

} // namespace

std::expected<ResultadoMetadatos, QString>
ExtractorMetadatos::extraer(const QString &rutaArchivo)
{
    const QFileInfo infoArchivo(rutaArchivo);
    if (!infoArchivo.exists() || !infoArchivo.isFile()) {
        return std::unexpected(tr("El archivo no existe o no es un fichero regular."));
    }

    try {
        auto imagen = Exiv2::ImageFactory::open(rutaArchivo.toStdString());
        if (!imagen.get()) {
            return std::unexpected(tr("Exiv2 no pudo abrir la imagen."));
        }
        imagen->readMetadata();

        ResultadoMetadatos resultado;

        // --- EXIF ---
        const Exiv2::ExifData &datosExif = imagen->exifData();
        if (!datosExif.empty()) {
            ListaEntradas entradas;
            entradas.reserve(datosExif.count());
            for (const auto &entrada : datosExif) {
                entradas.emplace_back(entrada.key(), entrada.toString());
            }
            resultado.datos.emplace_back("EXIF", std::move(entradas));
        }

        // --- IPTC ---
        const Exiv2::IptcData &datosIptc = imagen->iptcData();
        if (!datosIptc.empty()) {
            ListaEntradas entradas;
            entradas.reserve(datosIptc.count());
            for (const auto &entrada : datosIptc) {
                entradas.emplace_back(entrada.key(), entrada.toString());
            }
            resultado.datos.emplace_back("IPTC", std::move(entradas));
        }

        // --- XMP ---
        const Exiv2::XmpData &datosXmp = imagen->xmpData();
        if (!datosXmp.empty()) {
            ListaEntradas entradas;
            entradas.reserve(datosXmp.count());
            for (const auto &entrada : datosXmp) {
                entradas.emplace_back(entrada.key(), entrada.toString());
            }
            resultado.datos.emplace_back("XMP", std::move(entradas));
        }

        if (!resultado.datos.empty()) {
            resultado.estado = EstadoMetadatos::Ok;
        } else if (formatoSoportaMetadatos(rutaArchivo)) {
            // Formato que admite metadatos, pero esta imagen concreta no los tiene.
            resultado.estado = EstadoMetadatos::SinMetadatos;
        } else {
            // Formato que por definición no puede contener EXIF/IPTC/XMP.
            resultado.estado = EstadoMetadatos::FormatoNoSoportado;
        }
        return resultado;

    } catch (const Exiv2::Error &excepcion) {
        // Exiv2 lanza kerFileContainsUnknownImageType cuando el archivo
        // no es de un formato que reconozca (BMP, GIF, TGA, etc.).
        // No es un error del programa: es una limitación conocida.
        if (excepcion.code() == Exiv2::ErrorCode::kerFileContainsUnknownImageType) {
            ResultadoMetadatos resultado;
            resultado.estado = EstadoMetadatos::FormatoNoSoportado;
            return resultado;
        }
        return std::unexpected(tr("Error de Exiv2: %1")
                                   .arg(QString::fromUtf8(excepcion.what())));
    } catch (const std::exception &excepcion) {
        return std::unexpected(tr("Error inesperado: %1")
                                   .arg(QString::fromUtf8(excepcion.what())));
    }
}

QString ExtractorMetadatos::formatearComoTexto(const MapaMetadatos &metadatos)
{
    QString salida;
    QTextStream flujo(&salida);

    for (const auto &[categoria, entradas] : metadatos) {
        flujo << '[' << QString::fromStdString(categoria) << "]\n";
        for (const auto &[clave, valor] : entradas) {
            flujo << "  " << QString::fromStdString(clave)
                  << " = "  << QString::fromStdString(valor) << '\n';
        }
        flujo << '\n';
    }
    return salida;
}

std::expected<void, QString>
ExtractorMetadatos::escribirEnArchivo(const MapaMetadatos &metadatos, const QString &rutaArchivo)
{
    // QFile con WriteOnly|Text|Truncate. QIODevice::Text normaliza los
    // finales de línea según la plataforma (relevante en la fase Windows).
    QFile archivo(rutaArchivo);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return std::unexpected(tr("No se pudo abrir el archivo para escritura: %1")
                                   .arg(archivo.errorString()));
    }

    QTextStream flujo(&archivo);
    // UTF-8 explícito: importante cuando lleguen caracteres no ASCII
    // (nombres de autor, comentarios IPTC, etc.).
    flujo.setEncoding(QStringConverter::Utf8);
    flujo << formatearComoTexto(metadatos);

    if (flujo.status() != QTextStream::Ok) {
        return std::unexpected(tr("Error al escribir en el archivo: %1")
                                   .arg(rutaArchivo));
    }

    archivo.close();
    if (archivo.error() != QFileDevice::NoError) {
        return std::unexpected(tr("Error al cerrar el archivo: %1")
                                   .arg(archivo.errorString()));
    }

    return {};
}

void ExtractorMetadatos::imprimirEnSalidaEstandar(const MapaMetadatos &metadatos)
{
    // QTextStream sobre stdout gestiona la codificación correctamente en
    // todas las plataformas, incluyendo Windows con consolas no UTF-8.
    QTextStream salida(stdout);
    salida.setEncoding(QStringConverter::Utf8);
    salida << formatearComoTexto(metadatos);
}
