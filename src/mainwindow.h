// ============================================================================
// mainwindow.h - Ventana principal de "Sólo otro simple visor de metadatos"
// ============================================================================
#pragma once

#include <QMainWindow>
#include <QString>
#include <QApplication>
#include <QDir>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QTimer>
#include <QLabel>
#include <QAction>

#include <memory>

#include "metadataextractor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ImageHandler;

/**
 * @brief Ventana principal de la aplicación.
 *
 * Responsabilidades:
 *   - Mostrar la imagen seleccionada y sus metadatos.
 *   - Gestionar la navegación, zoom, rotación y modos de visualización.
 *   - Servir de punto de integración entre ImageHandler y MetadataExtractor.
 *
 * Nota de diseño: la lógica de carga de imágenes y extracción de metadatos
 * vive en sus propias clases. MainWindow actúa como orquestador y capa de
 * presentación.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief Carga una imagen desde disco y actualiza la UI.
     * @param filePath Ruta al archivo de imagen.
     */
    void loadImage(const QString &filePath);

private slots:
    // --- Menú Archivo ---
    void onActionOpenTriggered();
    void onActionCloseTriggered();
    void onActionQuitTriggered();
    void onActionPrintImageTriggered();
    void onActionPrintMetadataTriggered();
    void onActionExportMetadataTriggered();

    // --- Menú Visualización ---
    void onNextImage();
    void onPreviousImage();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onRotateClockwise();
    void onRotateCounterClockwise();
    void onToggleFullscreen();
    void onTogglePresentationMode(bool active);
    void onToggleMetadataPanel(bool visible);

    // --- Menú Ayuda ---
    void onActionShortcutsTriggered();
    void onActionAboutTriggered();

    // --- Internos ---
    void onNextSlide();
    void onEscapePressed();
    void onReservedShortcut();
    void applyZoomStep(int deltaPercent);

private:
    // --- Métodos auxiliares ---
    void setupUi();
    void setupConnections();
    void updateStatusBar(const QString &message);
    void clearView();
    void rebuildDirectoryList();
    void showImageAt(int index);
    void rotateCurrentImage(int degrees);
    void updateZoomLabel();
    [[nodiscard]] QString buildImageHeader() const;

    // --- Miembros ---
    std::unique_ptr<Ui::MainWindow> ui;

    QGraphicsScene      *m_scene          = nullptr;
    QGraphicsPixmapItem *m_pixmapItem     = nullptr;
    QTimer              *m_slideshowTimer = nullptr;

    std::unique_ptr<ImageHandler>      m_imageHandler;
    std::unique_ptr<MetadataExtractor> m_metadataExtractor;

    MetadataExtractor::MetadataMap m_currentMetadata;
    MetadataStatus                 m_currentMetadataStatus = MetadataStatus::NoMetadata;

    QString     m_currentFilePath;
    QStringList m_directoryImages;
    int         m_currentIndex = -1;

    QLabel *m_zoomLabel = nullptr;

    // Límites de zoom, en porcentaje.
    static constexpr double kZoomMinPercent = 2.0;
    static constexpr double kZoomMaxPercent = 2000.0;

    // Pasos de zoom, en puntos porcentuales.
    static constexpr int kZoomStepWheel  = 2;
    static constexpr int kZoomStepButton = 5;
};
