# Sólo otro simple visor de metadatos

Un visor de imágenes ligero y rápido para Linux, con extracción de metadatos EXIF, IPTC y XMP. Construido con C++23, Qt 6, OpenCV y Exiv2.

![Captura de pantalla](docs/screenshot.png)

## Versión

**0.4.0**

## Características

- Apertura de todos los formatos de imagen que soporta OpenCV (JPEG, PNG, TIFF, WebP, BMP, GIF, etc.).
- Visualización de metadatos EXIF, IPTC y XMP en un panel lateral.
- Zoom (rueda del ratón y botones), paneo y rotación de 90°.
- Navegación entre imágenes del mismo directorio.
- Modo de pantalla completa y modo presentación (5 segundos por imagen).
- Exportación de metadatos a archivo de texto.
- Interfaz oscura, atajos de teclado y soporte multiidioma (en preparación).

## Tecnologías

- **Lenguaje:** C++23
- **Interfaz gráfica:** Qt 6 (Widgets)
- **Procesamiento de imágenes:** OpenCV 4.x
- **Metadatos:** Exiv2 0.28+
- **Sistema de construcción:** CMake 3.20+

## Dependencias (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev libopencv-dev libexiv2-dev libxkbcommon-dev
