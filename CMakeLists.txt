# ============================================================================
# CMakeLists.txt para "Sólo otro simple visor de metadatos" (C++23 + Qt6 + OpenCV + Exiv2)
# ============================================================================

# 1. Requisitos de versión y configuración del proyecto
# ----------------------------------------------------------------------------
# Necesitamos CMake 3.20+ para el estándar C++23 de forma nativa.
cmake_minimum_required(VERSION 3.20)

# El proyecto se llama "Sólo otro simple visor de metadatos" (sosvdm) y está escrito en C++.
project(sosvdm VERSION 0.4.0 LANGUAGES CXX)

# 2. Configuración del estándar C++ y del comportamiento del compilador
# ----------------------------------------------------------------------------
# Establecer C++23 como el estándar de C++ para todo el proyecto.
set(CMAKE_CXX_STANDARD 23)

# Obligar a que el compilador sea C++23; si no, error en la configuración.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Desactivar extensiones específicas del compilador para máxima portabilidad.
set(CMAKE_CXX_EXTENSIONS OFF)

# Comprobamos que el compilador soporta std::expected.
# Si no, fallamos con un mensaje claro.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <expected>
    int main() { std::expected<int, int> e{42}; (void)e; return 0; }
" HAS_STD_EXPECTED)

if(NOT HAS_STD_EXPECTED)
    message(FATAL_ERROR
        "El compilador no soporta std::expected (requiere GCC 12+, Clang 16+ "
        "o MSVC 19.33+). Actualiza tu toolchain para usar C++23.")
endif()

# 3. Configuración de Qt (AUTOMOC, AUTOUIC, AUTORCC)
# ----------------------------------------------------------------------------
# Estas variables le dicen a CMake que ejecute automáticamente las herramientas
# de Qt (moc, uic, rcc) sobre los archivos correspondientes.
# CMAKE_AUTOMOC: Procesa los archivos .cpp que contienen la macro Q_OBJECT.
# CMAKE_AUTOUIC: Convierte los archivos .ui (diseños de Qt Designer) en código C++.
# CMAKE_AUTORCC: Compila los archivos .qrc (recursos) en código C++.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 4. Búsqueda de dependencias
# ----------------------------------------------------------------------------

# --- Qt6 ---
# Buscamos Qt6 y los módulos que necesitaremos inicialmente.
# Widgets: Para la ventana principal y controles básicos.
# Gui: Para la integración de la ventana con el sistema operativo.
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core LinguistTools)

# --- OpenCV ---
# OpenCV proporciona su propio archivo de configuración CMake.
# La opción CONFIG le dice a CMake que busque el archivo OpenCVConfig.cmake,
# que es más moderno y fiable que el antiguo FindOpenCV.cmake.
find_package(OpenCV REQUIRED CONFIG)

# --- Exiv2 ---
# Exiv2 también proporciona un archivo de configuración CMake.
# Usamos el nombre 'exiv2' (en minúsculas) y pedimos explícitamente la
# configuración. A veces, el nombre del paquete es 'Exiv2' o 'exiv2',
# así que es posible que necesites ajustar esto si tu instalación difiere.
find_package(exiv2 REQUIRED CONFIG)

# 5. Definición del ejecutable y sus archivos fuente
# ----------------------------------------------------------------------------
# Lista de todos los archivos que forman parte del ejecutable.
# Los archivos .ui y .qrc también se incluyen aquí; las herramientas de Qt
# los procesarán automáticamente gracias a las variables AUTOMOC/UIC/RCC.
set(PROJECT_SOURCES
    src/main.cpp
    src/ventanaprincipal.cpp
    src/ventanaprincipal.h
    src/ventanaprincipal.ui
    src/manejadorimagen.cpp
    src/manejadorimagen.h
    src/extractormetadatos.cpp
    src/extractormetadatos.h
    resources/resources.qrc
    src/vistagraficanivelampliacion.cpp
    src/vistagraficanivelampliacion.h
)

# Creamos el objetivo ejecutable.
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 6. Vinculación de bibliotecas y configuración de dependencias
# ----------------------------------------------------------------------------

# Los headers propios se incluyen con comillas ("...") desde el código y
# desde los archivos generados por uic. Añadimos src/ a las rutas de
# búsqueda para que ese #include se resuelva en cualquier unidad de
# traducción, incluidos los archivos generados en build/.
target_include_directories(${PROJECT_NAME} PRIVATE src)

# ----------------------------------------------------------------------------
# Traducciones (i18n)
# ----------------------------------------------------------------------------
# qt_add_translations hace tres cosas:
#   1. Crea reglas para que lupdate regenere los .ts cuando cambien fuentes.
#   2. Ejecuta lrelease para producir los .qm en cada build.
#   3. Embebe los .qm en los recursos del binario bajo el prefijo indicado.
#
# Los .ts son ficheros XML que se versionan en el repositorio. Los .qm son
# binarios generados y NO se versionan (van al .gitignore).
#
# Si el idioma del sistema no tiene .qm asociado, la aplicación funciona
# igual, mostrando las cadenas originales tal cual están en el código.
qt_add_translations(${PROJECT_NAME}
    TS_FILES
        i18n/sosvdm_es.ts
        i18n/sosvdm_en.ts
    RESOURCE_PREFIX "/i18n"
)

# --- Qt6 ---
# Vinculamos los módulos de Qt6 al ejecutable.
# El uso de los nombres de destino (Qt6::Widgets, Qt6::Gui) es la forma moderna
# y recomendada. Propaga automáticamente las rutas de inclusión y los flags
# de compilación necesarios.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Gui
)

# --- OpenCV ---
# Las variables OpenCV_LIBS y OpenCV_INCLUDE_DIRS son establecidas por
# find_package(OpenCV). Vinculamos todas las bibliotecas de OpenCV.
# Aunque podríamos usar solo opencv_core y opencv_imgcodecs, vincular
# OpenCV_LIBS es más sencillo y garantiza que no falte nada.
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

# --- Exiv2 ---
# Exiv2, al usar CONFIG, define un destino importado llamado 'exiv2lib'.
# Este es el nombre correcto a utilizar en target_link_libraries.
# La vinculación a este destino también añade automáticamente el directorio
# de inclusión necesario, por lo que no hace falta usar target_include_directories.
target_link_libraries(${PROJECT_NAME} PRIVATE exiv2lib)

# 7. Configuración específica para plataformas (Opcional, pero recomendado)
# ----------------------------------------------------------------------------
# En Windows, es común que una aplicación GUI no abra una consola de comandos.
# La propiedad WIN32 en add_executable o la variable CMAKE_WIN32_EXECUTABLE
# manejan esto. CMake 3.21+ permite usar add_executable con la opción WIN32.
# Por ahora, y dado que la Fase 1 es solo Linux, no es necesario.
# if(WIN32)
#     set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
# endif()

# 8. Mensajes de estado para verificar la configuración
# ----------------------------------------------------------------------------
message(STATUS "Versión de Qt6 encontrada: ${Qt6_VERSION}")
message(STATUS "Versión de OpenCV encontrada: ${OpenCV_VERSION}")
# Exiv2 no establece una variable de versión estándar de forma consistente
# en todas las versiones, así que no la mostramos para evitar errores.
