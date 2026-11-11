#!/usr/bin/env bash
set -e

export PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUT_DIR="${SCRIPT_DIR}/out/arm64-v8a"

echo "=========================================================="
echo "  Building Arke-Android (arm64-v8a) - libarke_android.so  "
echo "=========================================================="

mkdir -p "${BUILD_DIR}"
mkdir -p "${OUT_DIR}"

if [ -n "${ANDROID_NDK_ROOT}" ] || [ -n "${ANDROID_NDK_HOME}" ]; then
    NDK="${ANDROID_NDK_ROOT:-$ANDROID_NDK_HOME}"
    echo "[+] Using Android NDK: ${NDK}"
    cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-26 \
        -DCMAKE_BUILD_TYPE=Release
else
    echo "[+] No ANDROID_NDK set; building natively with local aarch64 clang..."
    cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" \
        -DCMAKE_BUILD_TYPE=Release
fi

cmake --build "${BUILD_DIR}" --config Release -j$(nproc 2>/dev/null || echo 4)

# Copy output artifact
if [ -f "${BUILD_DIR}/libarke_android.so" ]; then
    cp "${BUILD_DIR}/libarke_android.so" "${OUT_DIR}/"
    echo "[✓] Successfully built: ${OUT_DIR}/libarke_android.so"
fi

if [ -f "${BUILD_DIR}/test_shader_transpile" ]; then
    cp "${BUILD_DIR}/test_shader_transpile" "${OUT_DIR}/"
    echo "[✓] Running local unit & transpile tests..."
    "${BUILD_DIR}/test_shader_transpile"
fi
