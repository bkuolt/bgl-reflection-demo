# BGL Reflection Demo (Modernized C++23)

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu%20Linux%20(amd64)-orange.svg)](https://ubuntu.com/)
[![Build System](https://img.shields.io/badge/Build-CMake%20%7C%20Conan%202.x-green.svg)](https://conan.io/)
[![OpenGL](https://img.shields.io/badge/Graphics-OpenGL%20Legacy%202.1-red.svg)](https://www.khronos.org/opengl/)

A modernized **C++23** 3D graphics tech demo demonstrating **real-time planar reflections using the OpenGL stencil buffer** and animated Quake II **MD2 character models**.

![BGL Reflection Demo Screenshot](bin/screenshot.png)

---

> [!IMPORTANT]  
> **Legacy OpenGL Codebase Notice & Modernization Roadmap**  
> This application currently uses fixed-function **OpenGL 2.1 (Immediate Mode)** pipeline rendering (`glBegin`/`glEnd`, `glPushMatrix`/`glPopMatrix`, `glFrustum`, `gluLookAt`, display lists).  
> **Future Porting Goal**: The core architecture is scheduled for a full migration to **OpenGL 4.6 Core Profile** (Shaders `GLSL 460`, Vertex Array Objects `VAO`, Vertex Buffer Objects `VBO`, Uniform Buffer Objects `UBO`, and Framebuffer Objects `FBO` for offscreen reflections).

---

## Technical Highlights & Modernization Stack

* **Language Standard**: Modern **C++23** (`std::span`, `std::string_view`, `std::filesystem`, `std::format`, `std::unique_ptr`).
* **Mathematics**: **GLM** (`glm::vec3`, `glm::vec2`, `glm::mix`, `glm::value_ptr`) replacing custom legacy array mathematics.
* **Logging**: **spdlog** for high-performance structured terminal logging (`spdlog::info`, `spdlog::warn`, `spdlog::error`).
* **Dependency Management**: **Conan 2.x** (`conanfile.py`) with reproducible **lockfiles** (`conan.lock`).
* **Windowing & Input**: **FreeGLUT** cross-platform window initialization with automatic XWayland/X11 display routing on Wayland desktops.
* **Image Loading**: **DevIL** image library (`IL/il.h`) integrated with OpenGL texture binding and mipmapping.
* **Task Automation**: **Taskfile** (`Taskfile.yml`) automating lockfile creation, dependency installation, build configuration, compilation, and execution.

---

## Project Structure

```
bgl-reflection-demo/
├── bin/                   # Binary asset files (Ogros.md2, glass.jpg, igdosh.jpeg, screenshot.png)
├── src/
│   ├── main.cpp           # FreeGLUT event loop, camera logic, SceneResources RAII class, stencil reflection
│   ├── MD2.hpp            # bgl::MD2 model class declaration
│   ├── MD2.cpp            # MD2 binary file parser, RAII vertex interpolation, frame animation
│   └── FileHeader.hpp     # Fixed-width binary data layout structures for MD2 format (pack push/pop)
├── conanfile.py           # Conan 2.x package recipe
├── conan.lock             # Lockfile ensuring reproducible dependency resolution
├── CMakeLists.txt         # Modern CMake C++23 build configuration and asset copying scripts
├── Taskfile.yml           # Automated task runner recipes
└── README.md              # Comprehensive project documentation
```

---

## Requirements & Prerequisites

* **Operating System**: Ubuntu 22.04 LTS / 24.04 LTS (amd64) or compatible Linux distribution.
* **C++ Compiler**: GCC 13+ or Clang 16+ supporting C++23.
* **Build Tools**:
  * `cmake` (v3.20+)
  * `conan` (v2.x)
  * `task` (Taskfile runner: `sudo snap install task --classic`)
* **Graphics Dependencies**: OpenGL development headers (`libgl-dev`, `libglu1-mesa-dev`).

---

## Building and Running

### 1. Quick Start using Taskfile

Manage the complete lifecycle with reproducible lockfiles:

```bash
# 1. Generate / verify Conan 2.x lockfile
task lock

# 2. Install locked dependencies via Conan 2.x
task conan-install

# 3. Configure CMake with Conan debug preset
task configure

# 4. Build executable
task build

# 5. Launch the application (Full HD default resolution)
task run
```

### 2. Manual Commands

```bash
# Generate lockfile
conan lock create conanfile.py --build=missing

# Install locked dependencies
conan install . --lockfile=conan.lock --build=missing -s build_type=Debug

# Configure CMake preset
cmake --preset conan-debug

# Build executable
cmake --build --preset conan-debug

# Launch binary
./build/Debug/bgl-reflection-demo
```

---

## OpenGL 4.6 Core Profile Migration Roadmap

The following roadmap outlines the steps to upgrade this project from legacy fixed-function OpenGL 2.1 to modern OpenGL 4.6:

- [ ] **Windowing & Context**: Replace FreeGLUT with **GLFW** or **SDL2** requesting an explicit OpenGL 4.6 Core Profile context.
- [ ] **Shading Language**: Implement GLSL 460 vertex and fragment shaders for geometry rendering, texturing, and lighting.
- [ ] **Geometry Storage**: Replace `glBegin()`/`glEnd()` and display lists with Vertex Array Objects (**VAO**) and Vertex Buffer Objects (**VBO**).
- [ ] **Matrix Transformation**: Pass Model-View-Projection (`MVP`) matrices via Uniform Buffer Objects (**UBO**).
- [ ] **Real-time Reflection**: Replace the stencil buffer reflection with a Framebuffer Object (**FBO**) texture pass.

---

## Controls & Usage

* **Initial Resolution**: Full HD (**1920 × 1080**) at start, automatically entering fullscreen mode (`glutFullScreen()`).
* **ESC / Q**: Gracefully exit the application.
* **Camera**: Revolving orbital camera around the stencil-reflected character scene.

---

## License & Copyright

Original demo copyright (c) Bastian Kuolt. Modernized and ported to Linux C++23.
