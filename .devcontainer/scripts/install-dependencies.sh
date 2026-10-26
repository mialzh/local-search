#!/usr/bin/env bash
set -euo pipefail

dependency_root=/opt/local-search-deps
build_jobs=${BUILD_JOBS:-2}
mkdir -p "${dependency_root}/src"

fetch_commit() {
    local repository_url=$1
    local commit_sha=$2
    local destination=$3
    git init "${destination}"
    git -C "${destination}" remote add origin "${repository_url}"
    git -C "${destination}" fetch --depth 1 origin "${commit_sha}"
    git -C "${destination}" checkout --detach FETCH_HEAD
}

fetch_commit https://github.com/big-keva/libmorph.git \
    90a5a1a95d0edf693855f6b66759c8b66821865f "${dependency_root}/src/libmorph"
git -C "${dependency_root}/src/libmorph" submodule update --init --recursive --depth 1
fetch_commit https://github.com/big-keva/moonycode.git \
    639553712ab068315ba9726d56989956962048b3 "${dependency_root}/src/moonycode"

cmake -S /opt/local-search-dev/dependency-build \
    -B "${dependency_root}/build/morphology" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DLIBMORPH_SOURCE_DIR="${dependency_root}/src/libmorph" \
    -DMOONYCODE_SOURCE_DIR="${dependency_root}/src/moonycode"
cmake --build "${dependency_root}/build/morphology" \
    --target morphrus-static morpheng-static moonycode-static --parallel "${build_jobs}"
cmake --install "${dependency_root}/build/morphology" --component LocalSearchMorphology

# Catch2 3.8.1. The Ubuntu 22.04 package has a different major version.
fetch_commit https://github.com/catchorg/Catch2.git \
    2b60af89e23d28eefc081bc930831ee9d45ea58b "${dependency_root}/src/Catch2"
cmake -S "${dependency_root}/src/Catch2" \
    -B "${dependency_root}/build/catch2" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCATCH_BUILD_TESTING=OFF -DCATCH_INSTALL_DOCS=OFF -DCATCH_INSTALL_EXTRAS=ON
cmake --build "${dependency_root}/build/catch2" --parallel "${build_jobs}"
cmake --install "${dependency_root}/build/catch2"

ldconfig
