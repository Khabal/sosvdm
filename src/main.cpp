// ============================================================================
// main.cpp - Punto de entrada de "Sólo otro simple visor de metadatos"
// ============================================================================
// Este archivo es responsable de:
//   1. Endurecer el entorno antes de inicializar Qt.
//   2. Inicializar la aplicación Qt (QApplication) con argv saneado.
//   3. Analizar los argumentos de línea de comandos.
//   4. Gestionar la comprobación de compatibilidad extendida.
//   5. Decidir entre el modo GUI y el modo terminal (solo metadatos).
//   6. Crear y mostrar la ventana principal.
// ============================================================================

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>

#include "extractormetadatos.h"
#include "ventanaprincipal.h"

#include <cstdlib>   // unsetenv()
#include <expected>
#include <iostream>
#include <string>

#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

// Estructura que encapsula las opciones analizadas de la línea de comandos.
struct OpcionesTerminal {
    QString rutaImagen;                  // Ruta a la imagen a cargar (opcional).
    bool    soloMetadatos = false;       // Modo terminal: solo metadatos.
    bool    detallado     = false;       // Salida detallada.
    bool    segarroAmeGo  = false;       // Opción de compatibilidad extendida.
};

// Analiza los argumentos y rellena la estructura OpcionesTerminal.
//
// Recibe los argumentos como QStringList en lugar de leerlos de
// aplicacion.arguments(), porque main() construye QApplication con argv
// saneado (argc = 1) y por tanto aplicacion.arguments() solo devolvería el
// nombre del programa. Los argumentos reales se capturan antes de crear
// QApplication.
//
// Devuelve std::nullopt si el usuario pidió ayuda/versión (QCommandLineParser
// ya se encarga de mostrar el texto y salir) o si hubo un error.
std::optional<OpcionesTerminal> analizarLineaComandos(const QStringList &argumentos)
{
    QCommandLineParser analizador;
    analizador.setApplicationDescription(
        QCoreApplication::translate("main",
            "Visor imágenes con extracción de metadatos.\n"
            "Nada más que ver por aquí."));

    // Definimos las opciones que acepta el programa.
    const QCommandLineOption opcionMetadatos(
        QStringList{"m", "metadata"},
        QCoreApplication::translate("main", "Muestra solo los metadatos y sale (modo terminal)."));

    const QCommandLineOption opcionDetallado(
        QStringList{"D", "verbose"},
        QCoreApplication::translate("main", "Muestra información detallada por consola."));

    analizador.addOption(opcionMetadatos);
    analizador.addOption(opcionDetallado);

    // Añadimos un argumento posicional: la ruta de la imagen.
    analizador.addPositionalArgument(
        "imagen",
        QCoreApplication::translate("main", "Ruta al archivo de imagen."),
        "[imagen]");

    analizador.addHelpOption();
    analizador.addVersionOption();

    // Opción reservada. No aparece en --help. Reservada para uso interno
    // y comprobaciones de compatibilidad.
    QCommandLineOption opcionSegarro(
        QStringList{"segarro-amego"},
        QCoreApplication::translate("main", "Opción reservada."));
    opcionSegarro.setFlags(QCommandLineOption::HiddenFromHelp);
    analizador.addOption(opcionSegarro);

    // process() analiza los argumentos. En Qt6 ya no devuelve bool: si hay
    // error, o si el usuario pide --help/--version, muestra el mensaje y
    // llama a exit() internamente. Por eso no comprobamos su valor.
    analizador.process(argumentos);

    OpcionesTerminal opciones;

    opciones.segarroAmeGo  = analizador.isSet(opcionSegarro);
    opciones.soloMetadatos = analizador.isSet(opcionMetadatos);
    opciones.detallado     = analizador.isSet(opcionDetallado);

    const QStringList posicionales = analizador.positionalArguments();
    if (!posicionales.isEmpty()) {
        opciones.rutaImagen = posicionales.first();

        // Validación temprana: comprobar que el archivo existe y es legible.
        const QFileInfo infoArchivo(opciones.rutaImagen);
        if (!infoArchivo.exists() || !infoArchivo.isFile() || !infoArchivo.isReadable()) {
            qCritical() << "Error: no se puede leer el archivo:" << opciones.rutaImagen;
            return std::nullopt;
        }
    }

    return opciones;
}

/**
 * @brief Traductor mínimo para las cadenas internas de QCommandLineParser.
 *
 * Qt no traduce al español estas dos cadenas concretas del contexto
 * "QCommandLineParser". En lugar de reimplementar toda la salida de ayuda,
 * este traductor intercepta solo esas dos y deja pasar el resto a los
 * traductores estándar (qtbase_es.qm).
 *
 * Se instala DESPUÉS de traductorQt para que Qt lo consulte primero
 * (los traductores se buscan en orden inverso al de instalación).
 */
class TraductorAyudaLineaComandos : public QTranslator
{
public:
    using QTranslator::QTranslator;

    QString translate(const char *contexto,
                      const char *textoFuente,
                      const char *desambiguacion = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(desambiguacion);
        Q_UNUSED(n);

        if (qstrcmp(contexto, "QCommandLineParser") == 0) {
            if (qstrcmp(textoFuente, "Displays help on commandline options.") == 0) {
                return QStringLiteral(
                    "Muestra la ayuda sobre las opciones de línea de comandos.");
            }
            if (qstrcmp(textoFuente, "Displays help, including generic Qt options.") == 0) {
                return QStringLiteral(
                    "Muestra la ayuda, incluidas las opciones genéricas de Qt.");
            }
        }
        return {};
    }

    bool isEmpty() const override { return false; }
};

} // namespace

// ============================================================================
// Punto de entrada
// ============================================================================
int main(int argc, char *argv[])
{
    // --- 1. Capturar los argumentos originales ANTES de tocar nada ---
    // QApplication se va a construir con argv saneado, así que aquí es el
    // único punto donde podemos conservar los argumentos reales del usuario.
    // Usamos fromLocal8Bit porque argv viene codificado según el locale del
    // sistema, y Qt espera UTF-8 internamente.
    QStringList argumentosOriginales;
    argumentosOriginales.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        argumentosOriginales << QString::fromLocal8Bit(argv[i]);
    }

    // --- 2. Endurecer el entorno ANTES de crear QApplication ---
    // Qt lee estas variables al construirse. Limpiarlas elimina el vector
    // de ataque de carga de plugins desde rutas arbitrarias que pudieran
    // haber sido manipuladas por el entorno del usuario.
    // Los plugins del sistema se siguen cargando desde las rutas compiladas
    // por defecto, así que la aplicación funciona con normalidad.
    // Nota: unsetenv() es POSIX. En la fase de Windows habrá que sustituirlo
    // por _putenv_s() o equivalente.
#ifdef Q_OS_UNIX
    unsetenv("QT_PLUGIN_PATH");
    unsetenv("QT_QPA_PLATFORM_PLUGIN_PATH");
#endif

    // --- 3. Construir QApplication con argv saneado ---
    // Al pasar argc = 1, Qt no procesa ninguna opción genérica de la línea
    // de comandos (--platform, --session, --geometry, etc.). Todas las
    // opciones que el usuario haya pasado se gestionan explícitamente a
    // través de nuestro QCommandLineParser sobre argumentosOriginales.
    int argcLimpio = 1;
    char *argvLimpio[] = { argv[0], nullptr };
    QApplication aplicacion(argcLimpio, argvLimpio);

    // En Wayland, el icono se resuelve por el nombre del .desktop, no por
    // setWindowIcon(). Indicamos a Qt el nombre base del archivo .desktop
    // instalado en ~/.local/share/applications/ para que el compositor
    // asocie esta ventana con su entrada de escritorio.
    QGuiApplication::setDesktopFileName(QStringLiteral("sosvdm"));

    // --- 4. Configuración de la aplicación ---
    // Icono desde los recursos embebidos. La ruta debe coincidir con el
    // resultado de (prefix del .qrc + ruta del archivo dentro de <file>).
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/sosvdm.png")));

    // Metadatos de la aplicación (usados por QSettings, About, etc.).
    QCoreApplication::setApplicationName("sosvdm");
    QCoreApplication::setOrganizationName("sosvdm");
    QApplication::setApplicationDisplayName(
        QCoreApplication::translate("main", "Sólo otro simple visor de metadatos"));
    QCoreApplication::setApplicationVersion("0.4.0");

    // Traducciones internas de Qt (botones estándar, diálogos de archivo,
    // mensajes del propio framework). El archivo qtbase_es.qm lo instala
    // el paquete qt6-translations-l10n en Debian/Ubuntu.
    QTranslator traductorQt;
    if (traductorQt.load(QLocale(), "qtbase", "_",
                         QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&traductorQt);
    }

    // Traductor específico para las cadenas internas de QCommandLineParser
    // que no están cubiertas por qtbase_es.qm. Se instala después de
    // traductorQt para que Qt lo consulte con prioridad.
    TraductorAyudaLineaComandos traductorAyuda;
    QCoreApplication::installTranslator(&traductorAyuda);

    // Traductor de la propia aplicación. Carga el .qm embebido en los
    // recursos (prefijo :/i18n) que corresponda al idioma del sistema.
    // Si no existe el .qm del idioma activo, load() devuelve false y no
    // se instala: las cadenas se muestran tal cual están en el código.
    QTranslator traductorAplicacion;
    if (traductorAplicacion.load(QLocale(), "sosvdm", "_", ":/i18n")) {
        QCoreApplication::installTranslator(&traductorAplicacion);
    }

    QApplication::setStyle("Fusion"); // Base neutra para el stylesheet.

    QPalette paletaOscura;
    paletaOscura.setColor(QPalette::Window,          QColor(53, 53, 53));
    paletaOscura.setColor(QPalette::WindowText,      Qt::white);
    paletaOscura.setColor(QPalette::Base,            QColor(35, 35, 35));
    paletaOscura.setColor(QPalette::AlternateBase,   QColor(45, 45, 45));
    paletaOscura.setColor(QPalette::Text,            Qt::white);
    paletaOscura.setColor(QPalette::Button,          QColor(53, 53, 53));
    paletaOscura.setColor(QPalette::ButtonText,      Qt::white);
    paletaOscura.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    paletaOscura.setColor(QPalette::HighlightedText, Qt::black);
    QApplication::setPalette(paletaOscura);

    // --- 5. Análisis de argumentos ---
    const auto opciones = analizarLineaComandos(argumentosOriginales);
    if (!opciones.has_value()) {
        // QCommandLineParser ya habrá mostrado el mensaje adecuado.
        return EXIT_FAILURE;
    }

    // --- 6. Comprobación de compatibilidad extendida (interna) ---
    // Se evalúa antes que el resto de modos para darle prioridad.
    if (opciones->segarroAmeGo) {
        std::cout << "Comprobación de compatibilidad superada.\n";
        return EXIT_SUCCESS;
    }

    // --- 7. Modo terminal: solo metadatos ---
    if (opciones->soloMetadatos) {
        if (opciones->rutaImagen.isEmpty()) {
            qCritical() << "Error: el modo --metadata requiere una ruta de imagen.";
            return EXIT_FAILURE;
        }

        auto resultado = ExtractorMetadatos{}.extraer(opciones->rutaImagen);
        if (!resultado.has_value()) {
            qCritical() << "Error:" << resultado.error();
            return EXIT_FAILURE;
        }

        const ResultadoMetadatos &metadatos = resultado.value();
        switch (metadatos.estado) {
        case EstadoMetadatos::Ok:
            ExtractorMetadatos::imprimirEnSalidaEstandar(metadatos.datos);
            break;
        case EstadoMetadatos::SinMetadatos:
            std::cout << "Este formato admite metadatos, pero la imagen no "
                         "contiene ninguno EXIF, IPTC ni XMP.\n";
            break;
        case EstadoMetadatos::FormatoNoSoportado:
            std::cout << "El formato de imagen no soporta consulta de metadatos.\n";
            break;
        }

        return EXIT_SUCCESS;
    }

    // --- 8. Modo GUI ---
    VentanaPrincipal ventana;
    ventana.show();

    if (!opciones->rutaImagen.isEmpty()) {
        ventana.cargarImagen(opciones->rutaImagen);
    }

    return QCoreApplication::exec();
}
