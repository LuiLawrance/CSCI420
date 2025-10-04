/*
  CSCI 420 Computer Graphics, Computer Science, USC
  Assignment 1: Height Fields with Shaders.
  C/C++ starter code

  Student username: lzlui
*/

#include "openGLHeader.h"

#include "imageIO.h"
#include "glutHeader.h"
#include "openGLMatrix.h"
#include "pipelineProgram.h"
#include "vao.h"
#include "vbo.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper";
#endif

using namespace std;

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f }; 
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 1";

// Number of vertices in the single triangle (starter code).
int numVertices;

ImageIO* heightmapImage;

int bytesPerPixel;
typedef enum { GRAYSCALE, COLOR } COLOR_STATE;
COLOR_STATE colorState = GRAYSCALE;
ImageIO* colorImage;
int ARGC;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram pipelineProgram;
VBO vboVertices;
VBO vboColors;
VAO vao;

int imageW, imageH;
float height, scale, red, green, blue, alpha = 1.0;
std::vector<float> pointPosition, pointColor;
std::vector<float> linePosition, lineColor;
std::vector<float> fixedColorMeshPosition, fixedColorMeshColor;
std::vector<float> trianglePosition, triangleColor;

std::vector<float> triPleftPosition, triPrightPosition, triPdownPosition, triPupPosition;

std::vector<float> trianglePosition_m1;

// Display option
enum class DISPLAY_OPTION { POINTS, LINES, TRIANGLES, SMOOTH, MESHANDSOLID };
DISPLAY_OPTION displayOption = DISPLAY_OPTION::POINTS;

// To make the heightfield rotate automatically about y axis
bool autoRot = false; 
int toggle = -1;
// Screenshot counter
int shotCount = 0;
// To start 300 screenshots
bool takeShots = false;
// For animation
bool animate = false;

// Declare somewhere (members/globals as you had):
VAO vaoPoint, vaoLine, vaoFixedColorMesh, vaoTriangle;
VBO vboPointPos, vboPointCol;
VBO vboLinePos, vboLineCol;
VBO vboMeshPos, vboMeshCol;
VBO vboTriPos, vboTriCol;

VAO vaoMode1;
VBO vboTriPleft, vboTriPright, vboTriPdown, vboTriPup;
VBO vboTriPos_m1, vboTriCol_m1;

// Write a screenshot to the specified filename.
void saveScreenshot(const char* filename)
{
    // Match old behavior: double resolution
    int ww = windowWidth * 2;
    int hh = windowHeight * 2;

    // Allocate with unique_ptr
    std::unique_ptr<unsigned char[]> screenshotData =
        std::make_unique<unsigned char[]>(ww * hh * 3);

    // Read pixels at doubled resolution
    glReadPixels(0, 0, ww, hh, GL_RGB, GL_UNSIGNED_BYTE, screenshotData.get());

    // Create the image
    ImageIO screenshotImg(ww, hh, 3, screenshotData.get());

    // Save as JPEG
    if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
        std::cout << "File " << filename << " saved successfully." << std::endl;
    else
        std::cout << "Failed to save file " << filename << '.' << std::endl;
}

void idleFunc()
{
    // Auto-rotate (about Y) unless toggled off
    if (autoRot)
        terrainRotate[1] += 0.7f;
    if (toggle == -1)
        autoRot = false;

    // Drive the animation state machine + shader "mode" uniform
    if (animate || takeShots)
    {
        const GLint loc = glGetUniformLocation(pipelineProgram.GetProgramHandle(), "mode");

        if (shotCount > 300)
        {
            displayOption = DISPLAY_OPTION::MESHANDSOLID;
            glUniform1i(loc, 0);
            shotCount = 0;
            animate = false;
            takeShots = false;
            autoRot = false;
        }
        else
        {
            if (shotCount <= 30) // 0–30: POINTS
            {
                autoRot = true;
                displayOption = DISPLAY_OPTION::POINTS;
                glUniform1i(loc, 0);
            }
            else if (shotCount <= 60) // 31–60: LINES
            {
                autoRot = true;
                displayOption = DISPLAY_OPTION::LINES;
                glUniform1i(loc, 0);
            }
            else if (shotCount <= 140) // 61–140: TRIANGLES with translation sweep
            {
                autoRot = false;
                displayOption = DISPLAY_OPTION::TRIANGLES;
                glUniform1i(loc, 0);

                if (shotCount <= 100) { terrainTranslate[0] += 0.4f; terrainTranslate[1] += 0.4f; terrainTranslate[2] -= 0.7f; }
                else { terrainTranslate[0] -= 0.4f; terrainTranslate[1] -= 0.4f; terrainTranslate[2] += 0.7f; }
            }
            else if (shotCount <= 220) // 141–220: SMOOTH (mode=1) with translation sweep
            {
                autoRot = false;
                displayOption = DISPLAY_OPTION::SMOOTH;
                glUniform1i(loc, 1);

                if (shotCount <= 180) { terrainTranslate[0] -= 0.4f; terrainTranslate[1] += 0.4f; terrainTranslate[2] += 0.7f; }
                else { terrainTranslate[0] += 0.4f; terrainTranslate[1] -= 0.4f; terrainTranslate[2] -= 0.7f; }
            }
            else // 221–300: MESHANDSOLID with uniform scaling pulse
            {
                displayOption = DISPLAY_OPTION::MESHANDSOLID;
                glUniform1i(loc, 0);
                autoRot = true;

                if (shotCount <= 260) { terrainScale[0] += 0.02f; terrainScale[1] += 0.02f; terrainScale[2] += 0.02f; }
                else { terrainScale[0] -= 0.02f; terrainScale[1] -= 0.02f; terrainScale[2] -= 0.02f; }
            }

            shotCount++;
        }
    }

    // Save 300 screenshots to disk when takeShots is true
    const int k = shotCount - 2;
    if ((k >= 0 && k < 300) && takeShots)
    {
        char filenum[4]; // "000".."299" + null
#if defined(_MSC_VER)
        _snprintf_s(filenum, sizeof(filenum), _TRUNCATE, "%03d", k);
#else
        std::snprintf(filenum, sizeof(filenum), "%03d", k);
#endif
        const std::string filename = std::string("Animations/color/") + filenum + ".jpg";
        saveScreenshot(filename.c_str());
    }
    else if (k >= 300)
    {
        takeShots = false;
    }

    // Ask GLUT to redraw
    glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
    glViewport(0, 0, w, h);
    
    // When the window has been resized, we need to re-set our projection matrix.
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.LoadIdentity();
    
    // You need to be careful about setting the zNear and zFar. 
    // Anything closer than zNear, or further than zFar, will be culled.
    const float zNear = 0.1f;
    const float zFar = 10000.0f;
    const float humanFieldOfView = 60.0f;
    matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
    // Mouse has moved, and one of the mouse buttons is pressed (dragging).
    
    // The change in mouse position since the last invocation of this function
    int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };
    
    switch (controlState)
    {
        // Translate the terrain
        case TRANSLATE:
            if (leftMouseButton)
            {
                // Control x,y translation via the left mouse button
                terrainTranslate[0] += mousePosDelta[0] * 0.01f;
                terrainTranslate[1] -= mousePosDelta[1] * 0.01f;
            }
            if (middleMouseButton)
            {
                // Control z translation via the middle mouse button
                terrainTranslate[2] += mousePosDelta[1] * 0.01f;
            }
            break;

        // Rotate the terrain
        case ROTATE:
            if (leftMouseButton)
            {
                // Control x,y rotation via the left mouse button
                terrainRotate[0] += mousePosDelta[1];
                terrainRotate[1] += mousePosDelta[0];
            }
            if (middleMouseButton)
            {
                // Control z rotation via the middle mouse button
                terrainRotate[2] += mousePosDelta[1];
            }
            break;

        // Scale the terrain
        case SCALE:
            if (leftMouseButton)
            {
                // Control x,y scaling via the left mouse button
                terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
                terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
            }
            if (middleMouseButton)
            {
                // Control z scaling via the middle mouse button
                terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
            }
            break;
    }
    
    // Store the new mouse position
    mousePos[0] = x;
    mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
    // Mouse has moved.
    // Store the new mouse position.
    mousePos[0] = x;
    mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
    // A mouse button has has been pressed or depressed.
    
    // Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
    switch (button)
    {
    case GLUT_LEFT_BUTTON:
        leftMouseButton = (state == GLUT_DOWN);
        break;
    case GLUT_MIDDLE_BUTTON:
        middleMouseButton = (state == GLUT_DOWN);
        break;
    case GLUT_RIGHT_BUTTON:
        rightMouseButton = (state == GLUT_DOWN);
        break;
    }
    
    // Keep track of whether CTRL and SHIFT keys are pressed.
    switch (glutGetModifiers())
    {
    case GLUT_ACTIVE_CTRL:
        controlState = TRANSLATE;
        break;
    case GLUT_ACTIVE_SHIFT:
        controlState = SCALE;
        break;

    // If CTRL and SHIFT are not pressed, we are in rotate mode.
    default:
        controlState = ROTATE;
        break;
    }
    
    // Store the new mouse position.
    mousePos[0] = x;
    mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
    // Uniform location for the 'mode' flag used by your shaders
    const GLint loc = glGetUniformLocation(pipelineProgram.GetProgramHandle(), "mode");

    switch (key)
    {
    case 27: // ESC
        std::exit(0);
        break;

    case ' ':
        std::cout << "You pressed the spacebar." << std::endl;
        break;

    case 'x':
        // Take a screenshot
        saveScreenshot("screenshot.jpg");
        break;

    case '1':
        displayOption = DISPLAY_OPTION::POINTS;
        glUniform1i(loc, 0);
        break;

    case '2':
        displayOption = DISPLAY_OPTION::LINES;
        glUniform1i(loc, 0);
        break;

    case '3':
        displayOption = DISPLAY_OPTION::TRIANGLES;
        glUniform1i(loc, 0);
        break;

    case '4':
        displayOption = DISPLAY_OPTION::SMOOTH;
        glUniform1i(loc, 1);
        break;

    case '5':
        displayOption = DISPLAY_OPTION::MESHANDSOLID;
        glUniform1i(loc, 0);
        break;

    case 't': // translate mode
        controlState = TRANSLATE;
        break;

    case 'r': // auto-rotate about Y
        autoRot = true;
        toggle = -toggle;
        break;

    case 'a': // run animation
        animate = true;
        break;

    case 's': // take 300 screenshots
        takeShots = true;
        break;
    }
}

void displayFunc()
{
    // Clear the screen.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Camera ---
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.LoadIdentity();

    // Match old dynamic LookAt based on imageW.
    int lookAtZ =
        (imageW < 200) ? 0 :
        (imageW < 500) ? 128 :
        (imageW < 700) ? 448 : 640;

    // Old eye at (128, 128, lookAtZ), look at origin, up = +Y.
    matrix.LookAt(128.0, 128.0, static_cast<float>(lookAtZ),
        0.0, 0.0, 0.0,
        0.0, 1.0, 0.0);

    // --- Model transforms (translate, rotate X/Y/Z, scale) ---
    matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
    matrix.Rotate(terrainRotate[0], 1.0, 0.0, 0.0);
    matrix.Rotate(terrainRotate[1], 0.0, 1.0, 0.0);
    matrix.Rotate(terrainRotate[2], 0.0, 0.0, 1.0);
    matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

    // Read the current modelview and projection matrices from our helper class.
    // The matrices are only read here; nothing is actually communicated to OpenGL yet.
    float modelViewMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::ModelView);
    matrix.GetMatrix(modelViewMatrix);

    float projectionMatrix[16];
    matrix.SetMatrixMode(OpenGLMatrix::Projection);
    matrix.GetMatrix(projectionMatrix);

    // Upload the modelview and projection matrices to the GPU. Note that these are "uniform" variables.
    // Important: these matrices must be uploaded to *all* pipeline programs used.
    // In hw1, there is only one pipeline program, but in hw2 there will be several of them.
    // In such a case, you must separately upload to *each* pipeline program.
    // Important: do not make a typo in the variable name below; otherwise, the program will malfunction.
    pipelineProgram.Bind(); // ensure program is active (if your wrapper requires it)
    pipelineProgram.SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
    pipelineProgram.SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);

    // --- Draw based on displayOption (mirrors old switch) ---
    switch (displayOption)
    {
    case DISPLAY_OPTION::POINTS:
    {
        vaoPoint.Bind();
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointPosition.size() / 3));
        break;
    }
    case DISPLAY_OPTION::LINES:
    {
        vaoLine.Bind();
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(linePosition.size() / 3));
        break;
    }
    case DISPLAY_OPTION::TRIANGLES:
    {
        vaoTriangle.Bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trianglePosition.size() / 3));
        break;
    }
    case DISPLAY_OPTION::SMOOTH:
    {
        // Uses vaoMode1 as in the old code.
        vaoMode1.Bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trianglePosition.size() / 3));
        break;
    }
    case DISPLAY_OPTION::MESHANDSOLID:
    {
        // Draw mesh (lines) first...
        vaoFixedColorMesh.Bind();
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(fixedColorMeshPosition.size() / 3));

        // ...then solid triangles with polygon offset (to reduce Z-fighting).
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);

        vaoTriangle.Bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trianglePosition.size() / 3));

        glDisable(GL_POLYGON_OFFSET_FILL);
        break;
    }
    }

    // Unbind any VAO.
    glBindVertexArray(0);

    // Swap buffers.
    glutSwapBuffers();
}

void readColorOverlayImage(int i, int j)
{
    blue = colorImage->getPixel(i, j, 2) / 255.0f;
    green = colorImage->getPixel(i, j, 1) / 255.0f;
    red = colorImage->getPixel(i, j, 0) / 255.0f;
}

void computeColorPixel(int i, int j)  // compute height from color and set RGB
{
    // Fetch sRGB [0..1]
    red = heightmapImage->getPixel(i, j, 0) / 255.0f;
    green = heightmapImage->getPixel(i, j, 1) / 255.0f;
    blue = heightmapImage->getPixel(i, j, 2) / 255.0f;

    // Perceptual luminance-preserving grayscale (approximate):
    const float Clinear = 0.2126f * red + 0.7152f * green + 0.0722f * blue;

    // sRGB gamma encode
    const float Csrgb = (Clinear <= 0.0031308f)
        ? 12.92f * Clinear
        : 1.055f * std::pow(Clinear, 1.0f / 2.4f) - 0.055f;

    // Height in the same scale as your grayscale path
    height = Csrgb * 255.0f * scale;
}

void readHeightFieldMode0()
{
    // Dimensions
    imageW = heightmapImage->getWidth();
    imageH = heightmapImage->getHeight();

    // Center (world origin at middle of the heightfield)
    const int imageWC = imageW / 2;
    const int imageHC = imageH / 2;

    // Scale rule (same thresholds as before)
    scale = (imageW < 200) ? 0.11f
        : (imageW < 500) ? 0.16f
        : (imageW < 700) ? 0.23f
        : 0.34f;

    // Start fresh
    pointPosition.clear();          pointColor.clear();
    linePosition.clear();           lineColor.clear();
    fixedColorMeshPosition.clear(); fixedColorMeshColor.clear();
    trianglePosition.clear();       triangleColor.clear();

    // --------------- POINTS + LINES + FIXED-COLOR MESH ----------------
    for (int i = -imageWC; i < (imageWC - 1); ++i)
    {
        for (int j = -imageHC; j < (imageHC - 1); ++j)
        {
            // -------------------- POINT MODE --------------------
            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                if (ARGC == 3) // overlay color provided
                    readColorOverlayImage(i + imageWC, j + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                // Your computeColorPixel(...) should set red/green/blue and also set 'height'
                // (matching your original behavior).
                computeColorPixel(i + imageWC, j + imageHC);
            }
            pointPosition.push_back(static_cast<float>(i));
            pointPosition.push_back(height);
            pointPosition.push_back(-static_cast<float>(j));
            pointColor.push_back(red);
            pointColor.push_back(green);
            pointColor.push_back(blue);
            pointColor.push_back(alpha);

            // -------------------- LINE/WIREFRAME MODE --------------------
            // Horizontal segment (i, j) -> (i+1, j)
            if (i < (imageWC - 2))
            {
                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                    if (ARGC == 3)
                        readColorOverlayImage(i + imageWC, j + imageHC);
                    else
                        red = green = blue = height / 255.0f;
                    height *= scale;
                }
                else
                {
                    computeColorPixel(i + imageWC, j + imageHC);
                }
                linePosition.push_back(static_cast<float>(i));
                linePosition.push_back(height);
                linePosition.push_back(-static_cast<float>(j));
                lineColor.push_back(red);
                lineColor.push_back(green);
                lineColor.push_back(blue);
                lineColor.push_back(alpha);

                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel((i + 1) + imageWC, j + imageHC, 0);
                    if (ARGC == 3)
                        readColorOverlayImage((i + 1) + imageWC, j + imageHC);
                    else
                        red = green = blue = height / 255.0f;
                    height *= scale;
                }
                else
                {
                    computeColorPixel((i + 1) + imageWC, j + imageHC);
                }
                linePosition.push_back(static_cast<float>(i + 1));
                linePosition.push_back(height);
                linePosition.push_back(-static_cast<float>(j));
                lineColor.push_back(red);
                lineColor.push_back(green);
                lineColor.push_back(blue);
                lineColor.push_back(alpha);
            }

            // Vertical segment (i, j) -> (i, j+1)
            if (j < (imageHC - 2))
            {
                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                    if (ARGC == 3)
                        readColorOverlayImage(i + imageWC, j + imageHC);
                    else
                        red = green = blue = height / 255.0f;
                    height *= scale;
                }
                else
                {
                    computeColorPixel(i + imageWC, j + imageHC);
                }
                linePosition.push_back(static_cast<float>(i));
                linePosition.push_back(height);
                linePosition.push_back(-static_cast<float>(j));
                lineColor.push_back(red);
                lineColor.push_back(green);
                lineColor.push_back(blue);
                lineColor.push_back(alpha);

                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, (j + 1) + imageHC, 0);
                    if (ARGC == 3)
                        readColorOverlayImage(i + imageWC, (j + 1) + imageHC);
                    else
                        red = green = blue = height / 255.0f;
                    height *= scale;
                }
                else
                {
                    computeColorPixel(i + imageWC, (j + 1) + imageHC);
                }
                linePosition.push_back(static_cast<float>(i));
                linePosition.push_back(height);
                linePosition.push_back(-static_cast<float>(j + 1));
                lineColor.push_back(red);
                lineColor.push_back(green);
                lineColor.push_back(blue);
                lineColor.push_back(alpha);
            }

            // -------------------- FIXED-COLOR MESH (for MESHANDSOLID) --------------------
            // Horizontal fixed-color segment (i, j) -> (i+1, j)
            if (i < (imageWC - 2))
            {
                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                    height *= scale;
                }
                else
                {
                    // Maintain original behavior: computeColorPixel may set 'height'.
                    computeColorPixel(i + imageWC, j + imageHC);
                }
                red = 0.1f; green = 0.0f; blue = 0.2f;
                fixedColorMeshPosition.push_back(static_cast<float>(i));
                fixedColorMeshPosition.push_back(height);
                fixedColorMeshPosition.push_back(-static_cast<float>(j));
                fixedColorMeshColor.push_back(red);
                fixedColorMeshColor.push_back(green);
                fixedColorMeshColor.push_back(blue);
                fixedColorMeshColor.push_back(alpha);

                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel((i + 1) + imageWC, j + imageHC, 0);
                    height *= scale;
                }
                else
                {
                    computeColorPixel((i + 1) + imageWC, j + imageHC);
                }
                red = 0.1f; green = 0.0f; blue = 0.2f;
                fixedColorMeshPosition.push_back(static_cast<float>(i + 1));
                fixedColorMeshPosition.push_back(height);
                fixedColorMeshPosition.push_back(-static_cast<float>(j));
                fixedColorMeshColor.push_back(red);
                fixedColorMeshColor.push_back(green);
                fixedColorMeshColor.push_back(blue);
                fixedColorMeshColor.push_back(alpha);
            }

            // Vertical fixed-color segment (i, j) -> (i, j+1)
            if (j < (imageHC - 2))
            {
                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                    height *= scale;
                }
                else
                {
                    computeColorPixel(i + imageWC, j + imageHC);
                }
                red = 0.1f; green = 0.0f; blue = 0.2f;
                fixedColorMeshPosition.push_back(static_cast<float>(i));
                fixedColorMeshPosition.push_back(height);
                fixedColorMeshPosition.push_back(-static_cast<float>(j));
                fixedColorMeshColor.push_back(red);
                fixedColorMeshColor.push_back(green);
                fixedColorMeshColor.push_back(blue);
                fixedColorMeshColor.push_back(alpha);

                if (colorState == GRAYSCALE)
                {
                    height = heightmapImage->getPixel(i + imageWC, (j + 1) + imageHC, 0);
                    height *= scale;
                }
                else
                {
                    computeColorPixel(i + imageWC, (j + 1) + imageHC);
                }
                red = 0.1f; green = 0.0f; blue = 0.2f;
                fixedColorMeshPosition.push_back(static_cast<float>(i));
                fixedColorMeshPosition.push_back(height);
                fixedColorMeshPosition.push_back(-static_cast<float>(j + 1));
                fixedColorMeshColor.push_back(red);
                fixedColorMeshColor.push_back(green);
                fixedColorMeshColor.push_back(blue);
                fixedColorMeshColor.push_back(alpha);
            }
        }
    }

    // ------------------------- TRIANGLE MODE -------------------------
    for (int i = -imageWC; i < (imageWC - 1); ++i)
    {
        for (int j = -imageHC; j < (imageHC - 1); ++j)
        {
            //  /|   first tri: (i,j), (i+1,j), (i+1,j+1)
            // / |
            // ---
            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage(i + imageWC, j + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel(i + imageWC, j + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);

            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel((i + 1) + imageWC, j + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage((i + 1) + imageWC, j + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel((i + 1) + imageWC, j + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i + 1));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);

            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel((i + 1) + imageWC, (j + 1) + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage((i + 1) + imageWC, (j + 1) + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel((i + 1) + imageWC, (j + 1) + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i + 1));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j + 1));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);

            // --- second tri: (i,j), (i+1,j+1), (i, j+1) ---
            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel(i + imageWC, j + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage(i + imageWC, j + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel(i + imageWC, j + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);

            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel((i + 1) + imageWC, (j + 1) + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage((i + 1) + imageWC, (j + 1) + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel((i + 1) + imageWC, (j + 1) + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i + 1));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j + 1));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);

            if (colorState == GRAYSCALE)
            {
                height = heightmapImage->getPixel(i + imageWC, (j + 1) + imageHC, 0);
                if (ARGC == 3)
                    readColorOverlayImage(i + imageWC, (j + 1) + imageHC);
                else
                    red = green = blue = height / 255.0f;
                height *= scale;
            }
            else
            {
                computeColorPixel(i + imageWC, (j + 1) + imageHC);
            }
            trianglePosition.push_back(static_cast<float>(i));
            trianglePosition.push_back(height);
            trianglePosition.push_back(-static_cast<float>(j + 1));
            triangleColor.push_back(red);
            triangleColor.push_back(green);
            triangleColor.push_back(blue);
            triangleColor.push_back(alpha);
        }
    }
}

void readHeightFieldMode1() // fills triPleft/triPright/triPdown/triPup from heightmap
{
    // Dimensions from the height map
    imageW = heightmapImage->getWidth();
    imageH = heightmapImage->getHeight();

    // Center the grid at world origin
    const int imageWC = imageW / 2;
    const int imageHC = imageH / 2;

    // Scale rule matching the old logic
    scale = (imageW < 200) ? 0.11f
        : (imageW < 500) ? 0.16f
        : (imageW < 700) ? 0.23f
        : 0.34f;

    // We’ll write fresh data
    triPleftPosition.clear();
    triPrightPosition.clear();
    triPdownPosition.clear();
    triPupPosition.clear();

    // Optional: mild pre-reserve to avoid many reallocations (heuristic).
    // Each (i,j) iteration pushes 4 * (3 coords) many times; exact count is large.
    // triPleftPosition.reserve(imageW * imageH * 6);
    // triPrightPosition.reserve(imageW * imageH * 6);
    // triPdownPosition.reserve(imageW * imageH * 6);
    // triPupPosition.reserve(imageW * imageH * 6);

    for (int i = -imageWC; i < (imageWC - 1); ++i)
    {
        for (int j = -imageHC; j < (imageHC - 1); ++j)
        {
            // Helper lambda to fetch height (grayscale channel 0) with current scale.
            auto H = [&](int x, int y) -> float
                {
                    return static_cast<float>(scale * heightmapImage->getPixel(x + imageWC, y + imageHC, 0));
                };

            float height;

            // --- Upper-left tri fan around p(i,j) ---
            // Pleft: (i-1, j) or center if left edge
            height = (i == -imageWC) ? H(i, j) : H(i - 1, j);
            triPleftPosition.push_back(static_cast<float>(i - 1));
            triPleftPosition.push_back(height);
            triPleftPosition.push_back(-static_cast<float>(j));

            // Pright: (i+1, j)
            height = H(i + 1, j);
            triPrightPosition.push_back(static_cast<float>(i + 1));
            triPrightPosition.push_back(height);
            triPrightPosition.push_back(-static_cast<float>(j));

            // Pdown: (i, j-1) or center if bottom edge
            height = (j == -imageHC) ? H(i, j) : H(i, j - 1);
            triPdownPosition.push_back(static_cast<float>(i));
            triPdownPosition.push_back(height);
            triPdownPosition.push_back(-static_cast<float>(j - 1));

            // Pup: (i, j+1)
            height = H(i, j + 1);
            triPupPosition.push_back(static_cast<float>(i));
            triPupPosition.push_back(height);
            triPupPosition.push_back(-static_cast<float>(j + 1));

            // --- Upper-right tri fan around p(i+1, j) ---
            // Pleft: (i, j)
            height = H(i, j);
            triPleftPosition.push_back(static_cast<float>(i));
            triPleftPosition.push_back(height);
            triPleftPosition.push_back(-static_cast<float>(j));

            // Pright: (i+2, j) or center if right edge-1
            height = (i == imageWC - 2) ? H(i + 1, j) : H(i + 2, j);
            triPrightPosition.push_back(static_cast<float>(i + 2));
            triPrightPosition.push_back(height);
            triPrightPosition.push_back(-static_cast<float>(j));

            // Pdown: (i+1, j-1) or center if bottom edge
            height = (j == -imageHC) ? H(i + 1, j) : H(i + 1, j - 1);
            triPdownPosition.push_back(static_cast<float>(i + 1));
            triPdownPosition.push_back(height);
            triPdownPosition.push_back(-static_cast<float>(j - 1));

            // Pup: (i+1, j+1)
            height = H(i + 1, j + 1);
            triPupPosition.push_back(static_cast<float>(i + 1));
            triPupPosition.push_back(height);
            triPupPosition.push_back(-static_cast<float>(j + 1));

            // --- Lower-right tri fan around p(i+1, j+1) ---
            // Pleft: (i, j+1)
            height = H(i, j + 1);
            triPleftPosition.push_back(static_cast<float>(i));
            triPleftPosition.push_back(height);
            triPleftPosition.push_back(-static_cast<float>(j + 1));

            // Pright: (i+2, j+1) or center if right edge-1
            height = (i == imageWC - 2) ? H(i + 1, j + 1) : H(i + 2, j + 1);
            triPrightPosition.push_back(static_cast<float>(i + 2));
            triPrightPosition.push_back(height);
            triPrightPosition.push_back(-static_cast<float>(j + 1));

            // Pdown: (i+1, j)
            height = H(i + 1, j);
            triPdownPosition.push_back(static_cast<float>(i + 1));
            triPdownPosition.push_back(height);
            triPdownPosition.push_back(-static_cast<float>(j));

            // Pup: (i+1, j+2) or center if top edge-1
            height = (j == imageHC - 2) ? H(i + 1, j + 1) : H(i + 1, j + 2);
            triPupPosition.push_back(static_cast<float>(i + 1));
            triPupPosition.push_back(height);
            triPupPosition.push_back(-static_cast<float>(j + 2));

            // --- Lower-left tri fan around p(i, j) ---
            // Pleft: (i-1, j) or center if left edge
            height = (i == -imageWC) ? H(i, j) : H(i - 1, j);
            triPleftPosition.push_back(static_cast<float>(i - 1));
            triPleftPosition.push_back(height);
            triPleftPosition.push_back(-static_cast<float>(j));

            // Pright: (i+1, j)
            height = H(i + 1, j);
            triPrightPosition.push_back(static_cast<float>(i + 1));
            triPrightPosition.push_back(height);
            triPrightPosition.push_back(-static_cast<float>(j));

            // Pdown: (i, j-1) or center if bottom edge
            height = (j == -imageHC) ? H(i, j) : H(i, j - 1);
            triPdownPosition.push_back(static_cast<float>(i));
            triPdownPosition.push_back(height);
            triPdownPosition.push_back(-static_cast<float>(j - 1));

            // Pup: (i, j+1)
            height = H(i, j + 1);
            triPupPosition.push_back(static_cast<float>(i));
            triPupPosition.push_back(height);
            triPupPosition.push_back(-static_cast<float>(j + 1));

            // --- Lower-right tri fan around p(i+1, j+1) (second time per original) ---
            // Pleft: (i, j+1)
            height = H(i, j + 1);
            triPleftPosition.push_back(static_cast<float>(i));
            triPleftPosition.push_back(height);
            triPleftPosition.push_back(-static_cast<float>(j + 1));

            // Pright: (i+2, j+1) or center if right edge-1
            height = (i == imageWC - 2) ? H(i + 1, j + 1) : H(i + 2, j + 1);
            triPrightPosition.push_back(static_cast<float>(i + 2));
            triPrightPosition.push_back(height);
            triPrightPosition.push_back(-static_cast<float>(j + 1));

            // Pdown: (i+1, j)
            height = H(i + 1, j);
            triPdownPosition.push_back(static_cast<float>(i + 1));
            triPdownPosition.push_back(height);
            triPdownPosition.push_back(-static_cast<float>(j));

            // Pup: (i+1, j+2) or center if top edge-1
            height = (j == imageHC - 2) ? H(i + 1, j + 1) : H(i + 1, j + 2);
            triPupPosition.push_back(static_cast<float>(i + 1));
            triPupPosition.push_back(height);
            triPupPosition.push_back(-static_cast<float>(j + 2));
        }
    }
}

static inline void ConnectIfPresent(
    VAO& vao, PipelineProgram* prog, VBO& vbo, const char* attrName)
{
    const GLint loc = glGetAttribLocation(prog->GetProgramHandle(), attrName);
    if (loc < 0)
    {
        std::cout << "[init] Skipping missing vertex attribute '" << attrName << "'\n";
        return;
    }
    vao.ConnectPipelineProgramAndVBOAndShaderVariable(prog, &vbo, attrName);
}

void initVBO_VAO_mode0(PipelineProgram* pipelineProgram)
{
    // Points
    vboPointPos.Gen(static_cast<int>(pointPosition.size() / 3), 3, pointPosition.data(), GL_STATIC_DRAW);
    vboPointCol.Gen(static_cast<int>(pointColor.size() / 4), 4, pointColor.data(), GL_STATIC_DRAW);

    vaoPoint.Gen();
    ConnectIfPresent(vaoPoint, pipelineProgram, vboPointPos, "position");
    ConnectIfPresent(vaoPoint, pipelineProgram, vboPointCol, "color");

    // Lines
    vboLinePos.Gen(static_cast<int>(linePosition.size() / 3), 3, linePosition.data(), GL_STATIC_DRAW);
    vboLineCol.Gen(static_cast<int>(lineColor.size() / 4), 4, lineColor.data(), GL_STATIC_DRAW);

    vaoLine.Gen();
    ConnectIfPresent(vaoLine, pipelineProgram, vboLinePos, "position");
    ConnectIfPresent(vaoLine, pipelineProgram, vboLineCol, "color");

    // Fixed-color mesh
    vboMeshPos.Gen(static_cast<int>(fixedColorMeshPosition.size() / 3), 3, fixedColorMeshPosition.data(), GL_STATIC_DRAW);
    vboMeshCol.Gen(static_cast<int>(fixedColorMeshColor.size() / 4), 4, fixedColorMeshColor.data(), GL_STATIC_DRAW);

    vaoFixedColorMesh.Gen();
    ConnectIfPresent(vaoFixedColorMesh, pipelineProgram, vboMeshPos, "position");
    ConnectIfPresent(vaoFixedColorMesh, pipelineProgram, vboMeshCol, "color");

    // Triangles
    vboTriPos.Gen(static_cast<int>(trianglePosition.size() / 3), 3, trianglePosition.data(), GL_STATIC_DRAW);
    vboTriCol.Gen(static_cast<int>(triangleColor.size() / 4), 4, triangleColor.data(), GL_STATIC_DRAW);

    vaoTriangle.Gen();
    ConnectIfPresent(vaoTriangle, pipelineProgram, vboTriPos, "position");
    ConnectIfPresent(vaoTriangle, pipelineProgram, vboTriCol, "color");
}

void initVBO_VAO_mode1(PipelineProgram* pipelineProgram)
{
    // Generate VBOs for the four neighbor position streams.
    vboTriPleft.Gen(static_cast<int>(triPleftPosition.size() / 3), 3, triPleftPosition.data(), GL_STATIC_DRAW);
    vboTriPright.Gen(static_cast<int>(triPrightPosition.size() / 3), 3, triPrightPosition.data(), GL_STATIC_DRAW);
    vboTriPdown.Gen(static_cast<int>(triPdownPosition.size() / 3), 3, triPdownPosition.data(), GL_STATIC_DRAW);
    vboTriPup.Gen(static_cast<int>(triPupPosition.size() / 3), 3, triPupPosition.data(), GL_STATIC_DRAW);

    // Main triangle streams used in this mode.
    vboTriPos_m1.Gen(static_cast<int>(trianglePosition.size() / 3), 3, trianglePosition.data(), GL_STATIC_DRAW);
    vboTriCol_m1.Gen(static_cast<int>(triangleColor.size() / 4), 4, triangleColor.data(), GL_STATIC_DRAW);

    // Create VAO for mode 1.
    vaoMode1.Gen();

    // Helper: only connect an attribute if it's actually present in the shader.
    auto ConnectIfPresent = [&](VAO& vao, VBO& vbo, const char* attrName)
        {
            const GLint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), attrName);
            if (loc < 0)
            {
                std::cout << "[init] Skipping missing vertex attribute '" << attrName << "'\n";
                return;
            }
            vao.ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, &vbo, attrName);
        };

    // Neighbor attributes (used by SMOOTH mode shaders).
    ConnectIfPresent(vaoMode1, vboTriPleft, "PleftPosition");
    ConnectIfPresent(vaoMode1, vboTriPright, "PrightPosition");
    ConnectIfPresent(vaoMode1, vboTriPdown, "PdownPosition");
    ConnectIfPresent(vaoMode1, vboTriPup, "PupPosition");

    // Base attributes (position/color) also used in this mode.
    ConnectIfPresent(vaoMode1, vboTriPos_m1, "position");
    ConnectIfPresent(vaoMode1, vboTriCol_m1, "color");
}

void initScene(int argc, char* argv[])
{
    // --- Load heightmap ---
    // NOTE: keep these as your existing globals (as used by read* functions).
    heightmapImage = new ImageIO();
    if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
    {
        std::cout << "Error reading image " << argv[1] << "." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Determine grayscale vs color
    bytesPerPixel = heightmapImage->getBytesPerPixel();
    colorState = (bytesPerPixel == 1) ? GRAYSCALE : COLOR;

    // Optional overlay color image (argc==3)
    if (argc == 3)
    {
        colorImage = new ImageIO();
        if (colorImage->loadJPEG(argv[2]) != ImageIO::OK)
        {
            std::cout << "Error reading image " << argv[2] << "." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // Background color (match old project)
    glClearColor(0.08f, 0.0f, 0.08f, 0.0f);

    // Z-buffer
    glEnable(GL_DEPTH_TEST);

    // --- Build & bind the pipeline program (object form in the new template) ---
    if (pipelineProgram.BuildShadersFromFiles(shaderBasePath, "vertexShader.glsl", "fragmentShader.glsl") != 0)
    {
        std::cout << "Failed to build the pipeline program." << std::endl;
        throw 1;
    }
    pipelineProgram.Bind();
    std::cout << "Successfully built the pipeline program." << std::endl;

    // --- Generate geometry from the images (fills your std::vectors) ---
    // These are your existing functions; we already updated them earlier.
    readHeightFieldMode0(); // fills point/line/fixedColorMesh/triangle arrays
    readHeightFieldMode1(); // fills triPleft/triPright/triPdown/triPup arrays

    // --- Create/wire VAOs & VBOs using the helpers (new template style) ---
    // Use the helper-based init functions we rewrote (one-attribute-per-VBO).
    // Their signatures should take a PipelineProgram*.
    initVBO_VAO_mode0(&pipelineProgram);
    initVBO_VAO_mode1(&pipelineProgram);

    // Diagnostics
    std::cout << "GL error: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
    if (!((argc != 3) ^ (argc != 2)))
    {
        cout << "The arguments are incorrect." << endl;
        cout << "Usage: ./hw1 <heightmap file>" << endl;
        cout << "Ex: ./hw1 heightmap/spiral.jpg" << endl << endl;
        cout << "You can additionally provide color image to color the heightmap." << endl;
        cout << "Usage: ./hw1 <heightmap file> <color image file>" << endl;
        cout << "Ex: ./hw1 heightmap/Heightmap.jpg heightmap/HeightmapColor.jpg" << endl;
        exit(EXIT_FAILURE);
    }

    ARGC = argc;
    
    cout << "Initializing GLUT..." << endl;
    glutInit(&argc,argv);
    
    cout << "Initializing OpenGL..." << endl;

    #ifdef __APPLE__
        glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    #else
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    #endif
    
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(0, 0);  
    glutCreateWindow(windowTitle);
    
    cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
    cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

    #ifdef __APPLE__
        // This is needed on recent Mac OS X versions to correctly display the window.
        glutReshapeWindow(windowWidth - 1, windowHeight - 1);
    #endif
    
    // Tells GLUT to use a particular display function to redraw.
    glutDisplayFunc(displayFunc);
    // Perform animation inside idleFunc.
    glutIdleFunc(idleFunc);
    // callback for mouse drags
    glutMotionFunc(mouseMotionDragFunc);
    // callback for idle mouse movement
    glutPassiveMotionFunc(mouseMotionFunc);
    // callback for mouse button changes
    glutMouseFunc(mouseButtonFunc);
    // callback for resizing the window
    glutReshapeFunc(reshapeFunc);
    // callback for pressing the keys on the keyboard
    glutKeyboardFunc(keyboardFunc);
    
    // init glew
    #ifdef __APPLE__
        // nothing is needed on Apple
    #else
        // Windows, Linux
        GLint result = glewInit();
        if (result != GLEW_OK)
        {
            cout << "error: " << glewGetErrorString(result) << endl;
            exit(EXIT_FAILURE);
        }
    #endif
        
    // Perform the initialization.
    initScene(argc, argv);
    
    // Sink forever into the GLUT loop.
    glutMainLoop();
}
