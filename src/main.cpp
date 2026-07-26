// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"

#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>

namespace {

// Forward declarations
void resize(int width, int height);
void key(unsigned char key, int x, int y);
void idle();
void display();

std::unique_ptr<MD2> model_file;

void signalHandler(int signal) noexcept {
  spdlog::info("Received signal {}, shutting down application", signal);
  std::exit(EXIT_FAILURE);
}

struct ApplicationState {
  int width{1280};
  int height{720};
  float ar{16.0f / 9.0f};
  double a{0.0};
};

ApplicationState app;

constexpr float ROOM_WIDTH{100.0f};
constexpr float ROOM_HEIGHT{100.0f};
constexpr float ROOM_DEPTH{100.0f};
constexpr float MIRROR_WIDTH{0.5f};

void drawMirroredRoom();
void drawFloor();
void drawModel();
void drawWall();
void drawMirror();
void drawRoom();
void drawPattern();

GLuint loadTextureWithFallback(const std::string &filename, const std::string &fallback = "glass.jpg") {
  auto path = bgl::GetAssetPath(filename);
  if (!std::filesystem::exists(path)) {
    path = bgl::GetAssetPath(fallback);
  }
  return bgl::LoadImage(path.string());
}

} // anonymous namespace

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);

  spdlog::info("Starting BGL Reflection Demo on amd64 Ubuntu Linux (C++23)");

  // Force FreeGLUT to use XWayland/X11 display backend on Wayland desktop sessions
  if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
    spdlog::info("Wayland desktop detected: routing FreeGLUT display connection through XWayland/X11");
    unsetenv("WAYLAND_DISPLAY");
  }

  glutInit(&argc, argv);
  glutInitWindowSize(app.width, app.height);
  glutInitWindowPosition(0, 0);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL);

  glutCreateWindow("BGL Reflection Demo (amd64 Ubuntu)");
  glutSetCursor(GLUT_CURSOR_NONE);
  glutFullScreen();

  glutReshapeFunc(resize);
  glutDisplayFunc(display);
  glutKeyboardFunc(key);
  glutIdleFunc(idle);

  try {
    spdlog::info("Loading MD2 model...");
    auto model_path = bgl::GetAssetPath("Ogros.md2");
    model_file = std::make_unique<MD2>(model_path.string());
    model_file->start(0, 10);
  } catch (const std::exception &error) {
    spdlog::error("Error loading model: {}", error.what());
    return EXIT_FAILURE;
  }

  glClearStencil(0x00);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  spdlog::info("Entering FreeGLUT main loop");
  glutMainLoop();
  return EXIT_SUCCESS;
}

namespace {

void resize(int width, int height) {
  app.width = width;
  app.height = (height == 0) ? 1 : height;
  app.ar = static_cast<float>(app.width) / static_cast<float>(app.height);

  glViewport(0, 0, app.width, app.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-app.ar, app.ar, -1.0, 1.0, 1.0, 100000.0);
}

void key(unsigned char key_code, int x, int y) {
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

void idle() {
  glutPostRedisplay();
}

void display() {
  const double t = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
  app.a = t;

  static int last_time = 0;
  static int fps = 0;
  static std::string fps_text{"BGL Animation Tech Demo @ 0 FPS"};

  glDepthFunc(GL_LEQUAL);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // GLM math for camera view calculation
  glm::vec3 eye(
      std::sin(static_cast<float>(app.a)) * 100.0f,
      70.0f,
      std::cos(static_cast<float>(app.a)) * 400.0f
  );
  glm::vec3 center(0.0f, 0.0f, 0.0f);
  glm::vec3 up(0.0f, 1.0f, 0.0f);

  gluLookAt(eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z);
  glScalef(3.0f, 3.0f, 3.0f);

  glPushMatrix();
  glPushMatrix();
  glTranslatef(0.0f, 20.0f, 0.0f);
  drawRoom();
  glPopMatrix();
  glPopMatrix();

  drawPattern();

  // Calculate FPS
  int current_time = glutGet(GLUT_ELAPSED_TIME);
  if (current_time >= last_time + 1000) {
    fps_text = std::format("BGL Animation Tech Demo @ {} FPS", fps);
    fps = 0;
    last_time = current_time;
  } else {
    fps++;
  }

  // Render 2D FPS Text Overlay using orthographic projection
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, app.width, 0, app.height);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glColor3f(1.0f, 0.0f, 0.0f);
  glRasterPos2i(10, 20);
  bgl::glutBitmapString(GLUT_BITMAP_HELVETICA_18, fps_text.c_str());

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glutSwapBuffers();
}

void drawFloor() {
  static GLuint list{0};
  static GLuint texture{0};

  if (list == 0) {
    texture = loadTextureWithFallback("glass.jpg");
    list = glGenLists(1);

    glNewList(list, GL_COMPILE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_DEPTH);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.55f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);

    glEndList();
  }

  glCallList(list);
}

void drawModel() {
  if (!model_file) return;

  glPushMatrix();
  glTranslatef(0.0f, -25.0f, 0.0f);
  glRotatef(glm::degrees(static_cast<float>(app.a)), 0.0f, 1.0f, 0.0f);
  glPushMatrix();
  glRotatef(-180.0f, 0.0f, 1.0f, 0.0f);
  glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
  glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
  model_file->animate();
  glPopMatrix();
  glPopMatrix();
}

void drawMirror() {
  static GLuint list{0};
  static GLuint texture{0};

  if (list == 0) {
    texture = loadTextureWithFallback("mirror.jpg", "glass.jpg");
    list = glGenLists(1);

    glNewList(list, GL_COMPILE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_DEPTH);

    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.3f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, MIRROR_WIDTH);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -MIRROR_WIDTH);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -MIRROR_WIDTH);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, MIRROR_WIDTH);
    glEnd();
    glPopMatrix();

    glEndList();
  }

  glCallList(list);
}

void drawWall() {
  static GLuint list{0};
  static GLuint texture[2]{0, 0};

  if (list == 0) {
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);

    glPushMatrix();
    glScalef(ROOM_WIDTH, ROOM_HEIGHT, ROOM_WIDTH);

    texture[0] = loadTextureWithFallback("wall.jpg", "glass.jpg");
    texture[1] = loadTextureWithFallback("ceiling.jpg", "glass.jpg");

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor3f(0.0f, 0.0f, 1.0f);

    // Left mirror wall
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, MIRROR_WIDTH / 2.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, MIRROR_WIDTH / 2.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    // Right mirror wall
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -MIRROR_WIDTH / 2.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -MIRROR_WIDTH / 2.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glColor3f(0.0f, 1.0f, 0.0f);
    // Right wall
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    // Front wall
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glColor3f(1.0f, 0.0f, 0.0f);
    // Back wall
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();

    // Ceiling
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture[1]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glPopMatrix();

    glEndList();
  }

  glCallList(list);
}

void drawMirroredRoom() {
  glPushMatrix();
  // Render mirrored reflection
  glTranslatef(-ROOM_WIDTH, 0.0f, 0.0f);
  glScalef(-1.0f, 1.0f, 1.0f);
  drawRoom();
  glPopMatrix();
}

void drawRoom() {
  glEnable(GL_BLEND);
  glEnable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE);

  // Render mirror plane into stencil buffer
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, 1, 0);
  glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

  drawFloor();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glDepthMask(GL_TRUE);

  // Render mirrored reflection model
  glPushMatrix();
  glTranslatef(0.0f, -100.0f, 0.0f);
  glScalef(1.0f, -1.0f, 1.0f);
  drawModel();
  glPopMatrix();

  // Render mirror surface
  drawFloor();

  // Render normal model
  glDisable(GL_STENCIL_TEST);
  drawModel();
  glDisable(GL_BLEND);

  // Render model into depth buffer
  glEnable(GL_STENCIL_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  glStencilFunc(GL_ALWAYS, 1, 0);
  glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
  drawModel();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_STENCIL_TEST);
}

void drawPattern() {
  static const GLubyte stipple[] = {
      0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
      0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
      0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
      0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
      0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,

      0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
      0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
      0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
      0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
      0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
  };

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_NOTEQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // Draw stipple pattern
  glEnable(GL_POLYGON_STIPPLE);
  glPolygonStipple(stipple);
  glColor3f(1.1f, 1.0f, 1.0f);
  glRectd(-1.0, -1.0, 1.0, 1.0);
  glDisable(GL_POLYGON_STIPPLE);

  glDisable(GL_STENCIL_TEST);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-app.ar, app.ar, -1.0, 1.0, 1.0, 100000.0);
}

} // anonymous namespace
