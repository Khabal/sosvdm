// ============================================================================
// metadataextractor.h - Extracción de metadatos con Exiv2
// ============================================================================
#pragma once

#include <QCoreApplication>
#include <QString>

#include <expected>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Estado de la extracción de metadatos.
 *
 * Distinguir estos tres casos permite dar mensajes apropiados al usuario
 * en lugar de tratar "sin metadatos" como si fuera un error.
 */
enum class MetadataStatus {
    Ok,                 ///< Se encontraron metadatos (EXIF, IPTC o XMP).
    NoMetadata,         ///< Formato soportado, pero la imagen no tiene metadatos.
    UnsupportedFormat,  ///< Exiv2 no reconoce el formato (ej. BMP, GIF, TGA).
};

/**
 * @brief Resultado de una extracción de metadatos.
 *
 * El campo `data` solo es válido cuando `status == MetadataStatus::Ok`.
 */
struct MetadataResult {
    MetadataStatus                 status = MetadataStatus::NoMetadata;
    std::vector<std::pair<std::string,
        std::vector<std::pair<std::string, std::string>>>> data;
};

/**
 * @brief Extrae metadatos EXIF, IPTC y XMP de archivos de imagen.
 */
class MetadataExtractor
{
    Q_DECLARE_TR_FUNCTIONS(MetadataExtractor)

public:
    using Entry       = std::pair<std::string, std::string>;
    using EntryList   = std::vector<Entry>;
    using MetadataMap = std::vector<std::pair<std::string, EntryList>>;

    MetadataExtractor()  = default;
    ~MetadataExtractor() = default;

    MetadataExtractor(const MetadataExtractor &)            = delete;
    MetadataExtractor &operator=(const MetadataExtractor &) = delete;
    MetadataExtractor(MetadataExtractor &&)                 = default;
    MetadataExtractor &operator=(MetadataExtractor &&)      = default;

    /**
     * @brief Extrae los metadatos de una imagen.
     * @param filePath Ruta al archivo.
     * @return MetadataResult (posiblemente con status NoMetadata o
     *         UnsupportedFormat) o un QString de error real.
     *
     * Un error real solo se devuelve cuando el archivo no existe o Exiv2
     * falla por una razón no clasificable. Los casos "formato no soportado"
     * y "sin metadatos" NO son errores: se comunican mediante `status`.
     */
    [[nodiscard]] std::expected<MetadataResult, QString>
    extract(const QString &filePath);

    [[nodiscard]] static QString formatAsText(const MetadataMap &metadata);

    [[nodiscard]] static std::expected<void, QString>
    writeToFile(const MetadataMap &metadata, const QString &filePath);

    static void printToStdout(const MetadataMap &metadata);
};
