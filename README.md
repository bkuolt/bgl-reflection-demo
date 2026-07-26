# BGL Reflection Demo (Modernized C++23)

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu%20Linux%20(amd64)-orange.svg)](https://ubuntu.com/)
[![Build System](https://img.shields.io/badge/Build-CMake%20%7C%20Conan%202.x-green.svg)](https://conan.io/)

A modernized **C++23** 3D graphics tech demo demonstrating **real-time planar reflections using the OpenGL stencil buffer** and animated Quake II **MD2 character models**.

> **Note on Legacy Codebase Preservation**  
> This project is a modernized migration of a legacy Win32/OpenGL tech demo from the early 2000s. While the legacy fixed-function OpenGL rendering pipeline and Quake II MD2 binary file parser have been preserved for historical fidelity, the underlying infrastructure, memory management (RAII), mathematics (`glm`), structured logging (`spdlog`), window management (`FreeGLUT`), and build system (`Conan 2.x`, `CMake`) have been upgraded to modern C++23 standards.

---

## Technical Highlights & Modernization Stack

* **Language Standard**: Modern **C++23** with standard library abstractions (`std::filesystem`, `std::format`, `std::unique_ptr`).
* **Mathematics**: **GLM** (`glm::vec3`, `glm::vec2`, `glm::mix`, `glm::value_ptr`) replacing custom legacy array mathematics.
* **Logging**: **spdlog** for high-performance structured terminal logging (`spdlog::info`, `spdlog::warn`, `spdlog::error`).
* **Dependency Management**: **Conan 2.x** (`conanfile.py`) managing cross-platform dependencies.
* **Windowing & Input**: **FreeGLUT** cross-platform window initialization with automatic Wayland/XWayland display fallback.
* **Image Loading**: **DevIL** image library (`IL/il.h`) integrated with OpenGL texture binding and mipmapping (`gluBuild2DMipmaps`).
* **Task Automation**: **Taskfile** (`Taskfile.yml`) automating dependency installation, configuration, compilation, and execution.

---

## Project Structure

```
bgl-reflection-demo/
├── bin/                   # Binary asset files (Ogros.md2, textures)
├── src/
│   ├── main.cpp           # OpenGL display loop, camera controls, stencil reflection logic
│   ├── MD2.hpp            # MD2 model header declaration
│   ├── MD2.cpp            # MD2 binary parser, vertex interpolation (GLM), animation logic
│   └── FileHeader.hpp     # Binary data layout structures for MD2 format
├── CMakeLists.txt         # Modern CMake target definitions and asset copy scripts
├── conanfile.py           # Conan 2.x dependency package recipe
├── Taskfile.yml           # Task build runner configuration
└── README.md              # Project documentation
```

---

## Requirements & Prerequisites

* **Operating System**: Ubuntu 22.04 LTS / 24.04 LTS (amd64) or compatible Linux distribution.
* **C++ Compiler**: GCC 13+ or Clang 16+ with C++23 support.
* **Build Tools**:
  * `cmake` (v3.20+)
  * `conan` (v2.x)
  * `task` (Taskfile runner: `sudo snap install task --classic` or `apt install taskwarrior`)
* **Graphics Dependencies**: OpenGL development headers (`libgl-dev`, `libglu1-mesa-dev`).

---

## Building and Running

### 1. Quick Start using Taskfile

You can manage the complete lifecycle using `task`:

```bash
# 1. Install dependencies via Conan 2.x
task conan-install

# 2. Configure CMake with Conan toolchain preset
task configure

# 3. Build executable
task build

# 4. Launch the application
task run
```

### 2. Manual CMake Instructions

```bash
# Install dependencies with Conan 2.x
conan install . --build=missing -s build_type=Debug

# Configure CMake
cmake --preset conan-debug

# Build project
cmake --build --preset conan-debug

# Run executable
./build/Debug/bgl-reflection-demo
```

---

## Controls & Usage

* **ESC / Q**: Exit the application.
* **Camera**: Dynamic revolving orbital camera focusing on the animated character model and stencil floor reflection.

---

## License & Copyright

Original demo code copyright (c) Bastian Kuolt. Modernized and ported to Linux C++23.
