// ============================================================================
// main.cpp - Punto de entrada de "Sólo otro simple visor de metadatos"
// ============================================================================
// Este archivo es responsable de:
//   1. Endurecer el entorno antes de inicializar Qt.
//   2. Inicializar la aplicación Qt (QApplication) con argv saneado.
//   3. Parsear los argumentos de línea de comandos.
//   4. Gestionar la comprobación de compatibilidad extendida.
//   5. Decidir entre el modo GUI y el modo CLI (solo metadatos).
//   6. Crear y mostrar la ventana principal.
// ============================================================================

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>

#include "mainwindow.h"
#include "metadataextractor.h"

#include <expected>
#include <string>
#include <cstdlib>   // unsetenv()

#include <iostream>

#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

// Estructura que encapsula las opciones parseadas de la línea de comandos.
struct CliOptions {
    QString     imagePath;                // Ruta a la imagen a cargar (opcional).
    bool        showMetadataOnly = false; // Modo CLI: solo metadatos.
    bool        verbose          = false; // Salida detallada.
    bool        segarroAmeGo     = false; // Opción de compatibilidad extendida.
};

// Parsea los argumentos y rellena la estructura CliOptions.
//
// Recibe los argumentos como QStringList en lugar de leerlos de
// app.arguments(), porque main() construye QApplication con argv saneado
// (argc = 1) y por tanto app.arguments() solo devolvería el nombre del
// programa. Los argumentos reales se capturan antes de crear QApplication.
//
// Devuelve std::nullopt si el usuario pidió ayuda/versión (QCommandLineParser
// ya se encarga de mostrar el texto y salir) o si hubo un error.
std::optional<CliOptions> parseCommandLine(QCommandLineParser &parser,
                                           const QStringList &args)
{
    // Definimos las opciones que acepta el programa.
    const QCommandLineOption metadataOption(
        QStringList{"m", "metadata"},
        QCoreApplication::translate("main", "Muestra solo los metadatos y sale (modo CLI)."));

    const QCommandLineOption verboseOption(
        QStringList{"D", "verbose"},
        QCoreApplication::translate("main", "Muestra información detallada por consola."));

    parser.addOption(metadataOption);
    parser.addOption(verboseOption);

    // Añadimos un argumento posicional: la ruta de la imagen.
    parser.addPositionalArgument(
        "imagen",
        QCoreApplication::translate("main", "Ruta al archivo de imagen."),
        "[imagen]");

    parser.addHelpOption();
    parser.addVersionOption();

    // Opción reservada. No aparece en --help. Reservada para uso interno
    // y comprobaciones de compatibilidad.
    QCommandLineOption segarroOption(
        QStringList{"segarro-amego"},
        QCoreApplication::translate("main", "Opción reservada."));
    segarroOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(segarroOption);

    // process() parsea los argumentos. En Qt6 ya no devuelve bool: si hay
    // error, o si el usuario pide --help/--version, muestra el mensaje y
    // llama a exit() internamente. Por eso no comprobamos su valor.
    parser.process(args);

    CliOptions opts;

    opts.segarroAmeGo = parser.isSet(segarroOption);

    opts.showMetadataOnly = parser.isSet(metadataOption);
    opts.verbose          = parser.isSet(verboseOption);

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        opts.imagePath = positional.first();

        // Validación temprana: comprobar que el archivo existe y es legible.
        const QFileInfo fi(opts.imagePath);
        if (!fi.exists() || !fi.isFile() || !fi.isReadable()) {
            qCritical() << "Error: no se puede leer el archivo:" << opts.imagePath;
            return std::nullopt;
        }
    }

    return opts;
}

/**
 * @brief Traductor mínimo para las cadenas internas de QCommandLineParser.
 *
 * Qt no traduce al español estas dos cadenas concretas del contexto
 * "QCommandLineParser". En lugar de reimplementar toda la salida de ayuda,
 * este traductor intercepta solo esas dos y deja pasar el resto a los
 * traductores estándar (qtbase_es.qm).
 *
 * Se instala DESPUÉS de qtTranslator para que Qt lo consulte primero
 * (los traductores se buscan en orden inverso al de instalación).
 */
class CommandLineHelpTranslator : public QTranslator
{
public:
    using QTranslator::QTranslator;

    QString translate(const char *context,
                      const char *sourceText,
                      const char *disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (qstrcmp(context, "QCommandLineParser") == 0) {
            if (qstrcmp(sourceText, "Displays help on commandline options.") == 0) {
                return QStringLiteral(
                    "Muestra la ayuda sobre las opciones de línea de comandos.");
            }
            if (qstrcmp(sourceText, "Displays help, including generic Qt options.") == 0) {
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
    QStringList originalArgs;
    originalArgs.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        originalArgs << QString::fromLocal8Bit(argv[i]);
    }

    // --- 2. Endurecer el entorno ANTES de crear QApplication ---
    // Qt lee estas variables al construirse. Limpiarlas elimina el vector
    // de ataque de carga de plugins desde rutas arbitrarias que pudieran
    // haber sido manipuladas por el entorno del usuario.
    // Los plugins del sistema se siguen cargando desde las rutas compiladas
    // por defecto, así que la aplicación funciona con normalidad.
    unsetenv("QT_PLUGIN_PATH");
    unsetenv("QT_QPA_PLATFORM_PLUGIN_PATH");

    // --- 3. Construir QApplication con argv saneado ---
    // Al pasar argc = 1, Qt no procesa ninguna opción genérica de la línea
    // de comandos (--platform, --session, --geometry, etc.). Todas las
    // opciones que el usuario haya pasado se gestionan explícitamente a
    // través de nuestro QCommandLineParser sobre originalArgs.
    int cleanArgc = 1;
    char *cleanArgv[] = { argv[0], nullptr };
    QApplication app(cleanArgc, cleanArgv);

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
    QApplication::setApplicationDisplayName(QObject::tr("Sólo otro simple visor de metadatos"));
    QCoreApplication::setApplicationVersion("0.3.5");

    // Traducciones internas de Qt (botones estándar, diálogos de archivo,
    // mensajes del propio framework). El archivo qtbase_es.qm lo instala
    // el paquete qt6-translations-l10n en Debian/Ubuntu.
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), "qtbase", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Traductor específico para las cadenas internas de QCommandLineParser
    // que no están cubiertas por qtbase_es.qm. Se instala después de
    // qtTranslator para que Qt lo consulte con prioridad.
    CommandLineHelpTranslator helpTranslator;
    app.installTranslator(&helpTranslator);

    // Traductor de la propia aplicación. Carga el .qm embebido en los
    // recursos (prefijo :/i18n) que corresponda al idioma del sistema.
    // Si no existe el .qm del idioma activo, load() devuelve false y no
    // se instala: las cadenas se muestran tal cual están en el código.
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), "sosvdm", "_", ":/i18n")) {
        app.installTranslator(&appTranslator);
    }

    app.setStyle("Fusion"); // Base neutra para el stylesheet.

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText,      Qt::white);
    darkPalette.setColor(QPalette::Base,            QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase,   QColor(45, 45, 45));
    darkPalette.setColor(QPalette::Text,            Qt::white);
    darkPalette.setColor(QPalette::Button,          QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText,      Qt::white);
    darkPalette.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);

    // --- 5. Parseo de argumentos ---
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main",
            "Visor imágenes con extracción de metadatos.\n"
            "Nada más que ver por aquí."));

    const auto options = parseCommandLine(parser, originalArgs);
    if (!options.has_value()) {
        // QCommandLineParser ya habrá mostrado el mensaje adecuado.
        return EXIT_FAILURE;
    }

    // --- 6. Comprobación de compatibilidad extendida (interna) ---
    // Se evalúa antes que el resto de modos para darle prioridad.
    if (options->segarroAmeGo) {
        std::cout << "Comprobación de compatibilidad superada.\n";
        return EXIT_SUCCESS;
    }

    // --- 7. Modo CLI: solo metadatos ---
    if (options->showMetadataOnly) {
        if (options->imagePath.isEmpty()) {
            qCritical() << "Error: el modo --metadata requiere una ruta de imagen.";
            return EXIT_FAILURE;
        }

        MetadataExtractor extractor;
        auto result = extractor.extract(options->imagePath);
        if (!result.has_value()) {
            qCritical() << "Error:" << result.error();
            return EXIT_FAILURE;
        }

        const MetadataResult &meta = result.value();
        switch (meta.status) {
        case MetadataStatus::Ok:
            MetadataExtractor::printToStdout(meta.data);
            break;
        case MetadataStatus::NoMetadata:
            std::cout << "Este formato admite metadatos, pero la imagen no "
                         "contiene ninguno EXIF, IPTC ni XMP.\n";
            break;
        case MetadataStatus::UnsupportedFormat:
            std::cout << "El formato de imagen no soporta consulta de metadatos.\n";
            break;
        }

        return EXIT_SUCCESS;
    }

    // --- 8. Modo GUI ---
    MainWindow window;
    window.show();

    if (!options->imagePath.isEmpty()) {
        window.loadImage(options->imagePath);
    }

    return app.exec();
}
