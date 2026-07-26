// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"

#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <glm/glm.hpp>
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

// ============================================================================
// NOTE FOR BASTI (Future OpenGL 4.6 Migration):
// ----------------------------------------------------------------------------
// ApplicationState holds global viewport and timing state.
// In OpenGL 4.6 Core Profile:
// - Replace GLUT callbacks with an explicit GLFW/SDL2 window class.
// - Pass aspect ratio and viewport updates directly to your Camera / MVP UBO.
// ============================================================================
struct ApplicationState {
  int width{1920};
  int height{1080};
  float aspect_ratio{16.0f / 9.0f};
  double animation_time{0.0};
};

ApplicationState g_app;

constexpr float ROOM_WIDTH{100.0f};
constexpr float ROOM_HEIGHT{100.0f};
constexpr float ROOM_DEPTH{100.0f};
constexpr float MIRROR_WIDTH{0.5f};

// Forward declarations of FreeGLUT callbacks
void resizeCallback(int width, int height) noexcept;
void keyboardCallback(unsigned char key, int x, int y) noexcept;
void idleCallback() noexcept;
void displayCallback();

void signalHandler(int signal) noexcept {
  spdlog::info("Received signal {}, shutting down application", signal);
  std::exit(EXIT_FAILURE);
}

[[nodiscard]] GLuint loadTextureWithFallback(std::string_view filename, std::string_view fallback = "glass.jpg") {
  auto path = GetAssetPath(filename);
  if (!std::filesystem::exists(path)) {
    path = GetAssetPath(fallback);
  }
  return LoadImage(path.string());
}

// ============================================================================
// NOTE FOR BASTI (Future OpenGL 4.6 Migration):
// ----------------------------------------------------------------------------
// SceneResources encapsulates legacy display lists (glGenLists, glNewList, glCallList).
// When porting to OpenGL 4.6:
// 1. Replace display lists with VAO (Vertex Array Object) and VBO (Vertex Buffer Object) buffers.
// 2. Replace fixed-function texture environment (GL_TEXTURE_ENV, GL_MODULATE/GL_REPLACE)
//    with fragment shader texture sampling: `fragColor = texture(uTextureSampler, vTexCoord);`.
// 3. For planar reflections, replace stencil buffer multi-pass rendering with a
//    dedicated Framebuffer Object (FBO) render-to-texture pass or reflection shader.
// ============================================================================
class SceneResources {
public:
  SceneResources() = default;

  ~SceneResources() noexcept {
    cleanup();
  }

  void initialize(MD2 *model) {
    _model = model;
    _floor_texture = loadTextureWithFallback("glass.jpg");
    _wall_textures[0] = loadTextureWithFallback("wall.jpg", "glass.jpg");
    _wall_textures[1] = loadTextureWithFallback("ceiling.jpg", "glass.jpg");
    _mirror_texture = loadTextureWithFallback("mirror.jpg", "glass.jpg");

    buildDisplayLists();
  }

  void drawFloor() const {
    if (_floor_list != 0) {
      glCallList(_floor_list);
    }
  }

  void drawWall() const {
    if (_wall_list != 0) {
      glCallList(_wall_list);
    }
  }

  void drawMirror() const {
    if (_mirror_list != 0) {
      glCallList(_mirror_list);
    }
  }

  void drawModel() const {
    if (!_model) return;

    glPushMatrix();
    glTranslatef(0.0f, -25.0f, 0.0f);
    glRotatef(glm::degrees(static_cast<float>(g_app.animation_time)), 0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glRotatef(-180.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    _model->animate();
    glPopMatrix();
    glPopMatrix();
  }

  void drawRoom() const {
    glEnable(GL_BLEND);
    glEnable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);

    // Step 1: Render mirror floor into stencil buffer without writing to color buffer
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

    drawFloor();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glDepthMask(GL_TRUE);

    // Step 2: Render mirrored reflection of character model (scaled y by -1)
    glPushMatrix();
    glTranslatef(0.0f, -100.0f, 0.0f);
    glScalef(1.0f, -1.0f, 1.0f);
    drawModel();
    glPopMatrix();

    // Step 3: Render semi-transparent floor surface
    drawFloor();

    // Step 4: Render primary character model
    glDisable(GL_STENCIL_TEST);
    drawModel();
    glDisable(GL_BLEND);

    // Step 5: Write model depth stencil mask
    glEnable(GL_STENCIL_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glStencilFunc(GL_ALWAYS, 1, 0);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    drawModel();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
  }

  void drawStipplePattern() const {
    constexpr std::array<GLubyte, 128> stipple_pattern = {
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,

        0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF
    };

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glEnable(GL_POLYGON_STIPPLE);
    glPolygonStipple(stipple_pattern.data());
    glColor3f(1.1f, 1.0f, 1.0f);
    glRectd(-1.0, -1.0, 1.0, 1.0);
    glDisable(GL_POLYGON_STIPPLE);

    glDisable(GL_STENCIL_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-g_app.aspect_ratio, g_app.aspect_ratio, -1.0, 1.0, 1.0, 100000.0);
  }

private:
  void buildDisplayLists() {
    // 1. Build Floor List
    _floor_list = glGenLists(1);
    glNewList(_floor_list, GL_COMPILE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _floor_texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_DEPTH);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.55f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, -0.5f);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();

    // 2. Build Mirror List
    _mirror_list = glGenLists(1);
    glNewList(_mirror_list, GL_COMPILE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _mirror_texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_DEPTH);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.3f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, MIRROR_WIDTH);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-0.5f, -0.5f, -MIRROR_WIDTH);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-0.5f, 0.5f, -MIRROR_WIDTH);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, MIRROR_WIDTH);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();

    // 3. Build Wall List
    _wall_list = glGenLists(1);
    glNewList(_wall_list, GL_COMPILE);
    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_WIDTH);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _wall_textures[0]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-0.5f, -0.5f, MIRROR_WIDTH / 2.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-0.5f, 0.5f, MIRROR_WIDTH / 2.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, 0.5f);

    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-0.5f, -0.5f, -MIRROR_WIDTH / 2.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-0.5f, 0.5f, -MIRROR_WIDTH / 2.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, -0.5f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(0.5f, 0.5f, -0.5f);

    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, -0.5f);

    glColor3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, _wall_textures[1]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();
  }

  void cleanup() noexcept {
    if (_floor_list != 0) { glDeleteLists(_floor_list, 1); _floor_list = 0; }
    if (_mirror_list != 0) { glDeleteLists(_mirror_list, 1); _mirror_list = 0; }
    if (_wall_list != 0) { glDeleteLists(_wall_list, 1); _wall_list = 0; }

    if (_floor_texture != 0) { glDeleteTextures(1, &_floor_texture); _floor_texture = 0; }
    if (_mirror_texture != 0) { glDeleteTextures(1, &_mirror_texture); _mirror_texture = 0; }
    if (_wall_textures[0] != 0) { glDeleteTextures(1, &_wall_textures[0]); _wall_textures[0] = 0; }
    if (_wall_textures[1] != 0) { glDeleteTextures(1, &_wall_textures[1]); _wall_textures[1] = 0; }
  }

private:
  MD2 *_model{nullptr};
  GLuint _floor_texture{0};
  GLuint _mirror_texture{0};
  GLuint _wall_textures[2]{0, 0};

  GLuint _floor_list{0};
  GLuint _mirror_list{0};
  GLuint _wall_list{0};
};

std::unique_ptr<MD2> g_model;
std::unique_ptr<SceneResources> g_scene;

void resizeCallback(int width, int height) noexcept {
  g_app.width = width;
  g_app.height = (height == 0) ? 1 : height;
  g_app.aspect_ratio = static_cast<float>(g_app.width) / static_cast<float>(g_app.height);

  glViewport(0, 0, g_app.width, g_app.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-g_app.aspect_ratio, g_app.aspect_ratio, -1.0, 1.0, 1.0, 100000.0);
}

void keyboardCallback(unsigned char key_code, int x, int y) noexcept {
  switch (key_code) {
  case 27: // ESC key
  case 'q':
  case 'Q':
    spdlog::info("User requested exit");
    glutLeaveMainLoop();
    break;
  default:
    break;
  }
  glutPostRedisplay();
}

void idleCallback() noexcept {
  glutPostRedisplay();
}

void displayCallback() {
  const double time_seconds = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
  g_app.animation_time = time_seconds;

  static int last_time = 0;
  static int frame_counter = 0;
  static std::string fps_text{"BGL Animation Tech Demo @ 0 FPS"};

  glDepthFunc(GL_LEQUAL);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // GLM camera calculation
  const glm::vec3 eye(
      std::sin(static_cast<float>(g_app.animation_time)) * 100.0f,
      70.0f,
      std::cos(static_cast<float>(g_app.animation_time)) * 400.0f
  );
  constexpr glm::vec3 center(0.0f, 0.0f, 0.0f);
  constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

  gluLookAt(eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z);
  glScalef(3.0f, 3.0f, 3.0f);

  if (g_scene) {
    glPushMatrix();
    glTranslatef(0.0f, 20.0f, 0.0f);
    g_scene->drawRoom();
    glPopMatrix();

    g_scene->drawStipplePattern();
  }

  // Calculate FPS
  const int current_time = glutGet(GLUT_ELAPSED_TIME);
  if (current_time >= last_time + 1000) {
    fps_text = std::format("BGL Animation Tech Demo @ {} FPS", frame_counter);
    frame_counter = 0;
    last_time = current_time;
  } else {
    frame_counter++;
  }

  // Render 2D FPS Text Overlay using orthographic projection
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, g_app.width, 0, g_app.height);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glColor3f(1.0f, 0.0f, 0.0f);
  glRasterPos2i(10, 20);
  glutBitmapString(GLUT_BITMAP_HELVETICA_18, fps_text);

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glutSwapBuffers();
}

} // namespace bgl

int main(int argc, char *argv[]) {
  std::signal(SIGINT, bgl::signalHandler);

  spdlog::info("Starting BGL Reflection Demo on amd64 Ubuntu Linux (C++23)");

  // Force FreeGLUT to use XWayland/X11 display backend on Wayland desktop sessions
  if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
    spdlog::info("Wayland desktop detected: routing FreeGLUT display connection through XWayland/X11");
    unsetenv("WAYLAND_DISPLAY");
  }

  glutInit(&argc, argv);
  glutInitWindowSize(bgl::g_app.width, bgl::g_app.height);
  glutInitWindowPosition(0, 0);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL);

  glutCreateWindow("BGL Reflection Demo (amd64 Ubuntu)");
  glutSetCursor(GLUT_CURSOR_NONE);
  glutFullScreen();

  glutReshapeFunc(bgl::resizeCallback);
  glutDisplayFunc(bgl::displayCallback);
  glutKeyboardFunc(bgl::keyboardCallback);
  glutIdleFunc(bgl::idleCallback);

  try {
    spdlog::info("Loading MD2 model...");
    const auto model_path = bgl::GetAssetPath("Ogros.md2");
    bgl::g_model = std::make_unique<bgl::MD2>(model_path);
    bgl::g_model->start(0, 10);

    spdlog::info("Initializing scene resources...");
    bgl::g_scene = std::make_unique<bgl::SceneResources>();
    bgl::g_scene->initialize(bgl::g_model.get());
  } catch (const std::exception &error) {
    spdlog::error("Error initializing scene: {}", error.what());
    return EXIT_FAILURE;
  }

  glClearStencil(0x00);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  spdlog::info("Entering FreeGLUT main loop");
  glutMainLoop();

  // Reset resources on clean shutdown
  bgl::g_scene.reset();
  bgl::g_model.reset();

  return EXIT_SUCCESS;
}
