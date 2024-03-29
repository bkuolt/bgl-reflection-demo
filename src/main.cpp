// Copyright 2021
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>


#include "MD2.hpp"
#include <GL/glut.h>

namespace {

// forward declarations
void resize(int width, int height);
void key(unsigned char key, int x, int y);
void idle(void);
void display(void);

std::unique_ptr<MD2> file;

void SignalHandler(int signal) noexcept { std::exit(EXIT_FAILURE); }

} // anonymous namespace

int main(int argc, char *argv[]) {
  std::signal(SIGINT, SignalHandler);

  glutInit(&argc, argv);
  glutInitWindowSize(1280, 720);
  glutInitWindowPosition(0, 0);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL);
  glutSetCursor(GLUT_CURSOR_NONE);

  glutReshapeFunc(resize);
  glutDisplayFunc(display);
  glutKeyboardFunc(key);
  glutIdleFunc(idle);

  glutEnterGameMode();
  glutFullScreen();

  try {
    std::iostream << "Loading MD2-Modell..." << std::endl;
    file = std::make_unique<MD2>(GetPath("ogros.md2"));
    file->start(0, 10);
  } catch (const std::exception &error) {
    std::cerr << error.what();
    return EXIT_FAILURE;
  }

  glClearStencil(0x00);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glutMainLoop();
  return EXIT_SUCCESS;
}

/*
------------------------------------------------
-                  Details                     -
------------------------------------------------ */
namespace {

constexpr double M_PI{3.14159265358979323846};

struct {
  int width, height;
  float ar;
  double a;
} App;

// forward declarations
void DrawMirroredRoom(void);
void DrawFloor(void);
void DrawModel(void);
void DrawWall(void);
void DrawMirror(void);
void DrawRoom(void);
void DrawPattern(void);

/* ----------------------- Callbacks ----------------------- */

void resize(int width, int height) {
  App.width = width;
  App.height = height;
  App.ar = static_cast<float>(width) / height;

  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-App.ar, App.ar, -1.0, 1.0, 1.0, 100000.0);
}

void key(unsigned char key, int x, int y) {
  switch (key) {
  case 27 /** ESC */:
  case 'q':
    std::exit(0);
    break;
  }
  glutPostRedisplay();
}

void idle(void) { glutPostRedisplay(); }

void display(void) {
  const double t{clock() / 100.0}; // 1000.0;
  a = t / 100.0;
  static int time;
  static int fps;

  glDepthFunc(GL_LEQUAL);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  gluLookAt(sin(a) * 100, 70, cos(a) * 400, 0, 0, 0, 0, 1, 0);
  glScalef(3, 3, 3);

  glPushMatrix();
  glPushMatrix();
  glTranslatef(0, 20, 0);
  DrawRoom();
  glPopMatrix();
  glPopMatrix();

  DrawPattern();

  // FPS
  static char buffer[100];
  if (glutGet(GLUT_ELAPSED_TIME) >= time + 1000) {
    sprintf(buffer, "BGL Animation Tech Demo @ %i FPS", fps);
    fps = 0;
    time = glutGet(GLUT_ELAPSED_TIME);
  } else {
    fps++;
  }
  glColor3f(1, 0, 0);
  glWindowPos2i(3, 5);

  glutBitmapString(GLUT_BITMAP_HELVETICA_18, buffer);
  glutSwapBuffers();
}

/* ------------------ Rendering Details -------------------- */

GLuint LoadTexture(const std::string &filename) {
  ilutInit();
  const GLuint texture{ilutGLLoadImage(filename.c_str())};
  if (texture == 0) {
    // TODO(bkuolt): error handling
    return 0;
  }

  glBindTexture(GL_TEXTURE_2D, texture);
  ilutGLBuildMipmaps();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

//------------------------------------------------------------------------------------------------

const float room_width{100};
const float room_height{100};
const float room_depth{100};
const float mirror_width{0.5}; // in range [0;1]

/**
 * @brief Zeichnet Boden (mit  Mittelpunkt im Ursprung)
 */
void DrawFloor(void) {
  static GLuint list{0};
  static GLuint texture{0};

  if (list == 0) {
    texture = LoadTexture("C:/glass.jpg"); // TODO(bkuolt): fix path
    list = glGenLists(1);

    glNewList(list, GL_COMPILE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(room_width, room_height, room_depth);
    glBegin(GL_QUADS);
    glColor4f(1.0, 1.0, 1.0, 0.55);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(0.5, -0.5, -0.5);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, -0.5, -0.5);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);

    glEndList();
  }

  glCallList(list);
}

/**
 * @brief Zeichnet Model
 */
void DrawModel(void) {
  glPushMatrix();
  glTranslatef(0.0, -25.0, 0.0);
  glRotatef(a, 0, 1, 0);
  glPushMatrix();
  glRotatef(-180, 0, 1, 0);
  glRotatef(-90, 1, 0, 0);
  glRotatef(90, 0, 0, 1);
  file->animate();
  glPopMatrix();
  glPopMatrix();
}

/**
 * @brief Zeichnet Spiegel
 */
void DrawMirror(void) {
  static GLuint list{0};
  static GLuint texture{0};

  if (list == 0) {
    texture = LoadTexture("C:/mirror.jpg"); // TODO(bkuolt): fix path
    list = glGenLists(1)

        glNewList(list, GL_COMPILE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glScalef(room_width, room_height, room_depth);

    glBegin(GL_QUADS);
    glColor4f(1.0, 1.0, 1.0, 0.3);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, mirror_width);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(-0.5, -0.5, -mirror_width);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(-0.5, 0.5, -mirror_width);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, mirror_width);
    glEnd();
    glPopMatrix();

    glEndList();
  }

  glCallList(list);
}

/**
 * @brief Zeichnet Raum mit Mittelpunkt im Ursprung
 */
void DrawWall(void) {
  static GLuint list{0};
  static GLuint texture[2];

  if (list == 0) {
    glNewList(list = glGenLists(1), GL_COMPILE);

    glPushMatrix();
    glScalef(room_width, room_height, room_width);

    texture[0] = LoadTexture("C:/wall.jpg");
    texture[1] = LoadTexture("C:/ceiling.jpg");

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor3f(0.0, 0.0, 1.0);

    // linke Spiegelwand
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(-0.5, -0.5, mirror_width / 2);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(-0.5, 0.5, mirror_width / 2);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, 0.5);
    // rechte Spiegelwand
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, -0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(-0.5, -0.5, -mirror_width / 2);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(-0.5, 0.5, -mirror_width / 2);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, -0.5);

    glColor3f(0.0, 1.0, 0.0);
    // Rechts
    glTexCoord2f(0.0, 0.0);
    glVertex3f(0.5, -0.5, -0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(0.5, 0.5, 0.5);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(0.5, 0.5, -0.5);
    // Vorne
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, -0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(0.5, -0.5, -0.5);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(0.5, 0.5, -0.5);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, -0.5);

    glColor3f(1.0, 0.0, 0.0);
    // Hinten
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(0.5, -0.5, 0.5);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(0.5, 0.5, 0.5);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, 0.5);
    glEnd();

    // Decke
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture[1]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    glColor4f(1, 0, 1, 1);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-0.5, 0.5, 0.5);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(0.5, 0.5, 0.5);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(0.5, 0.5, -0.5);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-0.5, 0.5, -0.5);
    glEnd();
    glPopMatrix();

    glEndList();
  }

  glCallList(list);
}

void DrawMirroredRoom(void) {
  glPushMatrix();
  // Zeichnet Spiegelbild
  glTranslatef(-room_width, 0.0, 0.0);
  glScalef(-1.0, 1.0, 1.0);
  DrawRoom();
  glPopMatrix();
}

void DrawRoom(void) {
  // glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glEnable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE);
  // Zeichnet Spiegel in Stencil-Buffer
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, 1, 0);
  glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

  DrawFloor();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glDepthMask(GL_TRUE);
  // Zeichnet Gespiegeltes Modell
  // glEnable(GL_DEPTH_TEST);

  glPushMatrix();
  glTranslatef(0.0, -100, 0.0);
  glScalef(1.0, -1.0, 1.0);
  DrawModel();
  glPopMatrix();

  // Zeichnet Speigel
  DrawFloor();

  // normales Modell
  glDisable(GL_STENCIL_TEST);
  DrawModel();
  glDisable(GL_BLEND);

  // Modell in Depth Buffer
  glEnable(GL_STENCIL_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  glStencilFunc(GL_ALWAYS, 1, 0);
  glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
  DrawModel();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_STENCIL_TEST);
}

void DrawPattern(void) {
  static GLubyte stipple[] = {
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
  gluOrtho2D(-1, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_NOTEQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // Zeichnet Muster
  glEnable(GL_POLYGON_STIPPLE);
  glPolygonStipple(stipple);
  glColor3f(1.1, 1, 1);
  glRectd(-1.0, -1.0, 1.0, 1.0);
  glDisable(GL_POLYGON_STIPPLE);

  glDisable(GL_STENCIL_TEST);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-ar, ar, -1.0, 1.0, 1.0, 100000.0);
}

} // anonymous namespace
