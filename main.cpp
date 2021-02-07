// Copyright 2020
//#include "Windows.h"  // TODO
#include <GL/glut.h>

#include <stdlib.h>
#include "MD2.h"


#define M_PI 3.14159265358979323846

namespace {

GLuint LoadTexture(char *filename) {
    ilutInit();
    const GLuint texture {ilutGLLoadImage(filename) };
    // TODO(bkuolt): error handling

    glBindTexture(GL_TEXTURE_2D, texture);
    ilutGLBuildMipmaps();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}


// forward declarations
void DrawMirroredRoom(void);
void DrawFloor(void);
void DrawModel(void);
void DrawWall(void);
void DrawMirror(void);
void DrawRoom(void);
void DrawPattern(void);

MD2 *file;
int width, height;
float ar;
double a;


void resize(int width, int height) {
    ar = (float) width / (float) height;
    glViewport(0, 0, ::width = width, ::height = height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-ar, ar, -1.0, 1.0, 1.0, 100000.0);
}

void display(void) {
    const double t { clock() / 100.0 };  // 1000.0;
      a = t /100.0;
    static int time;
    static int fps;

    glDepthFunc(GL_LEQUAL);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(sin(a) * 100, 70, cos(a) *400,
              0, 0, 0,
              0, 1, 0);

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
    if (glutGet(GLUT_ELAPSED_TIME) >= time +1000) {
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

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
float room_width {  100 };
float room_height { 100 };
float room_depth {  100 };
float mirror_width { 0.5 };  // in range [0;1]

/**
 * @brief Zeichnet Boden (mit  Mittelpunkt im Ursprung)
 */
void DrawFloor(void) {
    static GLuint list { 0 };
    static GLuint texture { 0 };

    if (list == 0) {
        texture = LoadTexture("C:/glass.jpg");  // TODO(bkuolt): fix path

        glNewList(list = glGenLists(1), GL_COMPILE);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            glPushMatrix();
                glScalef(room_width, room_height, room_depth);

                glBegin(GL_QUADS);
                    glColor4f(1.0, 1.0, 1.0, 0.55);
                    glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, 0.5);
                    glTexCoord2f(1.0, 0.0); glVertex3f(0.5, -0.5, 0.5);
                    glTexCoord2f(1.0, 1.0); glVertex3f(0.5, -0.5, -0.5);
                    glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, -0.5, -0.5);
                glEnd();
            glPopMatrix();

            glBindTexture(GL_TEXTURE_2D, 0);
        glEndList();
    } else {
        glCallList(list);
    }
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
    static GLuint list  { 0 };
    static GLuint texture { 0 };

    if (list == 0) {
        texture = LoadTexture("C:/mirror.jpg");  // TODO(bkuolt): fix path

        glNewList(list = glGenLists(1), GL_COMPILE);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            glPushMatrix();
                glScalef(room_width, room_height, room_depth);

                glBegin(GL_QUADS);
                    glColor4f(1.0, 1.0, 1.0, 0.3);
                    glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, mirror_width);
                    glTexCoord2f(1.0, 0.0); glVertex3f(-0.5, -0.5, -mirror_width);
                    glTexCoord2f(1.0, 1.0); glVertex3f(-0.5,  0.5, -mirror_width);
                    glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, mirror_width);
                glEnd();
            glPopMatrix();
        glEndList();
    } else {
        glCallList(list);
    }
}

/**
 * @brief Zeichnet Raum mit Mittelpunkt im Ursprung
 */
void DrawWall(void) {
    static GLuint list { 0 };
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
                glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, 0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(-0.5, -0.5, mirror_width / 2);
                glTexCoord2f(1.0, 1.0); glVertex3f(-0.5, 0.5, mirror_width / 2);
                glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, 0.5);
                // rechte Spiegelwand
                glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, -0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(-0.5, -0.5, -mirror_width / 2);
                glTexCoord2f(1.0, 1.0); glVertex3f(-0.5, 0.5, -mirror_width / 2);
                glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, -0.5);

                glColor3f(0.0, 1.0, 0.0);
                // Rechts
                glTexCoord2f(0.0, 0.0); glVertex3f(0.5, -0.5, -0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(0.5, -0.5, 0.5);
                glTexCoord2f(1.0, 1.0); glVertex3f(0.5, 0.5, 0.5);
                glTexCoord2f(0.0, 1.0); glVertex3f(0.5, 0.5, -0.5);
                // Vorne
                glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, -0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(0.5, -0.5, -0.5);
                glTexCoord2f(1.0, 1.0); glVertex3f(0.5, 0.5, -0.5);
                glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, -0.5);

                glColor3f(1.0, 0.0, 0.0);
                // Hinten
                glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, -0.5, 0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(0.5, -0.5, 0.5);
                glTexCoord2f(1.0, 1.0); glVertex3f(0.5, 0.5, 0.5);
                glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, 0.5);
            glEnd();

            // Decke
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture[1]);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

            glBegin(GL_QUADS);
                glColor4f(1, 0, 1, 1);
                glTexCoord2f(0.0, 0.0); glVertex3f(-0.5, 0.5, 0.5);
                glTexCoord2f(1.0, 0.0); glVertex3f(0.5, 0.5, 0.5);
                glTexCoord2f(1.0, 1.0); glVertex3f(0.5, 0.5, -0.5);
                glTexCoord2f(0.0, 1.0); glVertex3f(-0.5, 0.5, -0.5);
            glEnd();
            glPopMatrix();
        glEndList();
    } else {
        glCallList(list);
    }
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
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00,

        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
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
    glRectd(-1, -1, 1, 1);
    glDisable(GL_POLYGON_STIPPLE);

    glDisable(GL_STENCIL_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-ar, ar, -1.0, 1.0, 1.0, 100000.0);
}

void key(unsigned char key, int x, int y) {
    switch (key) {
        case 27 /** ESC */:
        case 'q':
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void idle(void) {
    glutPostRedisplay();
}

}  // anonymous namespace

/*
------------------------------------------------
-                  Main                        -
------------------------------------------------ */
int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitWindowSize(640, 480);  // TODO(bkuolt): increase resolution
    glutInitWindowPosition(10, 10);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL);
    glutEnterGameMode();

    glutReshapeFunc(resize);
    glutDisplayFunc(display);
    glutKeyboardFunc(key);
    glutIdleFunc(idle);

    glClearColor(0, 0, 0, 1);
    glutFullScreen();
    glutSetCursor(GLUT_CURSOR_NONE);
    glEnable(GL_DEPTH_TEST);

    printf("Laedt MD2-Modell...\n");
    file = new MD2(GetPath("ogros.md2"));
    file->start(0, 10);
    glEnable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glShadeModel(GL_FLAT);
    glClearStencil(0x00);
    glutMainLoop();
    delete file;

    return EXIT_SUCCESS;
}
