// ============================================================================
// metadataextractor.cpp
// ============================================================================
#include "metadataextractor.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <exiv2/exiv2.hpp>

#include <iostream>

namespace {

/**
 * @brief Comprueba si el formato del archivo admite metadatos.
 * @param filePath Ruta al archivo.
 * @return true si el formato puede contener EXIF, IPTC o XMP.
 *
 * La comprobación se hace por extensión. Es una simplificación consciente:
 * el caso marginal de un archivo renombrado (un .bmp que en realidad es
 * .jpg) se resuelve solo, porque Exiv2 abrirá el contenido real y
 * encontrará los metadatos si existen.
 */
bool isFormatSupportedForMetadata(const QString &filePath)
{
    // Formatos que admiten metadatos según la documentación de Exiv2.
    // El resto (BMP, GIF, TGA, Netpbm, HDR, EXR, Sun Raster, PFM)
    // se reconocen pero no pueden contener EXIF/IPTC/XMP.
    static const QStringList supported = {
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
    return supported.contains(QFileInfo(filePath).suffix().toLower());
}

} // namespace

std::expected<MetadataResult, QString>
MetadataExtractor::extract(const QString &filePath)
{
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        return std::unexpected(tr("El archivo no existe o no es un fichero regular."));
    }

    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        if (!image.get()) {
            return std::unexpected(tr("Exiv2 no pudo abrir la imagen."));
        }
        image->readMetadata();

        MetadataResult result;

        // --- EXIF ---
        const Exiv2::ExifData &exifData = image->exifData();
        if (!exifData.empty()) {
            EntryList entries;
            entries.reserve(exifData.count());
            for (const auto &entry : exifData) {
                entries.emplace_back(entry.key(), entry.toString());
            }
            result.data.emplace_back("EXIF", std::move(entries));
        }

        // --- IPTC ---
        const Exiv2::IptcData &iptcData = image->iptcData();
        if (!iptcData.empty()) {
            EntryList entries;
            entries.reserve(iptcData.count());
            for (const auto &entry : iptcData) {
                entries.emplace_back(entry.key(), entry.toString());
            }
            result.data.emplace_back("IPTC", std::move(entries));
        }

        // --- XMP ---
        const Exiv2::XmpData &xmpData = image->xmpData();
        if (!xmpData.empty()) {
            EntryList entries;
            entries.reserve(xmpData.count());
            for (const auto &entry : xmpData) {
                entries.emplace_back(entry.key(), entry.toString());
            }
            result.data.emplace_back("XMP", std::move(entries));
        }

        if (!result.data.empty()) {
            result.status = MetadataStatus::Ok;
        } else if (isFormatSupportedForMetadata(filePath)) {
            // Formato que admite metadatos, pero esta imagen concreta no los tiene.
            result.status = MetadataStatus::NoMetadata;
        } else {
            // Formato que por definición no puede contener EXIF/IPTC/XMP.
            result.status = MetadataStatus::UnsupportedFormat;
        }
        return result;

    } catch (const Exiv2::Error &e) {
        // Exiv2 lanza kerFileContainsUnknownImageType cuando el archivo
        // no es de un formato que reconozca (BMP, GIF, TGA, etc.).
        // No es un error del programa: es una limitación conocida.
        if (e.code() == Exiv2::ErrorCode::kerFileContainsUnknownImageType) {
            MetadataResult result;
            result.status = MetadataStatus::UnsupportedFormat;
            return result;
        }
        return std::unexpected(tr("Error de Exiv2: %1")
                                   .arg(QString::fromUtf8(e.what())));
    } catch (const std::exception &e) {
        return std::unexpected(tr("Error inesperado: %1")
                                   .arg(QString::fromUtf8(e.what())));
    }
}

QString MetadataExtractor::formatAsText(const MetadataMap &metadata)
{
    QString output;
    QTextStream stream(&output);

    for (const auto &[category, entries] : metadata) {
        stream << '[' << QString::fromStdString(category) << "]\n";
        for (const auto &[key, value] : entries) {
            stream << "  " << QString::fromStdString(key)
                   << " = "  << QString::fromStdString(value) << '\n';
        }
        stream << '\n';
    }
    return output;
}

std::expected<void, QString>
MetadataExtractor::writeToFile(const MetadataMap &metadata, const QString &filePath)
{
    // QFile con WriteOnly|Text|Truncate. QIODevice::Text normaliza los
    // finales de línea según la plataforma (relevante en la fase Windows).
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return std::unexpected(tr("No se pudo abrir el archivo para escritura: %1")
                                   .arg(file.errorString()));
    }

    QTextStream stream(&file);
    // UTF-8 explícito: importante cuando lleguen caracteres no ASCII
    // (nombres de autor, comentarios IPTC, etc.).
    stream.setEncoding(QStringConverter::Utf8);
    stream << formatAsText(metadata);

    if (stream.status() != QTextStream::Ok) {
        return std::unexpected(tr("Error al escribir en el archivo: %1")
                                   .arg(filePath));
    }

    file.close();
    if (file.error() != QFileDevice::NoError) {
        return std::unexpected(tr("Error al cerrar el archivo: %1")
                                   .arg(file.errorString()));
    }

    return {};
}

void MetadataExtractor::printToStdout(const MetadataMap &metadata)
{
    // QTextStream sobre stdout gestiona la codificación correctamente en
    // todas las plataformas, incluyendo Windows con consolas no UTF-8.
    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Utf8);
    out << formatAsText(metadata);
}
