#!/usr/bin/env bash
set -euo pipefail
check_build_dir=$(mktemp -d /tmp/local-search-check.XXXXXX)
trap 'rm -rf "${check_build_dir}"' EXIT
cmake -S /opt/local-search-dev/check -B "${check_build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "${check_build_dir}" --parallel 2
"${check_build_dir}/morphology_check"
