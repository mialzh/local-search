# Native Windows / MinGW. Run from the repository root.
SHELL := cmd.exe
.SHELLFLAGS := /D /C

VCPKG_ROOT ?= C:/dev/vcpkg
BUILD_DIR ?= build-mingw
TRIPLET ?= x64-mingw-dynamic

.PHONY: test configure build

test: build
	set "PATH=$(subst /,\,$(VCPKG_ROOT)/installed/$(TRIPLET)/bin);%PATH%" && ctest --test-dir "$(BUILD_DIR)" --output-on-failure

build: configure
	cmake --build "$(BUILD_DIR)" --parallel

configure:
	cmake -S . -B "$(BUILD_DIR)" -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_TOOLCHAIN_FILE="$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=$(TRIPLET) -DVCPKG_HOST_TRIPLET=$(TRIPLET) -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
