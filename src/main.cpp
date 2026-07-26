// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"
#include "Shader.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace bgl {

struct ApplicationState {
  int width{1920};
  int height{1080};
  float aspect_ratio{16.0f / 9.0f};
  double animation_time{0.0};
};

ApplicationState g_app;
bool g_is_fullscreen{true};
int g_windowed_x{100};
int g_windowed_y{100};
int g_windowed_w{1920};
int g_windowed_h{1080};

constexpr float FLOOR_WIDTH{150.0f};
constexpr float FLOOR_DEPTH{150.0f};

void framebufferSizeCallback(GLFWwindow *window, int width, int height) noexcept {
  g_app.width = width;
  g_app.height = (height == 0) ? 1 : height;
  g_app.aspect_ratio = static_cast<float>(g_app.width) / static_cast<float>(g_app.height);
  glViewport(0, 0, g_app.width, g_app.height);
}

void toggleFullscreen(GLFWwindow *window) noexcept {
  GLFWmonitor *primary = glfwGetPrimaryMonitor();
  if (!primary) return;

  const GLFWvidmode *mode = glfwGetVideoMode(primary);
  if (!mode) return;

  if (!g_is_fullscreen) {
    glfwGetWindowPos(window, &g_windowed_x, &g_windowed_y);
    glfwGetWindowSize(window, &g_windowed_w, &g_windowed_h);
    glfwSetWindowMonitor(window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
    g_is_fullscreen = true;
    spdlog::info("Switched to Fullscreen mode ({}x{}@{}Hz)", mode->width, mode->height, mode->refreshRate);
  } else {
    glfwSetWindowMonitor(window, nullptr, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, GLFW_DONT_CARE);
    g_is_fullscreen = false;
    spdlog::info("Switched to Windowed mode ({}x{})", g_windowed_w, g_windowed_h);
  }
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) noexcept {
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
      spdlog::info("User requested application shutdown via key event");
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_F11 || key == GLFW_KEY_F) {
      toggleFullscreen(window);
    }
  }
}

[[nodiscard]] GLuint loadTextureWithFallback(std::string_view filename, std::string_view fallback = "glass.jpg") {
  auto path = GetAssetPath(filename);
  if (!std::filesystem::exists(path)) {
    path = GetAssetPath(fallback);
  }
  return LoadImage(path.string());
}

class SceneResources {
public:
  SceneResources() = default;
  ~SceneResources() noexcept { cleanup(); }

  void initialize(MD2Model *model, MD2Renderer *renderer) {
    _model = model;
    _renderer = renderer;

    spdlog::info("Loading shader files: shaders/reflection_demo.vert and shaders/reflection_demo.frag");
    _shader.loadFromFile("shaders/reflection_demo.vert", "shaders/reflection_demo.frag");

    _floor_texture = loadTextureWithFallback("glass.jpg");

    setupQuadVAO();
  }

  void render(const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &eye_pos) const {
    if (_renderer && _model) {
      _renderer->updateBuffers(*_model);
    }

    _shader.use();
    _shader.setVec3("uViewPos", eye_pos);
    drawScene(view, projection);
  }

private:
  void setupQuadVAO() {
    struct Vertex {
      glm::vec3 pos;
      glm::vec2 tex;
      glm::vec3 norm;
    };

    const std::array<Vertex, 4> vertices = {{
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    }};

    const std::array<std::uint32_t, 6> indices = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &_quad_vao);
    glGenBuffers(1, &_quad_vbo);
    glGenBuffers(1, &_quad_ibo);

    glBindVertexArray(_quad_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _quad_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, tex));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, norm));

    glBindVertexArray(0);
  }

  void drawFloorQuad(const glm::mat4 &model) const {
    _shader.setMat4("uModel", model);
    _shader.setBool("uUseTexture", _floor_texture != 0);
    _shader.setBool("uIsCheckerboard", false);
    _shader.setBool("uEnableLighting", true);
    _shader.setVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.65f));

    if (_floor_texture != 0) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, _floor_texture);
      _shader.setInt("uTexture", 0);
    }

    glBindVertexArray(_quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  }

  void drawModel(const glm::mat4 &parent_transform = glm::mat4(1.0f)) const {
    if (!_renderer) return;

    _shader.setBool("uIsCheckerboard", false);
    _shader.setBool("uEnableLighting", true);
    _shader.setVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    glm::mat4 model = parent_transform;
    model = glm::translate(model, glm::vec3(0.0f, -25.0f, 0.0f));
    model = glm::rotate(model, static_cast<float>(g_app.animation_time), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    _renderer->render(_shader, model);
  }

  void drawCheckerboardBackground() const {
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);

    _shader.setMat4("uModel", glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 1.0f)));
    _shader.setMat4("uView", glm::mat4(1.0f));
    _shader.setMat4("uProjection", glm::mat4(1.0f));
    _shader.setBool("uIsCheckerboard", true);
    _shader.setBool("uEnableLighting", false);

    glBindVertexArray(_quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
  }

  void drawScene(const glm::mat4 &view, const glm::mat4 &projection) const {
    // 1. Render background quad first behind all 3D geometry
    drawCheckerboardBackground();

    // 2. Set camera view and projection matrices for 3D scene
    _shader.setMat4("uView", view);
    _shader.setMat4("uProjection", projection);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_STENCIL_TEST);

    // Step A: Write glass floor quad into stencil buffer
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glm::mat4 floor_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -50.0f, 0.0f));
    floor_model = glm::rotate(floor_model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    floor_model = glm::scale(floor_model, glm::vec3(FLOOR_WIDTH, FLOOR_DEPTH, 1.0f));

    drawFloorQuad(floor_model);

    // Step B: Render mirrored reflection of character model inside stencil mask
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask(GL_TRUE);

    glm::mat4 reflection_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -100.0f, 0.0f));
    reflection_model = glm::scale(reflection_model, glm::vec3(1.0f, -1.0f, 1.0f));
    drawModel(reflection_model);

    // Step C: Draw semi-transparent glass floor quad over reflection
    drawFloorQuad(floor_model);

    // 3. Render primary character model on top (100% opaque, smooth-illuminated)
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    drawModel(glm::mat4(1.0f));
  }

  void cleanup() noexcept {
    if (_quad_vao != 0) glDeleteVertexArrays(1, &_quad_vao);
    if (_quad_vbo != 0) glDeleteBuffers(1, &_quad_vbo);
    if (_quad_ibo != 0) glDeleteBuffers(1, &_quad_ibo);

    if (_floor_texture != 0) glDeleteTextures(1, &_floor_texture);
  }

private:
  MD2Model *_model{nullptr};
  MD2Renderer *_renderer{nullptr};
  Shader _shader;
  GLuint _floor_texture{0};

  GLuint _quad_vao{0};
  GLuint _quad_vbo{0};
  GLuint _quad_ibo{0};
};

std::unique_ptr<MD2Model> g_model;
std::unique_ptr<MD2Renderer> g_renderer;
std::unique_ptr<SceneResources> g_scene;

} // namespace bgl

int main(int argc, char *argv[]) {
  spdlog::info("Starting BGL Reflection Demo on amd64 Ubuntu Linux (OpenGL 4.6 Core, GLFW, C++23)");

  if (!glfwInit()) {
    spdlog::error("Failed to initialize GLFW library");
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWmonitor *primary_monitor = glfwGetPrimaryMonitor();
  if (primary_monitor) {
    const GLFWvidmode *mode = glfwGetVideoMode(primary_monitor);
    if (mode) {
      bgl::g_app.width = mode->width;
      bgl::g_app.height = mode->height;
      bgl::g_app.aspect_ratio = static_cast<float>(mode->width) / static_cast<float>(mode->height);
    }
  }

  GLFWwindow *window = glfwCreateWindow(
      bgl::g_app.width, bgl::g_app.height,
      "BGL Reflection Demo (OpenGL 4.6 Core Profile)",
      primary_monitor, nullptr
  );

  if (!window) {
    spdlog::error("Failed to create GLFW fullscreen window");
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, bgl::framebufferSizeCallback);
  glfwSetKeyCallback(window, bgl::keyCallback);

  // Initialize GLAD OpenGL 4.6 Loader
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    spdlog::error("Failed to initialize GLAD OpenGL loader");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  spdlog::info("OpenGL Version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
  spdlog::info("GPU Vendor: {}", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
  spdlog::info("GPU Renderer: {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
  glClearStencil(0x00);

  try {
    spdlog::info("Loading MD2 model...");
    const auto model_path = bgl::GetAssetPath("Ogros.md2");
    bgl::g_model = std::make_unique<bgl::MD2Model>(model_path);
    bgl::g_model->start(0, 10);

    spdlog::info("Initializing OpenGL 4.6 MD2 renderer...");
    bgl::g_renderer = std::make_unique<bgl::MD2Renderer>(*bgl::g_model, "igdosh.jpeg");

    spdlog::info("Initializing OpenGL 4.6 scene resources...");
    bgl::g_scene = std::make_unique<bgl::SceneResources>();
    bgl::g_scene->initialize(bgl::g_model.get(), bgl::g_renderer.get());
  } catch (const std::exception &error) {
    spdlog::error("Initialization error: {}", error.what());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  double last_time = glfwGetTime();
  int frame_count = 0;
  double fps_timer = last_time;

  spdlog::info("Entering GLFW main loop");
  while (!glfwWindowShouldClose(window)) {
    const double current_time = glfwGetTime();
    const float delta_time = static_cast<float>(current_time - last_time);
    last_time = current_time;

    bgl::g_app.animation_time = current_time;

    // FPS calculation
    frame_count++;
    if (current_time - fps_timer >= 1.0) {
      const std::string title = std::format("BGL Reflection Demo (OpenGL 4.6 Core) @ {} FPS", frame_count);
      glfwSetWindowTitle(window, title.c_str());
      frame_count = 0;
      fps_timer = current_time;
    }

    if (bgl::g_model) {
      bgl::g_model->update(delta_time);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Compute View and Projection Matrices (Closer camera distance & model framing)
    constexpr float camera_radius = 180.0f;
    const glm::vec3 eye(
        std::sin(static_cast<float>(bgl::g_app.animation_time) * 0.4f) * camera_radius,
        35.0f + std::sin(static_cast<float>(bgl::g_app.animation_time) * 0.2f) * 15.0f,
        std::cos(static_cast<float>(bgl::g_app.animation_time) * 0.4f) * camera_radius
    );
    constexpr glm::vec3 center(0.0f, -25.0f, 0.0f);
    constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

    const glm::mat4 view = glm::lookAt(eye, center, up);
    const glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        bgl::g_app.aspect_ratio,
        1.0f, 10000.0f
    );

    if (bgl::g_scene) {
      bgl::g_scene->render(view, projection, eye);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  spdlog::info("Cleaning up resources and terminating GLFW");
  bgl::g_scene.reset();
  bgl::g_renderer.reset();
  bgl::g_model.reset();

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
