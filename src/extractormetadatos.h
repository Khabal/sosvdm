// ============================================================================
// extractormetadatos.h - Extracción de metadatos con Exiv2
// ============================================================================
#pragma once

#include <QCoreApplication>
#include <QString>

#include <cstdint>
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
enum class EstadoMetadatos : std::uint8_t {
    Ok,                 ///< Se encontraron metadatos (EXIF, IPTC o XMP).
    SinMetadatos,       ///< Formato soportado, pero la imagen no tiene metadatos.
    FormatoNoSoportado, ///< Exiv2 no reconoce el formato (ej. BMP, GIF, TGA).
};

/**
 * @brief Resultado de una extracción de metadatos.
 *
 * El campo `datos` solo es válido cuando `estado == EstadoMetadatos::Ok`.
 */
struct ResultadoMetadatos {
    EstadoMetadatos estado = EstadoMetadatos::SinMetadatos;
    std::vector<std::pair<std::string,
        std::vector<std::pair<std::string, std::string>>>> datos;
};

/**
 * @brief Extrae metadatos EXIF, IPTC y XMP de archivos de imagen.
 */
class ExtractorMetadatos
{
    Q_DECLARE_TR_FUNCTIONS(ExtractorMetadatos)

public:
    using Entrada       = std::pair<std::string, std::string>;
    using ListaEntradas = std::vector<Entrada>;
    using MapaMetadatos = std::vector<std::pair<std::string, ListaEntradas>>;

    ExtractorMetadatos()  = default;
    ~ExtractorMetadatos() = default;

    ExtractorMetadatos(const ExtractorMetadatos &)            = delete;
    ExtractorMetadatos &operator=(const ExtractorMetadatos &) = delete;
    ExtractorMetadatos(ExtractorMetadatos &&)                 = default;
    ExtractorMetadatos &operator=(ExtractorMetadatos &&)      = default;

    /**
     * @brief Extrae los metadatos de una imagen.
     * @param rutaArchivo Ruta al archivo.
     * @return ResultadoMetadatos (posiblemente con estado SinMetadatos o
     *         FormatoNoSoportado) o un QString de error real.
     *
     * Un error real solo se devuelve cuando el archivo no existe o Exiv2
     * falla por una razón no clasificable. Los casos "formato no soportado"
     * y "sin metadatos" NO son errores: se comunican mediante `estado`.
     */
    [[nodiscard]] std::expected<ResultadoMetadatos, QString>
    extraer(const QString &rutaArchivo);

    [[nodiscard]] static QString formatearComoTexto(const MapaMetadatos &metadatos);

    [[nodiscard]] static std::expected<void, QString>
    escribirEnArchivo(const MapaMetadatos &metadatos, const QString &rutaArchivo);

    static void imprimirEnSalidaEstandar(const MapaMetadatos &metadatos);
};
