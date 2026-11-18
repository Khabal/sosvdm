// ============================================================================
// mainwindow.cpp - Implementación de la ventana principal
// ============================================================================
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "imagehandler.h"
#include "metadataextractor.h"
#include "graphicsviewzoom.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QLabel>
#include <QMessageBox>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , m_imageHandler(std::make_unique<ImageHandler>())
    , m_metadataExtractor(std::make_unique<MetadataExtractor>())
{
    ui->setupUi(this);

    setupUi();
    setupConnections();

    updateStatusBar(tr("Listo. Abre una imagen para comenzar."));
}

MainWindow::~MainWindow() = default;

// ============================================================================
// Configuración inicial
// ============================================================================

void MainWindow::setupUi()
{
    setWindowTitle(QApplication::applicationDisplayName());

    // --- Árbol de metadatos ---
    ui->treeMetadata->setColumnCount(2);
    ui->treeMetadata->setHeaderLabels({tr("Propiedad"), tr("Valor")});
    ui->treeMetadata->setAlternatingRowColors(true);
    ui->treeMetadata->setRootIsDecorated(true);

    // --- Escena y vista de imagen ---
    m_scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // --- Proporciones del splitter ---
    // La imagen ocupa la mayor parte del espacio; los metadatos quedan
    // en un panel estrecho a la derecha.
    //
    //  * setStretchFactor(0, 1): el widget de imagen absorbe el espacio
    //    sobrante al redimensionar la ventana.
    //  * setStretchFactor(1, 0): el panel de metadatos mantiene su tamaño.
    //  * setSizes({700, 300}): reparto inicial, imagen dominante.
    //  * setMinimumWidth: evita que el panel desaparezca o se vuelva inútil.
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 0);
    ui->splitter->setSizes({700, 300});
    ui->treeMetadata->setMinimumWidth(220);

    m_pixmapItem = new QGraphicsPixmapItem();
    m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    m_scene->addItem(m_pixmapItem);

    connect(ui->graphicsView, &GraphicsViewZoom::zoomStepRequested,
            this, &MainWindow::applyZoomStep);

    // --- Timer del modo presentación ---
    m_slideshowTimer = new QTimer(this);
    m_slideshowTimer->setInterval(5000);
    connect(m_slideshowTimer, &QTimer::timeout, this, &MainWindow::onNextSlide);

    // --- Configuración de iconos de las QAction ---
    // Si el tema activo tiene el icono, se usa; si no, se recurre al
    // recurso SVG embebido. Así se ve nativo en Linux y sigue funcionando
    // en plataformas sin temas de iconos.
    auto loadIcon = [](const QString &themeName, const QString &fallbackPath) {
        QIcon icon = QIcon::fromTheme(themeName);
        if (icon.isNull()) {
            icon = QIcon(fallbackPath);
        }
        return icon;
    };

    ui->actionPreviousImage ->setIcon(loadIcon("go-previous",         ":/icons/prev.svg"));
    ui->actionNextImage     ->setIcon(loadIcon("go-next",             ":/icons/next.svg"));
    ui->actionZoomIn        ->setIcon(loadIcon("zoom-in",             ":/icons/zoom-in.svg"));
    ui->actionZoomOut       ->setIcon(loadIcon("zoom-out",            ":/icons/zoom-out.svg"));
    ui->actionZoomReset     ->setIcon(loadIcon("zoom-fit-best",       ":/icons/zoom-fit.svg"));
    ui->actionRotateLeft    ->setIcon(loadIcon("object-rotate-left",  ":/icons/rotate-left.svg"));
    ui->actionRotateRight   ->setIcon(loadIcon("object-rotate-right", ":/icons/rotate-right.svg"));
    ui->actionToggleMetadata->setIcon(loadIcon("dialog-information",  ":/icons/metadata.svg"));
    ui->actionFullscreen    ->setIcon(loadIcon("view-fullscreen",     ":/icons/fullscreen.svg"));
    ui->actionPresentation  ->setIcon(loadIcon("media-playback-start",":/icons/slideshow.svg"));

    // --- Construcción de la barra de herramientas ---
    // Se reutilizan las mismas QAction del menú: una sola fuente de verdad.
    // El orden replica el del menú Visualización, con el botón de metadatos
    // al final (para coincidir también con su posición en el menú).
    ui->toolBar->addAction(ui->actionPreviousImage);
    ui->toolBar->addAction(ui->actionNextImage);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionZoomOut);

    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setMinimumWidth(55);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    ui->toolBar->addWidget(m_zoomLabel);

    ui->toolBar->addAction(ui->actionZoomIn);
    ui->toolBar->addAction(ui->actionZoomReset);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionRotateLeft);
    ui->toolBar->addAction(ui->actionRotateRight);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionFullscreen);
    ui->toolBar->addAction(ui->actionPresentation);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionToggleMetadata);
}

void MainWindow::setupConnections()
{
    // --- Menú Archivo ---
    connect(ui->actionOpen,           &QAction::triggered, this, &MainWindow::onActionOpenTriggered);
    connect(ui->actionClose,          &QAction::triggered, this, &MainWindow::onActionCloseTriggered);
    connect(ui->actionQuit,           &QAction::triggered, this, &MainWindow::onActionQuitTriggered);
    connect(ui->actionPrintImage,     &QAction::triggered, this, &MainWindow::onActionPrintImageTriggered);
    connect(ui->actionPrintMetadata,  &QAction::triggered, this, &MainWindow::onActionPrintMetadataTriggered);
    connect(ui->actionExportMetadata, &QAction::triggered, this, &MainWindow::onActionExportMetadataTriggered);

    // --- Menú Visualización ---
    connect(ui->actionNextImage,      &QAction::triggered, this, &MainWindow::onNextImage);
    connect(ui->actionPreviousImage,  &QAction::triggered, this, &MainWindow::onPreviousImage);
    connect(ui->actionZoomIn,         &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(ui->actionZoomOut,        &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(ui->actionZoomReset,      &QAction::triggered, this, &MainWindow::onZoomReset);
    connect(ui->actionRotateRight,    &QAction::triggered, this, &MainWindow::onRotateClockwise);
    connect(ui->actionRotateLeft,     &QAction::triggered, this, &MainWindow::onRotateCounterClockwise);
    connect(ui->actionFullscreen,     &QAction::triggered, this, &MainWindow::onToggleFullscreen);
    connect(ui->actionPresentation,   &QAction::toggled,   this, &MainWindow::onTogglePresentationMode);
    connect(ui->actionToggleMetadata, &QAction::toggled,   this, &MainWindow::onToggleMetadataPanel);

    // --- Menú Ayuda ---
    connect(ui->actionShortcuts, &QAction::triggered, this, &MainWindow::onActionShortcutsTriggered);
    connect(ui->actionAbout,     &QAction::triggered, this, &MainWindow::onActionAboutTriggered);

    // --- Atajos sin entrada en menú ---

    // Escape sale de pantalla completa o presentación.
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this),
            &QShortcut::activated, this, &MainWindow::onEscapePressed);

    // Atajo reservado para uso interno. Se usa Ctrl+Shift+5 (en lugar de
    // Ctrl+Shift+0) para evitar pulsaciones accidentales junto a Ctrl+0.
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_5), this),
            &QShortcut::activated, this, &MainWindow::onReservedShortcut);

    // --- Atajos de zoom asignados por código ---
    // "Ctrl++" no se parsea bien desde el .ui; aquí se incluyen varias
    // variantes para máxima compatibilidad entre layouts de teclado.
    ui->actionZoomIn ->setShortcuts({QKeySequence::ZoomIn,  QKeySequence("Ctrl+=")});
    ui->actionZoomOut->setShortcuts({QKeySequence::ZoomOut, QKeySequence("Ctrl+-")});
}

// ============================================================================
// Carga de imagen
// ============================================================================

void MainWindow::loadImage(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    // --- 1. Cargar la imagen con OpenCV ---
    auto imageResult = m_imageHandler->loadAsQImage(filePath);
    if (!imageResult.has_value()) {
        QMessageBox::warning(this, tr("Error al cargar imagen"),
                             imageResult.error());
        updateStatusBar(tr("Error: %1").arg(imageResult.error()));
        return;
    }

    const QImage &image = imageResult.value();
    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());

    // Escala inicial: si la imagen cabe en la vista, mostrarla al 100%;
    // si es más grande, ajustarla a ventana.
    const QSize imageSize = m_pixmapItem->pixmap().size();
    const QSize viewSize  = ui->graphicsView->viewport()->size();
    if (imageSize.width() <= viewSize.width()
        && imageSize.height() <= viewSize.height()) {
        ui->graphicsView->resetTransform();
    } else {
        ui->graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    }

    // --- 2. Extraer metadatos con Exiv2 ---
    auto metadataResult = m_metadataExtractor->extract(filePath);
    ui->treeMetadata->clear();
    m_currentMetadata.clear();
    m_currentMetadataStatus = MetadataStatus::NoMetadata;

    if (!metadataResult.has_value()) {
        auto *errorItem = new QTreeWidgetItem(ui->treeMetadata);
        errorItem->setText(0, tr("Error"));
        errorItem->setText(1, metadataResult.error());
    } else {
        const MetadataResult &meta = metadataResult.value();
        m_currentMetadataStatus = meta.status;

        switch (meta.status) {
        case MetadataStatus::Ok:
            m_currentMetadata = meta.data;
            for (const auto &[category, entries] : m_currentMetadata) {
                auto *categoryItem = new QTreeWidgetItem(ui->treeMetadata);
                categoryItem->setText(0, QString::fromStdString(category));
                categoryItem->setExpanded(true);
                for (const auto &[key, value] : entries) {
                    auto *entryItem = new QTreeWidgetItem(categoryItem);
                    entryItem->setText(0, QString::fromStdString(key));
                    entryItem->setText(1, QString::fromStdString(value));
                }
            }
            ui->treeMetadata->resizeColumnToContents(0);
            break;

        case MetadataStatus::NoMetadata: {
            auto *infoItem = new QTreeWidgetItem(ui->treeMetadata);
            infoItem->setText(0, tr("Sin metadatos"));
            infoItem->setText(1,
                tr("Este formato admite metadatos, pero la imagen no contiene "
                   "ninguno EXIF, IPTC ni XMP."));
            break;
        }

        case MetadataStatus::UnsupportedFormat: {
            auto *infoItem = new QTreeWidgetItem(ui->treeMetadata);
            infoItem->setText(0, tr("Formato no soportado"));
            infoItem->setText(1,
                tr("El formato de imagen no soporta consulta de metadatos."));
            break;
        }
        }
    }

    // --- 3. Actualizar estado interno y navegación ---
    m_currentFilePath = filePath;
    rebuildDirectoryList();
    ui->actionExportMetadata->setEnabled(true);
    ui->actionPrintImage   ->setEnabled(true);
    ui->actionPrintMetadata->setEnabled(true);

    updateStatusBar(tr("Cargada: %1 (%2x%3)")
                        .arg(QFileInfo(filePath).fileName())
                        .arg(image.width())
                        .arg(image.height()));

    updateZoomLabel();
}

// ============================================================================
// Slots - Menú Archivo
// ============================================================================

void MainWindow::onActionOpenTriggered()
{
    const QString allFilter =
        tr("Todas las imágenes soportadas (%1)")
            .arg(ImageHandler::supportedExtensions().join(' '));

    const QStringList filterList = {
        allFilter,
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

    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Abrir imagen"), QString(), filterList.join(";;"));

    if (!filePath.isEmpty()) {
        loadImage(filePath);
    }
}

void MainWindow::onActionCloseTriggered()
{
    clearView();
    updateStatusBar(tr("Imagen cerrada."));
}

void MainWindow::onActionQuitTriggered()
{
    close();
}

void MainWindow::onActionPrintImageTriggered()
{
    // TODO(impresión): sustituir por QPrintDialog + QPrinter en la fase
    // intermedia. Requerirá añadir Qt6::PrintSupport al CMake.
    QMessageBox::information(
        this, tr("Imprimir imagen"),
        tr("Función no implementada todavía."));
}

void MainWindow::onActionPrintMetadataTriggered()
{
    // TODO(impresión): volcar formatAsText() a un QTextDocument y enviarlo
    // a QPrinter. Requerirá añadir Qt6::PrintSupport al CMake.
    QMessageBox::information(
        this, tr("Imprimir metadatos"),
        tr("Función no implementada todavía."));
}

void MainWindow::onActionExportMetadataTriggered()
{
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::information(this, tr("Exportar metadatos"),
            tr("No hay ninguna imagen cargada."));
        return;
    }

    QString suggestedName = QStringLiteral("metadata.txt");
    {
        const QFileInfo fi(m_currentFilePath);
        suggestedName = fi.completeBaseName() + QStringLiteral("_metadata.txt");
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exportar metadatos"), suggestedName,
        tr("Archivos de texto (*.txt);;Todos los archivos (*)"));

    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Error al exportar"),
            tr("No se pudo abrir el archivo: %1").arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << buildImageHeader();

    switch (m_currentMetadataStatus) {
    case MetadataStatus::Ok:
        stream << MetadataExtractor::formatAsText(m_currentMetadata);
        break;
    case MetadataStatus::NoMetadata:
        stream << "[Metadatos]\n"
               << "  Sin metadatos = La imagen no contiene metadatos EXIF, IPTC ni XMP.\n";
        break;
    case MetadataStatus::UnsupportedFormat:
        stream << "[Metadatos]\n"
               << "  Formato no soportado = El formato de imagen no soporta "
                  "consulta de metadatos.\n";
        break;
    }

    file.close();
    updateStatusBar(tr("Metadatos exportados a: %1").arg(path));
}

// ============================================================================
// Slots - Menú Visualización
// ============================================================================

void MainWindow::onNextImage()
{
    if (m_directoryImages.isEmpty()) return;
    m_currentIndex = (m_currentIndex + 1) % m_directoryImages.size();
    showImageAt(m_currentIndex);
}

void MainWindow::onPreviousImage()
{
    if (m_directoryImages.isEmpty()) return;
    m_currentIndex = (m_currentIndex - 1 + m_directoryImages.size()) % m_directoryImages.size();
    showImageAt(m_currentIndex);
}

void MainWindow::onZoomIn()
{
    applyZoomStep(kZoomStepButton);
}

void MainWindow::onZoomOut()
{
    applyZoomStep(-kZoomStepButton);
}

void MainWindow::onZoomReset()
{
    if (m_pixmapItem->pixmap().isNull()) return;
    ui->graphicsView->resetTransform();
    ui->graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    updateZoomLabel();
}

void MainWindow::onRotateClockwise()        { rotateCurrentImage(90); }
void MainWindow::onRotateCounterClockwise() { rotateCurrentImage(-90); }

void MainWindow::onToggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        menuBar()->setVisible(true);
        ui->toolBar->setVisible(true);
        statusBar()->setVisible(true);
        // Restaurar visibilidad del panel según el estado del action,
        // que es la fuente de verdad.
        ui->treeMetadata->setVisible(ui->actionToggleMetadata->isChecked());
    } else {
        menuBar()->setVisible(false);
        ui->toolBar->setVisible(false);
        statusBar()->setVisible(false);
        ui->treeMetadata->setVisible(false);
        showFullScreen();
    }
}

void MainWindow::onTogglePresentationMode(bool active)
{
    if (active) {
        if (m_directoryImages.isEmpty()) {
            QMessageBox::information(this, tr("Modo presentación"),
                tr("No hay imágenes en el directorio actual."));
            // Revertir el check sin re-disparar el slot.
            QSignalBlocker blocker(ui->actionPresentation);
            ui->actionPresentation->setChecked(false);
            return;
        }
        menuBar()->setVisible(false);
        ui->toolBar->setVisible(false);
        statusBar()->setVisible(false);
        ui->treeMetadata->setVisible(false);
        showFullScreen();
        m_slideshowTimer->start();
        updateStatusBar(tr("Modo presentación activo (5 s por imagen)."));
    } else {
        m_slideshowTimer->stop();
        showNormal();
        menuBar()->setVisible(true);
        ui->toolBar->setVisible(true);
        statusBar()->setVisible(true);
        ui->treeMetadata->setVisible(ui->actionToggleMetadata->isChecked());
        updateStatusBar(tr("Modo presentación detenido."));
    }
}

void MainWindow::onToggleMetadataPanel(bool visible)
{
    // El estado del panel es la fuente de verdad del QAction checkable:
    // menú, toolbar y atajo M comparten el mismo objeto y quedan siempre
    // sincronizados.
    ui->treeMetadata->setVisible(visible);
}

// ============================================================================
// Slots - Menú Ayuda
// ============================================================================

void MainWindow::onActionShortcutsTriggered()
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

    QMessageBox msg(this);
    msg.setWindowTitle(tr("Atajos de teclado"));
    msg.setTextFormat(Qt::RichText);
    msg.setText(html);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.exec();
}

void MainWindow::onActionAboutTriggered()
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

void MainWindow::onNextSlide()
{
    onNextImage();
}

void MainWindow::onEscapePressed()
{
    // Escape sale de presentación si está activa; si no, de fullscreen.
    if (ui->actionPresentation->isChecked()) {
        ui->actionPresentation->setChecked(false);  // dispara toggled(false)
    } else if (isFullScreen()) {
        onToggleFullscreen();
    }
}

void MainWindow::onReservedShortcut()
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

void MainWindow::applyZoomStep(int deltaPercent)
{
    if (m_pixmapItem->pixmap().isNull()) {
        return;
    }

    const double currentPercent = ui->graphicsView->transform().m11() * 100.0;
    const double targetPercent  = currentPercent + deltaPercent;

    double clampedPercent = targetPercent;
    if (clampedPercent < kZoomMinPercent) clampedPercent = kZoomMinPercent;
    if (clampedPercent > kZoomMaxPercent) clampedPercent = kZoomMaxPercent;

    if (qFuzzyCompare(clampedPercent, currentPercent)) {
        return;
    }

    const double factor = clampedPercent / currentPercent;
    ui->graphicsView->scale(factor, factor);
    updateZoomLabel();
}

// ============================================================================
// Auxiliares
// ============================================================================

void MainWindow::updateStatusBar(const QString &message)
{
    // Sin timeout: el mensaje permanece hasta que se sustituya por otro.
    statusBar()->showMessage(message);
}

void MainWindow::clearView()
{
    m_currentMetadataStatus = MetadataStatus::NoMetadata;
    m_currentFilePath.clear();
    m_currentMetadata.clear();
    m_pixmapItem->setPixmap(QPixmap());
    m_scene->setSceneRect(QRectF());
    ui->treeMetadata->clear();
    ui->actionExportMetadata->setEnabled(false);
    ui->actionPrintImage   ->setEnabled(false);
    ui->actionPrintMetadata->setEnabled(false);
}

void MainWindow::rebuildDirectoryList()
{
    m_directoryImages.clear();
    if (m_currentFilePath.isEmpty()) return;

    const QFileInfo fi(m_currentFilePath);
    QDir dir = fi.absoluteDir();

    const QFileInfoList entries = dir.entryInfoList(
        ImageHandler::supportedExtensions(), QDir::Files, QDir::Name);

    for (const auto &entry : entries) {
        m_directoryImages << entry.absoluteFilePath();
    }
    m_currentIndex = m_directoryImages.indexOf(m_currentFilePath);
}

void MainWindow::showImageAt(int index)
{
    if (index < 0 || index >= m_directoryImages.size()) return;
    loadImage(m_directoryImages.at(index));
}

void MainWindow::rotateCurrentImage(int degrees)
{
    if (m_pixmapItem->pixmap().isNull()) return;

    QTransform transform;
    transform.rotate(degrees);
    const QPixmap rotated = m_pixmapItem->pixmap().transformed(transform);
    m_pixmapItem->setPixmap(rotated);
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    ui->graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    updateZoomLabel();
}

void MainWindow::updateZoomLabel()
{
    if (!m_zoomLabel) return;
    const double scale = ui->graphicsView->transform().m11();
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(scale * 100.0)));
}

QString MainWindow::buildImageHeader() const
{
    const QFileInfo fi(m_currentFilePath);
    QString header;
    QTextStream stream(&header);
    stream << "=== DATOS DE LA IMAGEN ===\n"
           << "Nombre:  " << fi.fileName()         << '\n'
           << "Formato: " << fi.suffix().toUpper() << '\n'
           << "Ruta:    " << fi.absoluteFilePath() << "\n\n";
    return header;
}
