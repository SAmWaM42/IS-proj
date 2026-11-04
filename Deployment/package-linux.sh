#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-linux-release"
STAGE_DIR="${ROOT_DIR}/dist/animal-agent-linux"
TARGET_NAME="IS-2"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || true)}"

usage() {
    cat <<'EOF'
Usage: ./Deployment/package-linux.sh [build|package|stage|clean]

build    Configure and build the Linux Release target.
package  Build the Linux Release target and stage a self-contained runtime dir.
stage    Recreate the runtime staging directory without rebuilding.
clean   Remove generated build and staging directories.
EOF
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" >&2
        exit 1
    fi
}

prepare_environment() {
    require_command cmake
    require_command g++
    require_command python3
    require_command pkg-config

    if ! pkg-config --exists sfml-graphics; then
        echo "SFML development libraries are not available on this Linux system." >&2
        echo "Install the SFML development package for your distro before packaging." >&2
        exit 1
    fi
}

build_target() {
    mkdir -p "${BUILD_DIR}"
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    cmake --build "${BUILD_DIR}" --target "${TARGET_NAME}" --parallel "$(nproc)"
}

copy_runtime_libs() {
    local binary="${1}"
    mkdir -p "${STAGE_DIR}/bin"

    while IFS= read -r dep; do
        if [[ -z "${dep}" ]]; then
            continue
        fi

        case "${dep}" in
            */ld-linux*|*/libc.so*|*/libm.so*|*/libstdc++.so*|*/libgcc_s.so*|*/libpthread.so*|*/libdl.so*|*/librt.so*)
                continue
                ;;
        esac

        cp -L "${dep}" "${STAGE_DIR}/bin/" 2>/dev/null || true
    done < <(ldd "${binary}" | awk '/=>/ {print $3} END {print ""}')
}

bundle_python_runtime() {
    if [[ -z "${PYTHON_BIN}" || ! -x "${PYTHON_BIN}" ]]; then
        echo "Python 3 was not found. Cannot bundle a runtime interpreter." >&2
        exit 1
    fi

    local python_prefix
    python_prefix="$(dirname "$(dirname "${PYTHON_BIN}")")"
    local python_version
    python_version="$(${PYTHON_BIN} -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"

    mkdir -p "${STAGE_DIR}/runtime/python/bin" "${STAGE_DIR}/runtime/python/lib"
    cp "${PYTHON_BIN}" "${STAGE_DIR}/runtime/python/bin/python3"

    local python_stdlib_dir="${python_prefix}/lib/python${python_version}"
    if [[ -d "${python_stdlib_dir}" ]]; then
        cp -a "${python_stdlib_dir}" "${STAGE_DIR}/runtime/python/lib/"
    fi

    local python_site_packages_dir="${python_prefix}/lib/python${python_version}/site-packages"
    if [[ -d "${python_site_packages_dir}" ]]; then
        mkdir -p "${STAGE_DIR}/runtime/python/lib/python${python_version}"
        cp -a "${python_site_packages_dir}" "${STAGE_DIR}/runtime/python/lib/python${python_version}/"
    fi
}

stage_runtime_files() {
    mkdir -p "${STAGE_DIR}/Components" "${STAGE_DIR}/Data" "${STAGE_DIR}/wordTokenizer"

    cp -a "${ROOT_DIR}/Components/NLP" "${STAGE_DIR}/Components/"
    cp -a "${ROOT_DIR}/Data" "${STAGE_DIR}/"
    cp -a "${ROOT_DIR}/wordTokenizer" "${STAGE_DIR}/"

    cp "${BUILD_DIR}/${TARGET_NAME}" "${STAGE_DIR}/${TARGET_NAME}"
    chmod +x "${STAGE_DIR}/${TARGET_NAME}"

    copy_runtime_libs "${STAGE_DIR}/${TARGET_NAME}"
    bundle_python_runtime

    cat > "${STAGE_DIR}/launch.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/bin${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export PATH="${SCRIPT_DIR}/runtime/python/bin:${PATH}"
cd "${SCRIPT_DIR}"
exec "${SCRIPT_DIR}/IS-2" "$@"
EOF
    chmod +x "${STAGE_DIR}/launch.sh"

    cat > "${STAGE_DIR}/README.txt" <<'EOF'
Animal Agent - Linux participant bundle

This directory contains the packaged build output for this project.

Run:
  ./launch.sh

The bundled launcher adds runtime library and Python paths relative to this
package so the application can launch without requiring a developer install.
EOF
}

clean_generated_files() {
    rm -rf "${BUILD_DIR}" "${STAGE_DIR}"
}

main() {
    local action="${1:-package}"

    case "${action}" in
        build)
            prepare_environment
            build_target
            ;;
        stage)
            prepare_environment
            if [[ ! -x "${BUILD_DIR}/${TARGET_NAME}" ]]; then
                build_target
            fi
            stage_runtime_files
            ;;
        package)
            prepare_environment
            build_target
            stage_runtime_files
            ;;
        clean)
            clean_generated_files
            ;;
        -h|--help|help)
            usage
            ;;
        *)
            echo "Unknown action: ${action}" >&2
            usage >&2
            exit 1
            ;;
    esac
}

main "$@"
