from conan import ConanFile
from conan.tools.cmake import cmake_layout


class BglReflectionDemoConan(ConanFile):
    name = "bgl-reflection-demo"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    default_options = {
        "glad/*:gl_version": "4.6",
        "glad/*:gl_profile": "core",
    }

    def requirements(self):
        self.requires("glfw/3.4")
        self.requires("glad/0.1.36")
        self.requires("devil/1.8.0")
        self.requires("opengl/system")
        self.requires("spdlog/1.15.0")
        self.requires("glm/1.0.1")

    def layout(self):
        cmake_layout(self)
