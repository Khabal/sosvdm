// ============================================================================
// ventanaprincipal.cpp - Implementación de la ventana principal
// ============================================================================
#include "ventanaprincipal.h"
#include "ui_ventanaprincipal.h"

#include "extractormetadatos.h"
#include "manejadorimagen.h"
#include "vistagraficanivelampliacion.h"

#include <algorithm>

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QLabel>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextDocument>
#include <QPainter>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QTransform>
#include <QTreeWidgetItem>

// ============================================================================
// Constructor / Destructor
// ============================================================================

VentanaPrincipal::VentanaPrincipal(QWidget *padre)
    : QMainWindow(padre)
    , m_interfaz(std::make_unique<Ui::VentanaPrincipal>())
    , m_manejadorImagen(std::make_unique<ManejadorImagen>())
    , m_extractorMetadatos(std::make_unique<ExtractorMetadatos>())
{
    m_interfaz->setupUi(this);

    configurarInterfaz();
    configurarConexiones();

    actualizarBarraEstado(tr("Listo. Abre una imagen para comenzar."));
}

VentanaPrincipal::~VentanaPrincipal() = default;

// ============================================================================
// Configuración inicial
// ============================================================================

void VentanaPrincipal::configurarInterfaz()
{
    setWindowTitle(QApplication::applicationDisplayName());

    // --- Árbol de metadatos ---
    m_interfaz->arbolMetadatos->setColumnCount(2);
    m_interfaz->arbolMetadatos->setHeaderLabels({tr("Propiedad"), tr("Valor")});
    m_interfaz->arbolMetadatos->setAlternatingRowColors(true);
    m_interfaz->arbolMetadatos->setRootIsDecorated(true);

    // --- Escena y vista de imagen ---
    m_escena = new QGraphicsScene(this);
    m_interfaz->vistaGrafica->setScene(m_escena);
    m_interfaz->vistaGrafica->setRenderHint(QPainter::SmoothPixmapTransform);
    m_interfaz->vistaGrafica->setDragMode(QGraphicsView::ScrollHandDrag);
    m_interfaz->vistaGrafica->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // --- Proporciones del divisor ---
    // La imagen ocupa la mayor parte del espacio; los metadatos quedan
    // en un panel estrecho a la derecha.
    m_interfaz->divisor->setStretchFactor(0, 1);
    m_interfaz->divisor->setStretchFactor(1, 0);
    m_interfaz->divisor->setSizes({700, 300});
    m_interfaz->arbolMetadatos->setMinimumWidth(220);

    m_elementoPixmap = new QGraphicsPixmapItem();
    m_elementoPixmap->setTransformationMode(Qt::SmoothTransformation);
    m_escena->addItem(m_elementoPixmap);

    connect(m_interfaz->vistaGrafica, &VistaGraficaNivelAmpliacion::pasoNivelAmpliacionSolicitado,
            this, &VentanaPrincipal::aplicarPasoNivelAmpliacion);

    // --- Timer del modo presentación ---
    m_temporizadorPresentacion = new QTimer(this);
    m_temporizadorPresentacion->setInterval(5000);
    connect(m_temporizadorPresentacion, &QTimer::timeout, this, &VentanaPrincipal::alSiguienteDiapositiva);

    // --- Configuración de iconos de las QAction ---
    // Si el tema activo tiene el icono, se usa; si no, se recurre al
    // recurso SVG embebido. Así se ve nativo en Linux y sigue funcionando
    // en plataformas sin temas de iconos.
    auto cargarIcono = [](const QString &nombreTema, const QString &rutaAlternativa) {
        QIcon icono = QIcon::fromTheme(nombreTema);
        if (icono.isNull()) {
            icono = QIcon(rutaAlternativa);
        }
        return icono;
    };

    m_interfaz->accionImagenAnterior ->setIcon(cargarIcono("go-previous",         ":/icons/prev.svg"));
    m_interfaz->accionSiguienteImagen ->setIcon(cargarIcono("go-next",             ":/icons/next.svg"));
    m_interfaz->accionAcercar        ->setIcon(cargarIcono("zoom-in",             ":/icons/zoom-in.svg"));
    m_interfaz->accionAlejar         ->setIcon(cargarIcono("zoom-out",            ":/icons/zoom-out.svg"));
    m_interfaz->accionAjustarVentana ->setIcon(cargarIcono("zoom-fit-best",       ":/icons/zoom-fit.svg"));
    m_interfaz->accionRotarIzquierda ->setIcon(cargarIcono("object-rotate-left",  ":/icons/rotate-left.svg"));
    m_interfaz->accionRotarDerecha   ->setIcon(cargarIcono("object-rotate-right", ":/icons/rotate-right.svg"));
    m_interfaz->accionAlternarMetadatos->setIcon(cargarIcono("dialog-information",  ":/icons/metadata.svg"));
    m_interfaz->accionPantallaCompleta->setIcon(cargarIcono("view-fullscreen",     ":/icons/fullscreen.svg"));
    m_interfaz->accionPresentacion   ->setIcon(cargarIcono("media-playback-start",":/icons/slideshow.svg"));

    // --- Construcción de la barra de herramientas ---
    // Se reutilizan las mismas QAction del menú: una sola fuente de verdad.
    // El orden replica el del menú Visualización, con el botón de metadatos
    // al final (para coincidir también con su posición en el menú).
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionImagenAnterior);
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionSiguienteImagen);
    m_interfaz->barraHerramientas->addSeparator();
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionAlejar);

    m_etiquetaNivelAmpliacion = new QLabel(QStringLiteral("100%"), this);
    m_etiquetaNivelAmpliacion->setMinimumWidth(55);
    m_etiquetaNivelAmpliacion->setAlignment(Qt::AlignCenter);
    m_interfaz->barraHerramientas->addWidget(m_etiquetaNivelAmpliacion);

    m_interfaz->barraHerramientas->addAction(m_interfaz->accionAcercar);
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionAjustarVentana);
    m_interfaz->barraHerramientas->addSeparator();
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionRotarIzquierda);
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionRotarDerecha);
    m_interfaz->barraHerramientas->addSeparator();
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionPantallaCompleta);
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionPresentacion);
    m_interfaz->barraHerramientas->addSeparator();
    m_interfaz->barraHerramientas->addAction(m_interfaz->accionAlternarMetadatos);
}

void VentanaPrincipal::configurarConexiones()
{
    // --- Menú Archivo ---
    connect(m_interfaz->accionAbrir,           &QAction::triggered, this, &VentanaPrincipal::alAbrirArchivo);
    connect(m_interfaz->accionCerrar,          &QAction::triggered, this, &VentanaPrincipal::alCerrarImagen);
    connect(m_interfaz->accionSalir,           &QAction::triggered, this, &VentanaPrincipal::alSalir);
    connect(m_interfaz->accionImprimirImagen,  &QAction::triggered, this, &VentanaPrincipal::alImprimirImagen);
    connect(m_interfaz->accionImprimirMetadatos, &QAction::triggered, this, &VentanaPrincipal::alImprimirMetadatos);
    connect(m_interfaz->accionExportarMetadatos, &QAction::triggered, this, &VentanaPrincipal::alExportarMetadatos);

    // --- Menú Visualización ---
    connect(m_interfaz->accionSiguienteImagen, &QAction::triggered, this, &VentanaPrincipal::alSiguienteImagen);
    connect(m_interfaz->accionImagenAnterior,  &QAction::triggered, this, &VentanaPrincipal::alImagenAnterior);
    connect(m_interfaz->accionAcercar,         &QAction::triggered, this, &VentanaPrincipal::alAcercar);
    connect(m_interfaz->accionAlejar,          &QAction::triggered, this, &VentanaPrincipal::alAlejar);
    connect(m_interfaz->accionAjustarVentana,  &QAction::triggered, this, &VentanaPrincipal::alAjustarVentana);
    connect(m_interfaz->accionRotarDerecha,    &QAction::triggered, this, &VentanaPrincipal::alRotarDerecha);
    connect(m_interfaz->accionRotarIzquierda,  &QAction::triggered, this, &VentanaPrincipal::alRotarIzquierda);
    connect(m_interfaz->accionPantallaCompleta, &QAction::triggered, this, &VentanaPrincipal::alAlternarPantallaCompleta);
    connect(m_interfaz->accionPresentacion,    &QAction::toggled,   this, &VentanaPrincipal::alAlternarModoPresentacion);
    connect(m_interfaz->accionAlternarMetadatos, &QAction::toggled,   this, &VentanaPrincipal::alAlternarPanelMetadatos);

    // --- Menú Ayuda ---
    connect(m_interfaz->accionAtajos,    &QAction::triggered, this, &VentanaPrincipal::alMostrarAtajos);
    connect(m_interfaz->accionAcercaDe,  &QAction::triggered, this, &VentanaPrincipal::alAcercaDe);

    // --- Atajos sin entrada en menú ---

    // Escape sale de pantalla completa o presentación.
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this),
            &QShortcut::activated, this, &VentanaPrincipal::alPulsarEscape);

    // Atajo reservado para uso interno. Se usa Ctrl+Shift+5 (en lugar de
    // Ctrl+Shift+0) para evitar pulsaciones accidentales junto a Ctrl+0.
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_5), this),
            &QShortcut::activated, this, &VentanaPrincipal::alAtajoReservado);

    // --- Atajos de nivel de ampliación asignados por código ---
    // "Ctrl++" no se parsea bien desde el .ui; aquí se incluyen varias
    // variantes para máxima compatibilidad entre layouts de teclado.
    m_interfaz->accionAcercar->setShortcuts({QKeySequence::ZoomIn,  QKeySequence("Ctrl+=")});
    m_interfaz->accionAlejar ->setShortcuts({QKeySequence::ZoomOut, QKeySequence("Ctrl+-")});
}

// ============================================================================
// Carga de imagen
// ============================================================================

void VentanaPrincipal::cargarImagen(const QString &rutaArchivo)
{
    if (rutaArchivo.isEmpty()) {
        return;
    }

    // --- 1. Cargar la imagen con OpenCV ---
    auto resultadoImagen = m_manejadorImagen->cargarComoImagenQ(rutaArchivo);
    if (!resultadoImagen.has_value()) {
        QMessageBox::warning(this, tr("Error al cargar imagen"),
                             resultadoImagen.error());
        actualizarBarraEstado(tr("Error: %1").arg(resultadoImagen.error()));
        return;
    }

    const QImage &imagen = resultadoImagen.value();
    m_elementoPixmap->setPixmap(QPixmap::fromImage(imagen));
    m_escena->setSceneRect(m_elementoPixmap->boundingRect());

    // Escala inicial: si la imagen cabe en la vista, mostrarla al 100%;
    // si es más grande, ajustarla a ventana.
    const QSize tamanoImagen = m_elementoPixmap->pixmap().size();
    const QSize tamanoVista  = m_interfaz->vistaGrafica->viewport()->size();
    if (tamanoImagen.width() <= tamanoVista.width()
        && tamanoImagen.height() <= tamanoVista.height()) {
        m_interfaz->vistaGrafica->resetTransform();
    } else {
        m_interfaz->vistaGrafica->fitInView(m_elementoPixmap, Qt::KeepAspectRatio);
    }

    // --- 2. Extraer metadatos con Exiv2 ---
    auto resultadoMetadatos = m_extractorMetadatos->extraer(rutaArchivo);
    m_interfaz->arbolMetadatos->clear();
    m_metadatosActuales.clear();
    m_estadoMetadatosActuales = EstadoMetadatos::SinMetadatos;

    if (!resultadoMetadatos.has_value()) {
        auto *elementoError = new QTreeWidgetItem(m_interfaz->arbolMetadatos);
        elementoError->setText(0, tr("Error"));
        elementoError->setText(1, resultadoMetadatos.error());
    } else {
        const ResultadoMetadatos &metadatos = resultadoMetadatos.value();
        m_estadoMetadatosActuales = metadatos.estado;

        switch (metadatos.estado) {
        case EstadoMetadatos::Ok:
            m_metadatosActuales = metadatos.datos;
            for (const auto &[categoria, entradas] : m_metadatosActuales) {
                auto *elementoCategoria = new QTreeWidgetItem(m_interfaz->arbolMetadatos);
                elementoCategoria->setText(0, QString::fromStdString(categoria));
                elementoCategoria->setExpanded(true);
                for (const auto &[clave, valor] : entradas) {
                    auto *elementoEntrada = new QTreeWidgetItem(elementoCategoria);
                    elementoEntrada->setText(0, QString::fromStdString(clave));
                    elementoEntrada->setText(1, QString::fromStdString(valor));
                }
            }
            m_interfaz->arbolMetadatos->resizeColumnToContents(0);
            break;

        case EstadoMetadatos::SinMetadatos: {
            auto *elementoInfo = new QTreeWidgetItem(m_interfaz->arbolMetadatos);
            elementoInfo->setText(0, tr("Sin metadatos"));
            elementoInfo->setText(1,
                tr("Este formato admite metadatos, pero la imagen no contiene "
                   "ninguno EXIF, IPTC ni XMP."));
            break;
        }

        case EstadoMetadatos::FormatoNoSoportado: {
            auto *elementoInfo = new QTreeWidgetItem(m_interfaz->arbolMetadatos);
            elementoInfo->setText(0, tr("Formato no soportado"));
            elementoInfo->setText(1,
                tr("El formato de imagen no soporta consulta de metadatos."));
            break;
        }
        }
    }

    // --- 3. Actualizar estado interno y navegación ---
    m_rutaArchivoActual = rutaArchivo;
    reconstruirListaDirectorio();
    m_interfaz->accionExportarMetadatos->setEnabled(true);
    m_interfaz->accionImprimirImagen   ->setEnabled(true);
    m_interfaz->accionImprimirMetadatos->setEnabled(true);

    actualizarBarraEstado(tr("Cargada: %1 (%2x%3)")
                              .arg(QFileInfo(rutaArchivo).fileName())
                              .arg(imagen.width())
                              .arg(imagen.height()));

    actualizarEtiquetaNivelAmpliacion();
}

// ============================================================================
// Slots - Menú Archivo
// ============================================================================

void VentanaPrincipal::alAbrirArchivo()
{
    const QString filtroTodos =
        tr("Todas las imágenes soportadas (%1)")
            .arg(ManejadorImagen::extensionesSoportadas().join(' '));

    const QStringList listaFiltros = {
        filtroTodos,
        tr("JPEG (*.jpg *.jpeg *.jpe)"),
        tr("PNG (*.png)"),
        tr("TIFF (*.tif *.tiff)"),
        tr("WebP (*.webp)"),
        tr("JPEG 2000 (*.jp2)"),
        tr("Bitmap (*.bmp *.dib)"),
        tr("GIF (*.gif)"),
        tr("Targa (*.tga)"),
        tr("Netpbm (*.pbm *.pgm *.ppm *.pnm *.pxm)"),
        tr("Radiance HDR (*.hdr *.pic)"),
        tr("OpenEXR (*.exr)"),
        tr("Sun Raster (*.sr *.ras)"),
        tr("PFM (*.pfm)"),
        tr("Todos los archivos (*)"),
    };

    const QString rutaArchivo = QFileDialog::getOpenFileName(
        this, tr("Abrir imagen"), QString(), listaFiltros.join(";;"));

    if (!rutaArchivo.isEmpty()) {
        cargarImagen(rutaArchivo);
    }
}

void VentanaPrincipal::alCerrarImagen()
{
    limpiarVista();
    actualizarBarraEstado(tr("Imagen cerrada."));
}

void VentanaPrincipal::alSalir()
{
    close();
}

void VentanaPrincipal::alImprimirImagen()
{
    if (m_elementoPixmap->pixmap().isNull()) {
        return;
    }

    QPrinter impresora(QPrinter::HighResolution);
    QPrintDialog dialogo(&impresora, this);
    dialogo.setWindowTitle(tr("Imprimir imagen"));

    if (dialogo.exec() != QDialog::Accepted) {
        return;
    }

    QPainter pintor(&impresora);
    const QRectF areaImpresion = pintor.viewport();
    const QPixmap pixmap = m_elementoPixmap->pixmap();

    // Escalar la imagen para que quepa en la página manteniendo la proporción.
    QSize tamanoImpresion = pixmap.size();
    tamanoImpresion.scale(areaImpresion.size().toSize(), Qt::KeepAspectRatio);

    const QRectF rectanguloDestino(
        QPointF(areaImpresion.center().x() - tamanoImpresion.width() / 2.0,
                areaImpresion.center().y() - tamanoImpresion.height() / 2.0),
        tamanoImpresion);

    pintor.drawPixmap(rectanguloDestino, pixmap, pixmap.rect());
    pintor.end();
}

void VentanaPrincipal::alImprimirMetadatos()
{
    if (m_rutaArchivoActual.isEmpty()) {
        return;
    }

    QPrinter impresora(QPrinter::HighResolution);
    QPrintDialog dialogo(&impresora, this);
    dialogo.setWindowTitle(tr("Imprimir metadatos"));

    if (dialogo.exec() != QDialog::Accepted) {
        return;
    }

    // Construir el texto completo: cabecera + cuerpo de metadatos.
    QString texto = construirCabeceraImagen();

    switch (m_estadoMetadatosActuales) {
    case EstadoMetadatos::Ok:
        texto += ExtractorMetadatos::formatearComoTexto(m_metadatosActuales);
        break;
    case EstadoMetadatos::SinMetadatos:
        texto += "[Metadatos]\n"
                 "  Sin metadatos = La imagen no contiene metadatos EXIF, IPTC ni XMP.\n";
        break;
    case EstadoMetadatos::FormatoNoSoportado:
        texto += "[Metadatos]\n"
                 "  Formato no soportado = El formato de imagen no soporta "
                 "consulta de metadatos.\n";
        break;
    }

    QTextDocument documento;
    documento.setPlainText(texto);
    documento.print(&impresora);
}

void VentanaPrincipal::alExportarMetadatos()
{
    if (m_rutaArchivoActual.isEmpty()) {
        QMessageBox::information(this, tr("Exportar metadatos"),
            tr("No hay ninguna imagen cargada."));
        return;
    }

    QString nombreSugerido = QStringLiteral("metadata.txt");
    {
        const QFileInfo infoArchivo(m_rutaArchivoActual);
        nombreSugerido = infoArchivo.completeBaseName() + QStringLiteral("_metadata.txt");
    }

    const QString ruta = QFileDialog::getSaveFileName(
        this, tr("Exportar metadatos"), nombreSugerido,
        tr("Archivos de texto (*.txt);;Todos los archivos (*)"));

    if (ruta.isEmpty()) {
        return;
    }

    QFile archivo(ruta);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Error al exportar"),
            tr("No se pudo abrir el archivo: %1").arg(archivo.errorString()));
        return;
    }

    QTextStream flujo(&archivo);
    flujo.setEncoding(QStringConverter::Utf8);
    flujo << construirCabeceraImagen();

    switch (m_estadoMetadatosActuales) {
    case EstadoMetadatos::Ok:
        flujo << ExtractorMetadatos::formatearComoTexto(m_metadatosActuales);
        break;
    case EstadoMetadatos::SinMetadatos:
        flujo << "[Metadatos]\n"
              << "  Sin metadatos = La imagen no contiene metadatos EXIF, IPTC ni XMP.\n";
        break;
    case EstadoMetadatos::FormatoNoSoportado:
        flujo << "[Metadatos]\n"
              << "  Formato no soportado = El formato de imagen no soporta "
                 "consulta de metadatos.\n";
        break;
    }

    archivo.close();
    actualizarBarraEstado(tr("Metadatos exportados a: %1").arg(ruta));
}

// ============================================================================
// Slots - Menú Visualización
// ============================================================================

void VentanaPrincipal::alSiguienteImagen()
{
    if (m_imagenesDirectorio.isEmpty()) {
        return;
    }
    m_indiceActual = (m_indiceActual + 1) % m_imagenesDirectorio.size();
    mostrarImagenEn(m_indiceActual);
}

void VentanaPrincipal::alImagenAnterior()
{
    if (m_imagenesDirectorio.isEmpty()) {
        return;
    }
    m_indiceActual = (m_indiceActual - 1 + m_imagenesDirectorio.size()) % m_imagenesDirectorio.size();
    mostrarImagenEn(m_indiceActual);
}

void VentanaPrincipal::alAcercar()
{
    aplicarPasoNivelAmpliacion(kPasoNivelAmpliacionBoton);
}

void VentanaPrincipal::alAlejar()
{
    aplicarPasoNivelAmpliacion(-kPasoNivelAmpliacionBoton);
}

void VentanaPrincipal::alAjustarVentana()
{
    if (m_elementoPixmap->pixmap().isNull()) {
        return;
    }
    m_interfaz->vistaGrafica->resetTransform();
    m_interfaz->vistaGrafica->fitInView(m_elementoPixmap, Qt::KeepAspectRatio);
    actualizarEtiquetaNivelAmpliacion();
}

void VentanaPrincipal::alRotarDerecha()        { rotarImagenActual(90); }
void VentanaPrincipal::alRotarIzquierda()      { rotarImagenActual(-90); }

void VentanaPrincipal::alAlternarPantallaCompleta()
{
    if (isFullScreen()) {
        showNormal();
        menuBar()->setVisible(true);
        m_interfaz->barraHerramientas->setVisible(true);
        statusBar()->setVisible(true);
        // Restaurar visibilidad del panel según el estado del action,
        // que es la fuente de verdad.
        m_interfaz->arbolMetadatos->setVisible(m_interfaz->accionAlternarMetadatos->isChecked());
    } else {
        menuBar()->setVisible(false);
        m_interfaz->barraHerramientas->setVisible(false);
        statusBar()->setVisible(false);
        m_interfaz->arbolMetadatos->setVisible(false);
        showFullScreen();
    }
}

void VentanaPrincipal::alAlternarModoPresentacion(bool activo)
{
    if (activo) {
        if (m_imagenesDirectorio.isEmpty()) {
            QMessageBox::information(this, tr("Modo presentación"),
                tr("No hay imágenes en el directorio actual."));
            // Revertir el check sin re-disparar el slot.
            QSignalBlocker bloqueador(m_interfaz->accionPresentacion);
            m_interfaz->accionPresentacion->setChecked(false);
            return;
        }
        menuBar()->setVisible(false);
        m_interfaz->barraHerramientas->setVisible(false);
        statusBar()->setVisible(false);
        m_interfaz->arbolMetadatos->setVisible(false);
        showFullScreen();
        m_temporizadorPresentacion->start();
        actualizarBarraEstado(tr("Modo presentación activo (5 s por imagen)."));
    } else {
        m_temporizadorPresentacion->stop();
        showNormal();
        menuBar()->setVisible(true);
        m_interfaz->barraHerramientas->setVisible(true);
        statusBar()->setVisible(true);
        m_interfaz->arbolMetadatos->setVisible(m_interfaz->accionAlternarMetadatos->isChecked());
        actualizarBarraEstado(tr("Modo presentación detenido."));
    }
}

void VentanaPrincipal::alAlternarPanelMetadatos(bool visible)
{
    // El estado del panel es la fuente de verdad del QAction checkable:
    // menú, toolbar y atajo M comparten el mismo objeto y quedan siempre
    // sincronizados.
    m_interfaz->arbolMetadatos->setVisible(visible);
}

// ============================================================================
// Slots - Menú Ayuda
// ============================================================================

void VentanaPrincipal::alMostrarAtajos()
{
    // Este atajo no se incluye en la tabla: es de uso interno.
    const QString html = tr(
        "<h3>Atajos de teclado</h3>"
        "<table cellspacing='6'>"
        "<tr><td colspan='2'><b>Archivo</b></td></tr>"
        "<tr><td>Abrir</td><td>Ctrl+O</td></tr>"
        "<tr><td>Cerrar</td><td>Ctrl+W</td></tr>"
        "<tr><td>Salir</td><td>Ctrl+Q</td></tr>"
        "<tr><td colspan='2'><b>Visualización</b></td></tr>"
        "<tr><td>Imagen siguiente</td><td>Space</td></tr>"
        "<tr><td>Imagen anterior</td><td>Backspace</td></tr>"
        "<tr><td>Acercar</td><td>Ctrl++</td></tr>"
        "<tr><td>Alejar</td><td>Ctrl+-</td></tr>"
        "<tr><td>Ajustar a ventana</td><td>Ctrl+0</td></tr>"
        "<tr><td>Rotar 90º a la derecha</td><td>R</td></tr>"
        "<tr><td>Rotar 90º a la izquierda</td><td>Shift+R</td></tr>"
        "<tr><td>Modo pantalla completa</td><td>F11</td></tr>"
        "<tr><td>Modo presentación</td><td>F5</td></tr>"
        "<tr><td>Mostrar/ocultar metadatos</td><td>M</td></tr>"
        "<tr><td colspan='2'><b>Ayuda</b></td></tr>"
        "<tr><td>Atajos de teclado</td><td>F1</td></tr>"
        "</table>");

    QMessageBox mensaje(this);
    mensaje.setWindowTitle(tr("Atajos de teclado"));
    mensaje.setTextFormat(Qt::RichText);
    mensaje.setText(html);
    mensaje.setStandardButtons(QMessageBox::Ok);
    mensaje.exec();
}

void VentanaPrincipal::alAcercaDe()
{
    QMessageBox::about(
        this,
        tr("Acerca de"),
        tr("<h3>%1 %2</h3>"
           "<p>Visor de imágenes con extracción de metadatos.</p>"
           "<p><b>Tecnologías:</b> C++23, Qt6, OpenCV, Exiv2.</p>")
            .arg(QApplication::applicationDisplayName())
            .arg(QCoreApplication::applicationVersion()));
}

// ============================================================================
// Slots - Internos
// ============================================================================

void VentanaPrincipal::alSiguienteDiapositiva()
{
    alSiguienteImagen();
}

void VentanaPrincipal::alPulsarEscape()
{
    // Escape sale de presentación si está activa; si no, de fullscreen.
    if (m_interfaz->accionPresentacion->isChecked()) {
        m_interfaz->accionPresentacion->setChecked(false);  // dispara toggled(false)
    } else if (isFullScreen()) {
        alAlternarPantallaCompleta();
    }
}

void VentanaPrincipal::alAtajoReservado()
{
    // Marcador de posición para una funcionalidad reservada que se
    // desarrollará en fases posteriores. De momento confirma que el
    // atajo está registrado y operativo.
    QMessageBox::information(
        this,
        tr("Función reservada"),
        tr("Esta función está reservada para uso interno y no está "
           "disponible en esta versión."));
}

void VentanaPrincipal::aplicarPasoNivelAmpliacion(int variacionPorcentaje)
{
    if (m_elementoPixmap->pixmap().isNull()) {
        return;
    }

    const double porcentajeActual = m_interfaz->vistaGrafica->transform().m11() * 100.0;
    const double porcentajeObjetivo  = porcentajeActual + variacionPorcentaje;

    const double porcentajeAjustado = std::max(kNivelAmpliacionMinimo,
                                std::min(porcentajeObjetivo, kNivelAmpliacionMaximo));

    if (qFuzzyCompare(porcentajeAjustado, porcentajeActual)) {
        return;
    }

    const double factor = porcentajeAjustado / porcentajeActual;
    m_interfaz->vistaGrafica->scale(factor, factor);
    actualizarEtiquetaNivelAmpliacion();
}

// ============================================================================
// Auxiliares
// ============================================================================

void VentanaPrincipal::actualizarBarraEstado(const QString &mensaje)
{
    // Sin timeout: el mensaje permanece hasta que se sustituya por otro.
    statusBar()->showMessage(mensaje);
}

void VentanaPrincipal::limpiarVista()
{
    m_estadoMetadatosActuales = EstadoMetadatos::SinMetadatos;
    m_rutaArchivoActual.clear();
    m_metadatosActuales.clear();
    m_elementoPixmap->setPixmap(QPixmap());
    m_escena->setSceneRect(QRectF());
    m_interfaz->arbolMetadatos->clear();
    m_interfaz->accionExportarMetadatos->setEnabled(false);
    m_interfaz->accionImprimirImagen   ->setEnabled(false);
    m_interfaz->accionImprimirMetadatos->setEnabled(false);
}

void VentanaPrincipal::reconstruirListaDirectorio()
{
    m_imagenesDirectorio.clear();
    if (m_rutaArchivoActual.isEmpty()) {
        return;
    }

    const QFileInfo infoArchivo(m_rutaArchivoActual);
    QDir directorio = infoArchivo.absoluteDir();

    const QFileInfoList entradas = directorio.entryInfoList(
        ManejadorImagen::extensionesSoportadas(), QDir::Files, QDir::Name);

    for (const auto &entrada : entradas) {
        m_imagenesDirectorio << entrada.absoluteFilePath();
    }
    m_indiceActual = m_imagenesDirectorio.indexOf(m_rutaArchivoActual);
}

void VentanaPrincipal::mostrarImagenEn(int indice)
{
    if (indice < 0 || indice >= m_imagenesDirectorio.size()) {
        return;
    }
    cargarImagen(m_imagenesDirectorio.at(indice));
}

void VentanaPrincipal::rotarImagenActual(int grados)
{
    if (m_elementoPixmap->pixmap().isNull()) {
        return;
    }

    QTransform transformacion;
    transformacion.rotate(grados);
    const QPixmap rotada = m_elementoPixmap->pixmap().transformed(transformacion);
    m_elementoPixmap->setPixmap(rotada);
    m_escena->setSceneRect(m_elementoPixmap->boundingRect());
    m_interfaz->vistaGrafica->fitInView(m_elementoPixmap, Qt::KeepAspectRatio);
    actualizarEtiquetaNivelAmpliacion();
}

void VentanaPrincipal::actualizarEtiquetaNivelAmpliacion()
{
    if (!m_etiquetaNivelAmpliacion) {
        return;
    }
    const double escala = m_interfaz->vistaGrafica->transform().m11();
    m_etiquetaNivelAmpliacion->setText(QStringLiteral("%1%").arg(qRound(escala * 100.0)));
}

QString VentanaPrincipal::construirCabeceraImagen() const
{
    const QFileInfo infoArchivo(m_rutaArchivoActual);
    QString cabecera;
    QTextStream flujo(&cabecera);
    flujo << "=== DATOS DE LA IMAGEN ===\n"
          << "Nombre:  " << infoArchivo.fileName()         << '\n'
          << "Formato: " << infoArchivo.suffix().toUpper() << '\n'
          << "Ruta:    " << infoArchivo.absoluteFilePath() << "\n\n";
    return cabecera;
}
