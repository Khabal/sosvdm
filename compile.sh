#!/usr/bin/env bash
# ============================================================================
# compile.sh - Compilación y ejecución de sosvdm
# ============================================================================
# Uso:
#   ./compile.sh             # Compilación incremental y ejecución
#   ./compile.sh clean       # Limpieza total, compilación y ejecución
#   ./compile.sh build       # Solo compilación, sin ejecutar
#   ./compile.sh clean build # Limpieza total, compilación, sin ejecutar
#
# Variables de entorno:
#   BUILD_TYPE=Release ./compile.sh   # Cambiar tipo de build (por defecto Debug)
# ============================================================================

set -euo pipefail   # -e: salir en error; -u: error en var no definida;
                    # -o pipefail: propagar errores en pipes.

# --- Configuración ---
# Resolvemos la ruta del proyecto a partir de la ubicación del script,
# así funciona independientemente de dónde se ejecute.
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

# --- Parseo de argumentos ---
CLEAN_BUILD=false
RUN_AFTER_BUILD=true

for arg in "$@"; do
    case "${arg}" in
        clean)  CLEAN_BUILD=true ;;
        build)  RUN_AFTER_BUILD=false ;;
        -h|--help)
            sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Opción no reconocida: ${arg}" >&2
            exit 1
            ;;
    esac
done

# --- 1. Limpieza (opcional) ---
if [[ "${CLEAN_BUILD}" == true ]]; then
    echo ">>> Limpiando ${BUILD_DIR}..."
    rm -rf "${BUILD_DIR}"
fi

# --- 2. Configuración ---
echo ">>> Configurando (${BUILD_TYPE})..."
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# --- 3. Compilación ---
echo ">>> Compilando..."
cmake --build "${BUILD_DIR}" --parallel

# --- 4. Ejecución (opcional) ---
if [[ "${RUN_AFTER_BUILD}" == true ]]; then
    echo ">>> Ejecutando..."
    exec "${BUILD_DIR}/sosvdm"
else
    echo ">>> Compilación completada. Binario en ${BUILD_DIR}/sosvdm"
fi
