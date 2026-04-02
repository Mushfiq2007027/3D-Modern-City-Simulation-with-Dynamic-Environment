//
//  main.cpp  –  3D Smart City Environment Simulation  (v4 – corrected scale & proportions)
//  CSE 4208 - Computer Graphics
//
//  Scale: ~1 world unit ≈ 3 m
//  Buildings  ≈ 4× previous  |  World layout ≈ 3× wider  |  Vehicles unchanged
//  Sun/Moon brought close enough to be visible in default camera view
//

#define _CRT_SECURE_NO_DEPRECATE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "pointLight.h"
#include "directionalLight.h"
#include "spotLight.h"

#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

// ────────────────────────────────────────────────────────────
//  Prototypes
// ────────────────────────────────────────────────────────────
void framebuffer_size_callback(GLFWwindow*, int, int);
void key_callback(GLFWwindow*, int, int, int, int);
void scroll_callback(GLFWwindow*, double, double);
void processInput(GLFWwindow*);
unsigned int createCubeVAO();
void buildCylinder(int);
void buildSphere(int, int);

void drawCube(unsigned int, Shader&, glm::mat4, glm::vec3 pos,
    glm::vec3 sc, glm::vec3 col, float rotY = 0);
void drawCylinder(Shader&, glm::mat4, glm::vec3, float r, float h, glm::vec3);
void drawSphere(Shader&, glm::mat4, glm::vec3, float r, glm::vec3, bool em = false);

glm::mat4 myPerspective(float, float, float, float);
glm::mat4 myRotate(glm::mat4, float, glm::vec3);
glm::vec3 bezier3(glm::vec3, glm::vec3, glm::vec3, glm::vec3, float);
float bridgeDeckY(float x);

// ────────────────────────────────────────────────────────────
//  Constants  (all distances in world units; 1 unit ≈ 3 m)
// ────────────────────────────────────────────────────────────
const unsigned int SCR_W = 1600, SCR_H = 900;
const float PI = 3.14159265358979f;

const float HWY_Y = 14.0f;   // highway deck elevation
const float HWY_HW = 9.0f;   // highway half-width in Z
const float RIV_HW = 20.0f;   // river half-width in X
const float SCENE_X = 150.0f;  // scene half-extent in X
const float C1Z = 65.0f;  // city-1 building centre Z
const float C2Z = -65.0f;  // city-2 building centre Z

// Bridge deck Bezier control points
const glm::vec3 BP0(-20, HWY_Y, 0);
const glm::vec3 BP1(-7, HWY_Y + 13.0f, 0);
const glm::vec3 BP2(7, HWY_Y + 13.0f, 0);
const glm::vec3 BP3(20, HWY_Y, 0);

// ────────────────────────────────────────────────────────────
//  Camera   (bird's-eye, ~38° down)
// ────────────────────────────────────────────────────────────
Camera camera(glm::vec3(0.0f, 145.0f, 195.0f), glm::vec3(0, 0, 0));

// ────────────────────────────────────────────────────────────
//  Lighting
// ────────────────────────────────────────────────────────────
DirectionalLight dirLight;
SpotLight spotLight;
PointLight pointLights[8];
int numPL = 8;
bool dirLightOn = true, pointLightsOn = true, spotLightOn = true;
bool ambientOn = true, diffuseOn = true, specularOn = true;
bool isNight = false;

// ────────────────────────────────────────────────────────────
//  Timing / state
// ────────────────────────────────────────────────────────────
float deltaTime = 0, lastFrame = 0, gTime = 0;
bool trafficGreen = true;
bool splitViewport = false;

// ────────────────────────────────────────────────────────────
//  Geometry IDs
// ────────────────────────────────────────────────────────────
unsigned int cylVAO, cylVBO, cylEBO; int cylIC;
unsigned int sphVAO, sphVBO, sphEBO; int sphIC;

// ────────────────────────────────────────────────────────────
//  Vehicles / Boats
// ────────────────────────────────────────────────────────────
struct Vehicle { float x; int type; float speed; int lane; int dir; };
struct Boat { float z; float speed; int dir; float xOff; };
vector<Vehicle> vehicles;
vector<Boat>    boats;

// ════════════════════════════════════════════════════════════
//  Custom math
// ════════════════════════════════════════════════════════════
glm::mat4 myPerspective(float fov, float asp, float n, float f)
{
    float t = tanf(fov * 0.5f);
    glm::mat4 m(0);
    m[0][0] = 1.0f / (asp * t);
    m[1][1] = 1.0f / t;
    m[2][2] = -(f + n) / (f - n);
    m[2][3] = -1.0f;
    m[3][2] = -2.0f * f * n / (f - n);
    return m;
}
glm::mat4 myRotate(glm::mat4 m, float a, glm::vec3 ax)
{
    ax = glm::normalize(ax);
    float c = cosf(a), s = sinf(a), t = 1 - c;
    float x = ax.x, y = ax.y, z = ax.z;
    glm::mat4 r(1);
    r[0][0] = t * x * x + c;   r[1][0] = t * x * y - s * z; r[2][0] = t * x * z + s * y;
    r[0][1] = t * x * y + s * z; r[1][1] = t * y * y + c;   r[2][1] = t * y * z - s * x;
    r[0][2] = t * x * z - s * y; r[1][2] = t * y * z + s * x; r[2][2] = t * z * z + c;
    return m * r;
}
glm::vec3 bezier3(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, float t)
{
    float u = 1 - t;
    return u * u * u * a + 3 * u * u * t * b + 3 * u * t * t * c + t * t * t * d;
}
float bridgeDeckY(float x)
{
    if (x < -RIV_HW || x > RIV_HW) return HWY_Y;
    float t = (x + RIV_HW) / (2.0f * RIV_HW);
    return bezier3(BP0, BP1, BP2, BP3, t).y;
}

// ════════════════════════════════════════════════════════════
//  Geometry builders
// ════════════════════════════════════════════════════════════
unsigned int createCubeVAO()
{
    float v[] = {
        -.5f,-.5f,-.5f, 0,0,-1,  .5f,-.5f,-.5f, 0,0,-1,  .5f,.5f,-.5f, 0,0,-1, -.5f,.5f,-.5f, 0,0,-1,
         .5f,-.5f,-.5f, 1,0, 0,  .5f,.5f,-.5f,  1,0, 0,  .5f,-.5f,.5f, 1,0, 0,  .5f,.5f,.5f,  1,0, 0,
        -.5f,-.5f, .5f, 0,0, 1,  .5f,-.5f,.5f,  0,0, 1,  .5f,.5f,.5f,  0,0, 1, -.5f,.5f,.5f,  0,0, 1,
        -.5f,-.5f, .5f,-1,0, 0, -.5f,.5f,.5f,  -1,0, 0, -.5f,.5f,-.5f,-1,0, 0, -.5f,-.5f,-.5f,-1,0,0,
         .5f,.5f,  .5f, 0,1, 0,  .5f,.5f,-.5f,  0,1, 0, -.5f,.5f,-.5f, 0,1, 0, -.5f,.5f,.5f,  0,1, 0,
        -.5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,.5f, 0,-1,0, -.5f,-.5f,.5f, 0,-1,0
    };
    unsigned int i[] = { 0,3,2,2,1,0, 4,5,7,7,6,4, 8,9,10,10,11,8,
                      12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20 };
    unsigned int V, B, E;
    glGenVertexArrays(1, &V); glGenBuffers(1, &B); glGenBuffers(1, &E);
    glBindVertexArray(V);
    glBindBuffer(GL_ARRAY_BUFFER, B); glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, E); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12); glEnableVertexAttribArray(1);
    glBindVertexArray(0); return V;
}
void buildCylinder(int seg)
{
    vector<float> vt; vector<unsigned int> id; float d = 2 * PI / seg;
    for (int i = 0; i <= seg; i++) {
        float a = i * d, x = cosf(a), z = sinf(a);
        vt.insert(vt.end(), { x,0,z, x,0,z,  x,1,z, x,0,z });
    }
    for (int i = 0; i < seg; i++) {
        int b = i * 2, n = b + 2;
        id.insert(id.end(), { (unsigned)b,(unsigned)n,(unsigned)(b + 1),
                             (unsigned)(b + 1),(unsigned)n,(unsigned)(n + 1) });
    }
    cylIC = (int)id.size();
    glGenVertexArrays(1, &cylVAO); glGenBuffers(1, &cylVBO); glGenBuffers(1, &cylEBO);
    glBindVertexArray(cylVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cylVBO); glBufferData(GL_ARRAY_BUFFER, vt.size() * 4, vt.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, id.size() * 4, id.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
void buildSphere(int rings, int sectors)
{
    vector<float> vt; vector<unsigned int> id;
    for (int r = 0; r <= rings; r++) for (int s = 0; s <= sectors; s++) {
        float y = sinf(-PI / 2 + PI * r / (float)rings);
        float x = cosf(2 * PI * s / (float)sectors) * sinf(PI * r / (float)rings);
        float z = sinf(2 * PI * s / (float)sectors) * sinf(PI * r / (float)rings);
        vt.insert(vt.end(), { x,y,z, x,y,z });
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < sectors; s++) {
        int c = r * (sectors + 1) + s, n = c + sectors + 1;
        id.insert(id.end(), { (unsigned)c,(unsigned)n,(unsigned)(c + 1),
                             (unsigned)(c + 1),(unsigned)n,(unsigned)(n + 1) });
    }
    sphIC = (int)id.size();
    glGenVertexArrays(1, &sphVAO); glGenBuffers(1, &sphVBO); glGenBuffers(1, &sphEBO);
    glBindVertexArray(sphVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphVBO); glBufferData(GL_ARRAY_BUFFER, vt.size() * 4, vt.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, id.size() * 4, id.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ════════════════════════════════════════════════════════════
//  Drawing helpers
// ════════════════════════════════════════════════════════════
void drawCube(unsigned int vao, Shader& s, glm::mat4 P,
    glm::vec3 pos, glm::vec3 sc, glm::vec3 col, float rotY)
{
    glm::mat4 m = glm::translate(P, pos);
    if (rotY != 0) m = myRotate(m, glm::radians(rotY), glm::vec3(0, 1, 0));
    m = glm::scale(m, sc);
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.25f);
    s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.3f));
    s.setFloat("material.shininess", 32.0f);
    s.setBool("isEmissive", false);
    s.setBool("isWater", false);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}
void drawCylinder(Shader& s, glm::mat4 P, glm::vec3 pos, float r, float h, glm::vec3 col)
{
    glm::mat4 m = glm::translate(P, pos);
    m = glm::scale(m, glm::vec3(r, h, r));
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.25f); s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.2f)); s.setFloat("material.shininess", 16);
    s.setBool("isEmissive", false); s.setBool("isWater", false);
    glBindVertexArray(cylVAO); glDrawElements(GL_TRIANGLES, cylIC, GL_UNSIGNED_INT, 0);
}
void drawSphere(Shader& s, glm::mat4 P, glm::vec3 pos, float r, glm::vec3 col, bool em)
{
    glm::mat4 m = glm::translate(P, pos);
    m = glm::scale(m, glm::vec3(r));
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.3f); s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.4f)); s.setFloat("material.shininess", 64);
    s.setBool("isEmissive", em); s.setBool("isWater", false);
    glBindVertexArray(sphVAO); glDrawElements(GL_TRIANGLES, sphIC, GL_UNSIGNED_INT, 0);
}

// Window grid helper
void drawWindowGrid(unsigned int vao, Shader& s, glm::mat4 B, glm::vec3 bldg,
    int axis, float face, float yLo, float yHi,
    float uLo, float uHi, float yStep, float uStep)
{
    glm::vec3 wc(0.52f, 0.72f, 0.88f);
    for (float y = yLo; y < yHi; y += yStep)
        for (float u = uLo; u <= uHi + 0.01f; u += uStep) {
            glm::vec3 p = bldg;
            if (axis == 0) {
                p.x += face; p.y += y; p.z += u;
                drawCube(vao, s, B, p, glm::vec3(0.12f, 1.3f, 0.9f), wc);
            }
            else {
                p.z += face; p.y += y; p.x += u;
                drawCube(vao, s, B, p, glm::vec3(0.9f, 1.3f, 0.12f), wc);
            }
        }
}

// ════════════════════════════════════════════════════════════
//  Small scene objects
// ════════════════════════════════════════════════════════════

// Tree: trunk + canopy sphere
void drawTree(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float canopyR = 2.5f)
{
    drawCylinder(s, B, p, 0.3f, 5.0f, glm::vec3(0.42f, 0.26f, 0.12f));
    drawSphere(s, B, p + glm::vec3(0, 5.0f + canopyR * 0.75f, 0), canopyR, glm::vec3(0.14f, 0.52f, 0.18f));
}

// Street-light (tall pole + horizontal arm + lamp head)
void drawStreetLight(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    drawCylinder(s, B, p, 0.14f, 14.0f, glm::vec3(0.30f, 0.30f, 0.30f));
    drawCube(v, s, B, p + glm::vec3(1.5f, 14.0f, 0), glm::vec3(3.2f, 0.15f, 0.15f), glm::vec3(0.30f));
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(3.0f, 13.7f, 0), glm::vec3(1.4f, 0.45f, 1.4f),
        isNight ? glm::vec3(1, 0.93f, 0.65f) : glm::vec3(0.5f));
    s.setBool("isEmissive", false);
}

// Traffic light (pole + signal box + 3 lenses)
void drawTrafficLight(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    drawCylinder(s, B, p, 0.15f, 9.0f, glm::vec3(0.22f));
    drawCube(v, s, B, p + glm::vec3(0, 9.6f, 0), glm::vec3(0.7f, 2.2f, 0.7f), glm::vec3(0.12f));
    s.setBool("isEmissive", true);
    glm::vec3 rc = trafficGreen ? glm::vec3(0.25f, 0, 0) : glm::vec3(1, 0.05f, 0.05f);
    glm::vec3 yc = (!trafficGreen) ? glm::vec3(0.95f, 0.85f, 0.1f) : glm::vec3(0.3f, 0.3f, 0);
    glm::vec3 gc = trafficGreen ? glm::vec3(0.05f, 1, 0.05f) : glm::vec3(0, 0.25f, 0);
    drawCube(v, s, B, p + glm::vec3(0, 10.35f, 0.36f), glm::vec3(0.36f, 0.36f, 0.08f), rc);
    drawCube(v, s, B, p + glm::vec3(0, 9.60f, 0.36f), glm::vec3(0.36f, 0.36f, 0.08f), yc);
    drawCube(v, s, B, p + glm::vec3(0, 8.85f, 0.36f), glm::vec3(0.36f, 0.36f, 0.08f), gc);
    s.setBool("isEmissive", false);
}

// Billboard (two poles + emissive panel)
void drawBillboard(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, glm::vec3 col, float rotY = 0)
{
    drawCylinder(s, B, p, 0.18f, 10.0f, glm::vec3(0.32f));
    drawCylinder(s, B, p + glm::vec3(1.8f, 0, 0), 0.18f, 10.0f, glm::vec3(0.32f));
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(0.9f, 12.5f, 0), glm::vec3(9.0f, 4.5f, 0.35f), col, rotY);
    s.setBool("isEmissive", false);
}

// City signboard (single pole + sign)
void drawSignboard(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, glm::vec3 c)
{
    drawCylinder(s, B, p, 0.14f, 7.0f, glm::vec3(0.32f));
    drawCube(v, s, B, p + glm::vec3(0, 8.0f, 0), glm::vec3(5.5f, 2.2f, 0.18f), c);
}

// Toll booth (full plaza on highway deck)
void drawTollBooth(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 bc(0.85f, 0.75f, 0.55f), rf(0.3f, 0.3f, 0.6f), red(1, 0.25f, 0.15f);
    // Plaza base
    drawCube(v, s, B, p + glm::vec3(0, HWY_Y + 0.3f, 0), glm::vec3(7.0f, 0.5f, 20.0f), glm::vec3(0.42f));
    for (int g = -1; g <= 1; g++) {
        float gz = p.z + g * 6.0f;
        drawCube(v, s, B, glm::vec3(p.x, HWY_Y + 3.5f, gz), glm::vec3(3.0f, 5.5f, 3.0f), bc);
        drawCube(v, s, B, glm::vec3(p.x, HWY_Y + 6.5f, gz), glm::vec3(4.0f, 0.45f, 4.0f), rf);
        drawCube(v, s, B, glm::vec3(p.x + 3.5f, HWY_Y + 5.0f, gz), glm::vec3(5.5f, 0.18f, 0.18f), red);
    }
    // Overhead sign gantry
    drawCube(v, s, B, p + glm::vec3(0, HWY_Y + 10.5f, 0), glm::vec3(0.35f, 1.2f, 20.0f), glm::vec3(0.15f, 0.15f, 0.5f));
}

// ════════════════════════════════════════════════════════════
//  Vehicles  (travel along X on elevated highway)
//  Rotated 90° around Y so the vehicle's LOCAL forward = world X
// ════════════════════════════════════════════════════════════
void drawVehicle(unsigned int v, Shader& s, glm::mat4 B,
    glm::vec3 pos, int type, int dir)
{
    // 0° = faces +X (dir>0), 180° = faces -X (dir<0)
    float rotY = (dir > 0) ? 0.0f : 180.0f;
    glm::mat4 vm = glm::translate(B, pos);
    vm = myRotate(vm, glm::radians(rotY), glm::vec3(0, 1, 0));

    glm::vec3 body, top;
    glm::vec3 bsz, tsz, toff;
    switch (type) {
        //  Car
    case 0: body = glm::vec3(0.15f, 0.35f, 0.72f); top = glm::vec3(0.55f, 0.75f, 0.95f);
        bsz = glm::vec3(2.0f, 0.70f, 1.05f); tsz = glm::vec3(1.0f, 0.55f, 0.95f); toff = glm::vec3(-0.1f, 0.60f, 0); break;
        //  Bus
    case 1: body = glm::vec3(0.82f, 0.6f, 0.1f);  top = glm::vec3(0.92f, 0.85f, 0.3f);
        bsz = glm::vec3(3.8f, 1.40f, 1.20f); tsz = glm::vec3(3.5f, 1.10f, 1.10f); toff = glm::vec3(0, 1.15f, 0); break;
        //  Truck
    case 2: body = glm::vec3(0.5f, 0.48f, 0.48f); top = glm::vec3(0.38f, 0.35f, 0.35f);
        bsz = glm::vec3(3.3f, 1.00f, 1.10f); tsz = glm::vec3(1.6f, 1.40f, 1.00f); toff = glm::vec3(-0.5f, 1.15f, 0); break;
        //  Ambulance
    case 3: body = glm::vec3(0.95f, 0.95f, 0.95f); top = glm::vec3(0.92f, 0.18f, 0.18f);
        bsz = glm::vec3(2.8f, 1.00f, 1.10f); tsz = glm::vec3(1.5f, 0.50f, 1.00f); toff = glm::vec3(0.40f, 0.75f, 0); break;
        //  Fire truck
    default:body = glm::vec3(0.92f, 0.12f, 0.08f); top = glm::vec3(0.72f, 0.08f, 0.04f);
        bsz = glm::vec3(3.5f, 1.05f, 1.15f); tsz = glm::vec3(2.2f, 0.80f, 1.05f); toff = glm::vec3(-0.35f, 0.95f, 0); break;
    }

    drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.5f, 0), bsz, body);
    drawCube(v, s, vm, toff + glm::vec3(0, bsz.y * 0.5f, 0), tsz, top);

    // Wheels
    glm::vec3 wc(0.1f); glm::vec3 ws(0.42f, 0.42f, 0.18f);
    float wx = bsz.x * 0.35f, wz = bsz.z * 0.5f + 0.08f;
    drawCube(v, s, vm, glm::vec3(-wx, 0.22f, wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(wx, 0.22f, wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(-wx, 0.22f, -wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(wx, 0.22f, -wz), ws, wc);

    // Windshield
    float fx = bsz.x * 0.48f;
    drawCube(v, s, vm, glm::vec3(fx, bsz.y * 0.7f, 0),
        glm::vec3(0.08f, bsz.y * 0.5f, bsz.z * 0.7f), glm::vec3(0.12f, 0.15f, 0.2f));

    if (isNight) {
        s.setBool("isEmissive", true);
        drawCube(v, s, vm, glm::vec3(fx + 0.02f, bsz.y * 0.45f, -0.28f), glm::vec3(0.06f, 0.18f, 0.18f), glm::vec3(1, 1, 0.78f));
        drawCube(v, s, vm, glm::vec3(fx + 0.02f, bsz.y * 0.45f, 0.28f), glm::vec3(0.06f, 0.18f, 0.18f), glm::vec3(1, 1, 0.78f));
        drawCube(v, s, vm, glm::vec3(-fx - 0.02f, bsz.y * 0.45f, -0.28f), glm::vec3(0.06f, 0.14f, 0.14f), glm::vec3(0.9f, 0.05f, 0.05f));
        drawCube(v, s, vm, glm::vec3(-fx - 0.02f, bsz.y * 0.45f, 0.28f), glm::vec3(0.06f, 0.14f, 0.14f), glm::vec3(0.9f, 0.05f, 0.05f));
        s.setBool("isEmissive", false);
    }
}

// Boat (hull + cabin + mast + flag)
void drawBoat(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, int dir)
{
    glm::mat4 bm = glm::translate(B, p);
    if (dir < 0) bm = myRotate(bm, glm::radians(180.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, bm, glm::vec3(0, 0, 0), glm::vec3(2.5f, 1.1f, 6.5f), glm::vec3(0.45f, 0.25f, 0.12f));
    drawCube(v, s, bm, glm::vec3(0, 1.3f, 0.8f), glm::vec3(1.4f, 1.3f, 1.8f), glm::vec3(0.85f, 0.82f, 0.78f));
    drawCylinder(s, bm, glm::vec3(0, 1.3f, -2.2f), 0.07f, 2.2f, glm::vec3(0.35f));
    drawCube(v, s, bm, glm::vec3(0.45f, 2.8f, -2.2f), glm::vec3(0.85f, 0.55f, 0.1f), glm::vec3(0.92f, 0.2f, 0.15f));
}

// ════════════════════════════════════════════════════════════
//  BUILDINGS  (all 4× larger than v2)
// ════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════
//  JFP – Jamuna Future Park  (landmark mega-mall)
//  Building origin p: center at ground level
//  Main block: 44 wide (X) × 35 deep (Z) × 40 tall (Y)
//  halfW=22  halfD=17.5  front face at p.z-17.5 (faces −Z / highway)
//
//  Academic concepts demonstrated:
//   • Bezier curves  → curved front façade strips
//   • Ruled surface  → parking ramp (two edge rails connected by quads)
//   • Hierarchical   → drawJFP() calls 6 child functions
//   • Phong lighting → concrete / glass / metal material variation
// ════════════════════════════════════════════════════════════

// ── 1. Curved glass front façade via quadratic Bezier strips ──
void drawCurvedFacadeBezier(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    // Quadratic Bezier: P0=(−halfW, 0), P1=(0, −protrude), P2=(+halfW, 0)
    // x-component traces width; z-component gives forward curvature.
    const float halfW = 22.0f;
    const float h = 40.0f;
    const float protrude = 2.8f;       // centre protrudes this far in -Z
    const float baseZ = -17.5f;     // front face of main block
    const int   N = 26;         // number of vertical strips
    const float sw = (halfW * 2.0f) / (N - 1) * 1.06f;  // strip width with slight overlap

    glm::vec3 glass(0.38f, 0.60f, 0.84f), frame(0.28f, 0.34f, 0.52f);

    for (int i = 0; i < N; i++) {
        float t = (float)i / (N - 1);
        float u = 1.0f - t;
        float bx = u * u * (-halfW) + 2 * u * t * 0.0f + t * t * halfW;
        float bz = 2.0f * u * t * (-protrude);            // quadratic term only

        // Skip the 24-unit-wide entrance opening in the centre
        if (fabsf(bx) < 12.8f) continue;

        // Full-height glass strip
        s.setVec3("material.ambient", glass * 0.20f);
        s.setVec3("material.diffuse", glass);
        s.setVec3("material.specular", glm::vec3(0.75f));  // high specular = glass look
        s.setFloat("material.shininess", 128.0f);
        s.setBool("isEmissive", false); s.setBool("isWater", false);
        glm::mat4 m = glm::translate(B, p + glm::vec3(bx, h * 0.5f, baseZ + bz));
        m = glm::scale(m, glm::vec3(sw, h, 0.5f));
        s.setMat4("model", m);
        glBindVertexArray(v); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Horizontal floor-band on this strip
        for (float y = 0.0f; y <= h + 0.1f; y += 6.5f)
            drawCube(v, s, B, p + glm::vec3(bx, y, baseZ + bz), glm::vec3(sw * 1.05f, 0.38f, 0.55f), frame);
    }
    // Vertical mullions at every 5th unit in X (structural lines)
    for (float x = -20.0f; x <= 20.0f; x += 5.0f) {
        if (fabsf(x) < 12.8f) continue;
        float t = (x + halfW) / (halfW * 2);
        float u = 1.0f - t;
        float bz = 2.0f * u * t * (-protrude);
        drawCube(v, s, B, p + glm::vec3(x, h * 0.5f, baseZ + bz + 0.01f),
            glm::vec3(0.28f, h, 0.55f), frame);
    }
}

// ── 2. Glass entrance atrium ──
void drawEntrance(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    const float ew = 24.0f, eh = 20.0f, ed = 8.0f;
    const float ez = -17.5f - ed * 0.5f;  // entrance centre Z (relative to p)
    glm::vec3 glass(0.40f, 0.65f, 0.86f), frame(0.26f, 0.32f, 0.50f), lite(0.92f, 0.90f, 0.78f);

    // Front glass wall
    drawCube(v, s, B, p + glm::vec3(0, eh * 0.5f, ez - ed * 0.5f + 0.25f),
        glm::vec3(ew, eh, 0.38f), glass);
    // Side glass panels
    drawCube(v, s, B, p + glm::vec3(-ew * 0.5f, eh * 0.5f, ez), glm::vec3(0.38f, eh, ed), glass);
    drawCube(v, s, B, p + glm::vec3(ew * 0.5f, eh * 0.5f, ez), glm::vec3(0.38f, eh, ed), glass);
    // Roof canopy slab
    drawCube(v, s, B, p + glm::vec3(0, eh + 0.25f, ez), glm::vec3(ew + 2.5f, 0.5f, ed + 2.5f), frame);
    // Horizontal grid bands on front glass
    for (float y = 4.0f; y < eh; y += 4.0f)
        drawCube(v, s, B, p + glm::vec3(0, y, ez - ed * 0.5f + 0.35f),
            glm::vec3(ew + 0.5f, 0.28f, 0.28f), frame);
    // Vertical grid lines on front glass
    for (float x = -9.0f; x <= 9.0f; x += 4.5f)
        drawCube(v, s, B, p + glm::vec3(x, eh * 0.5f, ez - ed * 0.5f + 0.35f),
            glm::vec3(0.28f, eh, 0.28f), frame);
    // Double entry doors (emissive glow at night = lit lobby)
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(-4.0f, 3.0f, ez - ed * 0.5f + 0.15f),
        glm::vec3(3.0f, 5.8f, 0.25f), isNight ? lite : glass);
    drawCube(v, s, B, p + glm::vec3(4.0f, 3.0f, ez - ed * 0.5f + 0.15f),
        glm::vec3(3.0f, 5.8f, 0.25f), isNight ? lite : glass);
    s.setBool("isEmissive", false);
    // Welcome sign above entrance
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(0, eh + 1.2f, ez - ed * 0.5f + 0.1f),
        glm::vec3(16, 2.5f, 0.35f), glm::vec3(0.88f, 0.12f, 0.12f));
    s.setBool("isEmissive", false);
}

// ── 3. Cylindrical structural columns ──
void drawColumns(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    const int   nCols = 11;
    const float ew = 26.0f;   // spans slightly wider than entrance
    const float colH = 21.5f;
    const float colR = 0.9f;
    const float ez = -17.5f - 8.0f;   // front of entrance
    glm::vec3 conc(0.70f, 0.68f, 0.66f);

    // Row along entrance front edge
    for (int i = 0; i < nCols; i++) {
        float cx = ((float)i / (nCols - 1) - 0.5f) * 2.0f * (ew * 0.5f);
        drawCylinder(s, B, p + glm::vec3(cx, 0, ez), colR, colH, conc);
    }
    // Interior structural columns (4 rows × 3 columns inside main block)
    for (float x : {-14.0f, 0.0f, 14.0f})
        for (float z : {-10.0f, 0.0f, 10.0f})
            drawCylinder(s, B, p + glm::vec3(x, 0, z), 0.75f, 40.0f, conc);
}

// ── 4. Floor-by-floor window bands (side + back faces) ──
void drawWindowSystem(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    const float halfW = 22.0f, halfD = 17.5f, flH = 6.5f;
    const int   floors = 6;
    glm::vec3 win(0.44f, 0.66f, 0.90f), sep(0.28f, 0.32f, 0.48f);

    for (int f = 0; f < floors; f++) {
        float yc = f * flH + 3.2f;
        // Right face (+X)
        for (float z = -13.0f; z <= 13.0f; z += 3.5f)
            drawCube(v, s, B, p + glm::vec3(halfW + 0.1f, yc, z), glm::vec3(0.14f, 1.9f, 1.5f), win);
        // Left face (-X)
        for (float z = -13.0f; z <= 13.0f; z += 3.5f)
            drawCube(v, s, B, p + glm::vec3(-halfW - 0.1f, yc, z), glm::vec3(0.14f, 1.9f, 1.5f), win);
        // Back face (+Z)
        for (float x = -19.0f; x <= 19.0f; x += 3.5f)
            drawCube(v, s, B, p + glm::vec3(x, yc, halfD + 0.1f), glm::vec3(1.5f, 1.9f, 0.14f), win);
    }
    // Floor separator lines on left/right faces
    for (int f = 0; f <= floors; f++) {
        float y = f * flH;
        drawCube(v, s, B, p + glm::vec3(halfW + 0.05f, y, 0), glm::vec3(0.12f, 0.38f, 35.5f), sep);
        drawCube(v, s, B, p + glm::vec3(-halfW - 0.05f, y, 0), glm::vec3(0.12f, 0.38f, 35.5f), sep);
    }
}

// ── 5. Roof slab, parapet walls, HVAC, rooftop signage ──
void drawRoof(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    const float h = 40.0f, halfW = 22.0f, halfD = 17.5f;
    glm::vec3 slab(0.52f, 0.52f, 0.50f), parapet(0.58f, 0.56f, 0.54f), hvac(0.40f, 0.40f, 0.38f);

    // Roof slab (slightly overhangs footprint)
    drawCube(v, s, B, p + glm::vec3(0, h + 0.3f, 0), glm::vec3(47, 0.6f, 38), slab);
    // Parapet walls on all 4 edges
    drawCube(v, s, B, p + glm::vec3(0, h + 1.5f, -halfD - 0.6f), glm::vec3(48, 2.2f, 0.65f), parapet);
    drawCube(v, s, B, p + glm::vec3(0, h + 1.5f, halfD + 0.6f), glm::vec3(48, 2.2f, 0.65f), parapet);
    drawCube(v, s, B, p + glm::vec3(-halfW - 0.6f, h + 1.5f, 0), glm::vec3(0.65f, 2.2f, 38), parapet);
    drawCube(v, s, B, p + glm::vec3(halfW + 0.6f, h + 1.5f, 0), glm::vec3(0.65f, 2.2f, 38), parapet);
    // HVAC units (symmetric 2×5 grid)
    float hx[] = { -16,-8,0,8,16 };
    float hz[] = { -6, 6 };
    for (float hxi : hx)
        for (float hzi : hz)
            drawCube(v, s, B, p + glm::vec3(hxi, h + 2.9f, hzi), glm::vec3(3.8f, 1.8f, 3.8f), hvac);
    // Rooftop billboard / JFP sign (emissive at night)
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(0, h + 4.2f, -halfD - 0.3f),
        glm::vec3(26, 3.8f, 0.45f), glm::vec3(0.88f, 0.10f, 0.14f));
    s.setBool("isEmissive", false);
    // Flag poles at roof corners
    glm::vec3 poleC(0.50f, 0.50f, 0.52f);
    float fpx[] = { -20,20 }, fpz[] = { -15,15 };
    for (float fx : fpx)
        for (float fz : fpz) {
            drawCylinder(s, B, p + glm::vec3(fx, h + 0.6f, fz), 0.12f, 5.5f, poleC);
            drawCube(v, s, B, p + glm::vec3(fx + 0.7f, h + 4.5f, fz), glm::vec3(2.2f, 1.2f, 0.08f),
                glm::vec3(0.1f, 0.35f, 0.72f));
        }
}

// ── 6. Side wings & back extension ──
void drawSideWings(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wing(0.70f, 0.72f, 0.74f), wwin(0.44f, 0.66f, 0.88f);
    // Left wing (extends -X beyond main block)
    drawCube(v, s, B, p + glm::vec3(-29, 12.5f, 2), glm::vec3(12, 25, 28), wing);
    for (float y = 2.5f; y < 24; y += 4.5f)
        for (float z = -8.0f; z <= 8.0f; z += 3.5f)
            drawCube(v, s, B, p + glm::vec3(-35.1f, y, 2 + z), glm::vec3(0.14f, 1.7f, 1.3f), wwin);
    // Back extension (+Z, service/loading bay)
    drawCube(v, s, B, p + glm::vec3(0, 10.0f, 26.5f), glm::vec3(44, 20, 17), wing * 0.95f);
    for (float y = 2.5f; y < 19; y += 4.5f)
        for (float x = -17.0f; x <= 17.0f; x += 3.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, 35.1f), glm::vec3(1.3f, 1.7f, 0.14f), wwin);
}

// ── 7. Parking ramps – ruled surface approximation ──
//  A ruled surface connects two parallel guide rails;
//  each cross-section quad is one segment of the ruled surface.
void drawParkingRampRuledSurface(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 asph(0.20f, 0.20f, 0.20f), wall(0.48f, 0.46f, 0.44f);
    const float rampW = 9.5f, rampDrop = 4.5f, rampLen = 13.0f;
    const int   nSeg = 8;

    // Helper lambda draws one ramp at offset rBase
    auto drawRamp = [&](glm::vec3 rBase) {
        for (int i = 0; i < nSeg; i++) {
            float t0 = (float)i / nSeg, t1 = (float)(i + 1) / nSeg;
            float z0 = t0 * rampLen, z1 = t1 * rampLen, zm = (z0 + z1) * 0.5f;
            float y0 = -t0 * rampDrop, y1 = -t1 * rampDrop, ym = (y0 + y1) * 0.5f;
            float segL = sqrtf((z1 - z0) * (z1 - z0) + (y1 - y0) * (y1 - y0));
            float ang = atan2f(y1 - y0, z1 - z0);
            glm::mat4 rm = glm::translate(B, rBase + glm::vec3(0, ym, zm));
            rm = myRotate(rm, -ang, glm::vec3(1, 0, 0));
            rm = glm::scale(rm, glm::vec3(rampW, 0.5f, segL * 1.05f));
            s.setMat4("model", rm);
            s.setVec3("material.ambient", asph * 0.25f);
            s.setVec3("material.diffuse", asph);
            s.setVec3("material.specular", glm::vec3(0.08f));
            s.setFloat("material.shininess", 8);
            s.setBool("isEmissive", false); s.setBool("isWater", false);
            glBindVertexArray(v); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        // Ramp side retaining walls
        drawCube(v, s, B, rBase + glm::vec3(-rampW * 0.5f - 0.3f, -2, rampLen * 0.5f),
            glm::vec3(0.5f, 4.5f, rampLen), wall);
        drawCube(v, s, B, rBase + glm::vec3(rampW * 0.5f + 0.3f, -2, rampLen * 0.5f),
            glm::vec3(0.5f, 4.5f, rampLen), wall);
        };

    // Two ramps: left and right of entrance
    drawRamp(p + glm::vec3(-17.5f, 0, -19.0f));
    drawRamp(p + glm::vec3(17.5f, 0, -19.0f));
}

// ── Master JFP function ──
void drawJFP(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 conc(0.72f, 0.72f, 0.74f);
    // Main structural concrete body (44 × 35 × 40)
    drawCube(v, s, B, p + glm::vec3(0, 20, 0), glm::vec3(44, 40, 35), conc);
    // Child node functions (hierarchical model)
    drawSideWings(v, s, B, p);
    drawCurvedFacadeBezier(v, s, B, p);
    drawEntrance(v, s, B, p);
    drawColumns(v, s, B, p);
    drawWindowSystem(v, s, B, p);
    drawRoof(v, s, B, p);
    drawParkingRampRuledSurface(v, s, B, p);
    // Ground-level plaza slab
    drawCube(v, s, B, p + glm::vec3(0, -0.3f, 0), glm::vec3(52, 0.6f, 44), glm::vec3(0.66f, 0.64f, 0.61f));
    // Plaza water feature (decorative reflecting pool)
    s.setBool("isWater", true); s.setFloat("time", gTime);
    drawCube(v, s, B, p + glm::vec3(0, 0.12f, -27), glm::vec3(18, 0.22f, 4.5f), glm::vec3(0.1f, 0.35f, 0.65f));
    s.setBool("isWater", false);
}

// ── Hotel (tall modern tower, porte-cochere, penthouse) ──
void drawHotel(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wall(0.92f, 0.88f, 0.78f), trim(0.7f, 0.65f, 0.55f);
    // Main tower body  20 × 52 × 20
    drawCube(v, s, B, p + glm::vec3(0, 26.0f, 0), glm::vec3(20, 52, 20), wall);
    // Front windows (-Z face)
    for (float y = 3.0f; y < 50.0f; y += 3.5f)
        for (float x = -7.0f; x <= 7.0f; x += 2.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, -10.01f), glm::vec3(1.4f, 1.8f, 0.12f), glm::vec3(0.55f, 0.70f, 0.85f));
    // Side windows
    for (float y = 3.0f; y < 50.0f; y += 3.5f)
        for (float z = -7.0f; z <= 7.0f; z += 2.5f) {
            drawCube(v, s, B, p + glm::vec3(10.01f, y, z), glm::vec3(0.12f, 1.8f, 1.4f), glm::vec3(0.55f, 0.70f, 0.85f));
            drawCube(v, s, B, p + glm::vec3(-10.01f, y, z), glm::vec3(0.12f, 1.8f, 1.4f), glm::vec3(0.55f, 0.70f, 0.85f));
        }
    // Porte-cochere
    drawCube(v, s, B, p + glm::vec3(0, 7.5f, -14.0f), glm::vec3(10, 0.35f, 6.5f), trim);
    for (float x = -4.0f; x <= 4.0f; x += 4.0f)
        drawCylinder(s, B, p + glm::vec3(x, 0, -16.0f), 0.5f, 7.5f, wall);
    // Penthouse
    drawCube(v, s, B, p + glm::vec3(0, 54.0f, 0), glm::vec3(10, 3.5f, 10), trim);
    drawCylinder(s, B, p + glm::vec3(0, 57.5f, 0), 0.14f, 5.5f, glm::vec3(0.4f));
    // Roof billboard
    drawBillboard(v, s, B, p + glm::vec3(-2, 53.0f, -10.5f), glm::vec3(0.9f, 0.8f, 0.25f));
}

// ── Cricket Stadium (3-tier circular bowl, canopy ring, floodlights) ──
void drawCricketStadium(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    int seg = 36;
    float oR = 18.0f, iR = 11.0f, dth = 2 * PI / seg;
    // Green playing field
    drawCube(v, s, B, p + glm::vec3(0, 0.12f, 0), glm::vec3(iR * 2, 0.22f, iR * 2), glm::vec3(0.22f, 0.62f, 0.18f));
    // Pitch strip
    drawCube(v, s, B, p + glm::vec3(0, 0.20f, 0), glm::vec3(1.2f, 0.14f, 9.0f), glm::vec3(0.72f, 0.62f, 0.42f));
    // 3-tier stands (rendered as stand segment cubes around perimeter)
    float tierH[] = { 0.0f,  6.0f, 12.0f };
    float tierOR[] = { oR,    oR - 1.0f, oR - 2.0f };
    for (int t = 0; t < 3; t++) {
        glm::vec3 tc = (t == 1) ? glm::vec3(0.68f) : glm::vec3(0.75f, 0.75f, 0.78f);
        for (int i = 0; i < seg; i++) {
            float a = i * dth;
            float rm = (tierOR[t] + iR + t * 0.8f) * 0.5f;
            float mx = cosf(a) * rm, mz = sinf(a) * rm;
            drawCube(v, s, B, p + glm::vec3(mx, tierH[t] + 3.5f, mz),
                glm::vec3(3.8f, 7.0f, (tierOR[t] - iR + 1.5f)), tc, -glm::degrees(a));
        }
    }
    // Canopy ring
    for (int i = 0; i < seg; i++) {
        float a = i * dth;
        drawCube(v, s, B, p + glm::vec3(cosf(a) * oR, 20.0f, sinf(a) * oR),
            glm::vec3(4.0f, 0.3f, 6.0f), glm::vec3(0.88f, 0.88f, 0.90f), -glm::degrees(a));
    }
    // 4 floodlight towers
    float fp[][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
    for (auto& f : fp) {
        glm::vec3 lp = p + glm::vec3(f[0] * oR * 1.2f, 0, f[1] * oR * 1.2f);
        drawCylinder(s, B, lp, 0.3f, 28.0f, glm::vec3(0.35f));
        s.setBool("isEmissive", isNight);
        drawCube(v, s, B, lp + glm::vec3(0, 28.8f, 0), glm::vec3(1.8f, 0.9f, 1.8f),
            isNight ? glm::vec3(1, 0.95f, 0.72f) : glm::vec3(0.5f));
        s.setBool("isEmissive", false);
    }
    // Entry gates
    for (float a : {0.0f, PI * 0.5f, PI, PI * 1.5f})
        drawCube(v, s, B, p + glm::vec3(cosf(a) * oR, 3.5f, sinf(a) * oR),
            glm::vec3(4.5f, 7.0f, 1.5f), glm::vec3(0.3f, 0.28f, 0.25f));
}

// ── Apartment tower ──
void drawApartment(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float h, glm::vec3 tint)
{
    drawCube(v, s, B, p + glm::vec3(0, h * 0.5f, 0), glm::vec3(16, h, 16), tint);
    // Balcony slabs every 6 units
    for (float y = 6.0f; y < h; y += 6.0f)
        drawCube(v, s, B, p + glm::vec3(0, y, -8.01f), glm::vec3(13, 0.25f, 1.2f), tint * 0.82f);
    // Front windows (-Z face)
    for (float y = 3.0f; y < h - 1.0f; y += 3.5f)
        for (float x = -5.5f; x <= 5.5f; x += 2.2f)
            drawCube(v, s, B, p + glm::vec3(x, y, -8.01f), glm::vec3(1.1f, 1.8f, 0.14f), glm::vec3(0.5f, 0.72f, 0.9f));
    // Side windows (+X face)
    for (float y = 3.0f; y < h - 1.0f; y += 3.5f)
        for (float z = -5.0f; z <= 5.0f; z += 2.2f)
            drawCube(v, s, B, p + glm::vec3(8.01f, y, z), glm::vec3(0.14f, 1.8f, 1.1f), glm::vec3(0.5f, 0.72f, 0.9f));
    // Roof water-tank + stairwell
    drawCube(v, s, B, p + glm::vec3(2.5f, h + 1.2f, 2.5f), glm::vec3(2.0f, 2.5f, 2.0f), glm::vec3(0.32f));
    drawCube(v, s, B, p + glm::vec3(-2.0f, h + 1.5f, -2.0f), glm::vec3(3.5f, 3.0f, 3.5f), tint * 0.85f);
}

// ── Corporate Office (glass curtain-wall skyscraper) ──
void drawCorporateOffice(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 frame(0.3f, 0.42f, 0.58f), glass(0.42f, 0.62f, 0.8f);
    // Main tower  20 × 72 × 18
    drawCube(v, s, B, p + glm::vec3(0, 36.0f, 0), glm::vec3(20, 72, 18), frame);
    // Glass curtain bands on all 4 faces
    for (float y = 2.0f; y < 70.0f; y += 2.5f) {
        drawCube(v, s, B, p + glm::vec3(0, y, 9.01f), glm::vec3(18, 1.8f, 0.14f), glass);
        drawCube(v, s, B, p + glm::vec3(0, y, -9.01f), glm::vec3(18, 1.8f, 0.14f), glass);
        drawCube(v, s, B, p + glm::vec3(10.01f, y, 0), glm::vec3(0.14f, 1.8f, 16), glass);
        drawCube(v, s, B, p + glm::vec3(-10.01f, y, 0), glm::vec3(0.14f, 1.8f, 16), glass);
    }
    // Vertical mullions on front face
    for (float x = -8.0f; x <= 8.0f; x += 2.5f)
        drawCube(v, s, B, p + glm::vec3(x, 36.0f, 9.02f), glm::vec3(0.2f, 70, 0.08f), glm::vec3(0.88f));
    // Ground floor lobby
    drawCube(v, s, B, p + glm::vec3(0, 3.5f, 9.2f), glm::vec3(5.5f, 7.0f, 0.3f), glm::vec3(0.32f, 0.5f, 0.68f));
    // Helipad on roof
    drawCube(v, s, B, p + glm::vec3(0, 72.1f, 0), glm::vec3(7.0f, 0.2f, 7.0f), glm::vec3(0.5f, 0.5f, 0.18f));
    drawCylinder(s, B, p + glm::vec3(0, 72.3f, 0), 0.14f, 7.0f, glm::vec3(0.4f));
}

// ── Engineering University (E-shape, pillared portico, dome) ──
void drawUniversity(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 brick(0.62f, 0.35f, 0.25f), trim(0.88f, 0.82f, 0.72f);
    // Central block  40 × 28 × 24
    drawCube(v, s, B, p + glm::vec3(0, 14.0f, 0), glm::vec3(40, 28, 24), brick);
    // Left wing
    drawCube(v, s, B, p + glm::vec3(-26.0f, 10.0f, 0), glm::vec3(14, 20, 20), brick * 0.95f);
    // Right wing
    drawCube(v, s, B, p + glm::vec3(26.0f, 10.0f, 0), glm::vec3(14, 20, 20), brick * 0.95f);
    // Entrance portico (+Z face, towards highway for city2)
    drawCube(v, s, B, p + glm::vec3(0, 12.0f, 13.5f), glm::vec3(24, 0.5f, 3.5f), trim);
    for (float x = -10.0f; x <= 10.0f; x += 4.0f)
        drawCylinder(s, B, p + glm::vec3(x, 0, 14.5f), 0.45f, 13.5f, trim);
    // Pediment
    drawCube(v, s, B, p + glm::vec3(0, 24.0f, 13.0f), glm::vec3(28, 3.5f, 0.6f), trim);
    // Dome on central block
    drawSphere(s, B, p + glm::vec3(0, 30.5f, 0), 5.5f, glm::vec3(0.58f, 0.52f, 0.45f));
    // Window bands on central block front face
    for (float y = 3.0f; y < 24.0f; y += 4.0f)
        for (float x = -14.0f; x <= 14.0f; x += 3.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, 12.01f), glm::vec3(1.8f, 2.2f, 0.14f), glm::vec3(0.52f, 0.68f, 0.82f));
    // University lawn
    drawCube(v, s, B, p + glm::vec3(0, 0.08f, 0), glm::vec3(72, 0.14f, 30), glm::vec3(0.32f, 0.68f, 0.22f));
}

// ── Hospital (multi-wing, red cross, helipad) ──
void drawHospital(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wh(0.94f, 0.94f, 0.92f);
    // Main tower  24 × 40 × 20
    drawCube(v, s, B, p + glm::vec3(0, 20.0f, 0), glm::vec3(24, 40, 20), wh);
    // Front annex (lower, wider)
    drawCube(v, s, B, p + glm::vec3(0, 10.0f, 15.0f), glm::vec3(36, 20, 12), wh * 0.97f);
    // Side annex
    drawCube(v, s, B, p + glm::vec3(16.5f, 12.0f, 0), glm::vec3(10, 24, 16), wh * 0.95f);
    // Red cross (emissive)
    s.setBool("isEmissive", true);
    drawCube(v, s, B, p + glm::vec3(0, 14.0f, 21.01f), glm::vec3(1.0f, 5.5f, 0.14f), glm::vec3(0.88f, 0.08f, 0.08f));
    drawCube(v, s, B, p + glm::vec3(0, 14.0f, 21.01f), glm::vec3(5.5f, 1.0f, 0.14f), glm::vec3(0.88f, 0.08f, 0.08f));
    s.setBool("isEmissive", false);
    // Main entrance
    drawCube(v, s, B, p + glm::vec3(0, 4.5f, 21.02f), glm::vec3(5.5f, 6.5f, 0.14f), glm::vec3(0.38f, 0.60f, 0.75f));
    // Window bands on front annex (+Z)
    for (float y = 3.5f; y < 18.5f; y += 3.5f)
        for (float x = -14.0f; x <= 14.0f; x += 2.8f)
            drawCube(v, s, B, p + glm::vec3(x, y, 21.01f), glm::vec3(1.4f, 2.0f, 0.14f), glm::vec3(0.55f, 0.75f, 0.90f));
    // Window bands on main tower front (+Z half)
    for (float y = 2.5f; y < 38.0f; y += 3.0f)
        for (float x = -8.0f; x <= 8.0f; x += 2.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, 10.01f), glm::vec3(1.2f, 1.8f, 0.14f), glm::vec3(0.55f, 0.75f, 0.90f));
    // Helipad
    drawCylinder(s, B, p + glm::vec3(0, 40.08f, 0), 5.5f, 0.15f, glm::vec3(0.78f, 0.78f, 0.75f));
    drawCube(v, s, B, p + glm::vec3(0, 40.2f, 0), glm::vec3(0.7f, 0.12f, 4.0f), glm::vec3(0.95f));
    drawCube(v, s, B, p + glm::vec3(-1.3f, 40.2f, 0), glm::vec3(0.7f, 0.12f, 0.7f), glm::vec3(0.95f));
    drawCube(v, s, B, p + glm::vec3(1.3f, 40.2f, 0), glm::vec3(0.7f, 0.12f, 0.7f), glm::vec3(0.95f));
    // Ambulance bay overhang
    drawCube(v, s, B, p + glm::vec3(9.0f, 6.5f, 23.5f), glm::vec3(6.5f, 0.25f, 5.0f), glm::vec3(0.6f, 0.58f, 0.55f));
}

// ── Park (lawn, Bezier pathway, trees, pond, benches) ──
void drawPark(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    drawCube(v, s, B, p + glm::vec3(0, 0.08f, 0), glm::vec3(36, 0.14f, 32), glm::vec3(0.28f, 0.65f, 0.22f));
    // Bezier curved main path
    glm::vec3 pp0 = p + glm::vec3(-16, 0.25f, -12), pp1 = p + glm::vec3(-5, 0.25f, 8),
        pp2 = p + glm::vec3(6, 0.25f, -4), pp3 = p + glm::vec3(16, 0.25f, 12);
    for (int i = 0; i < 18; i++) {
        float t = i / 17.0f;
        glm::vec3 pt = bezier3(pp0, pp1, pp2, pp3, t);
        drawCube(v, s, B, pt, glm::vec3(2.0f, 0.15f, 1.4f), glm::vec3(0.68f, 0.58f, 0.45f));
    }
    // Trees
    drawTree(v, s, B, p + glm::vec3(-12, 0, -9), 3.2f);
    drawTree(v, s, B, p + glm::vec3(12, 0, -9), 2.8f);
    drawTree(v, s, B, p + glm::vec3(0, 0, 12), 3.5f);
    drawTree(v, s, B, p + glm::vec3(-12, 0, 6), 2.5f);
    drawTree(v, s, B, p + glm::vec3(10, 0, 4), 3.0f);
    drawTree(v, s, B, p + glm::vec3(-6, 0, -2), 2.2f);
    drawTree(v, s, B, p + glm::vec3(14, 0, -4), 2.8f);
    drawTree(v, s, B, p + glm::vec3(-14, 0, 12), 3.2f);
    // Benches
    drawCube(v, s, B, p + glm::vec3(3.5f, 1.0f, 0), glm::vec3(3.5f, 0.25f, 1.0f), glm::vec3(0.52f, 0.32f, 0.15f));
    drawCube(v, s, B, p + glm::vec3(3.5f, 1.6f, -0.45f), glm::vec3(3.5f, 1.0f, 0.2f), glm::vec3(0.52f, 0.32f, 0.15f));
    drawCube(v, s, B, p + glm::vec3(-8, 1.0f, 8), glm::vec3(3.5f, 0.25f, 1.0f), glm::vec3(0.52f, 0.32f, 0.15f));
    drawCube(v, s, B, p + glm::vec3(-8, 1.6f, 7.55f), glm::vec3(3.5f, 1.0f, 0.2f), glm::vec3(0.52f, 0.32f, 0.15f));
    // Pond
    s.setBool("isWater", true); s.setFloat("time", gTime);
    drawCube(v, s, B, p + glm::vec3(12, 0.1f, -5), glm::vec3(5.5f, 0.2f, 4.5f), glm::vec3(0.1f, 0.35f, 0.65f));
    s.setBool("isWater", false);
}

// ── Eiffel-style Tower (tripod base, column, saucer deck, spire) ──
void drawEiffelTower(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wh(0.96f, 0.96f, 0.94f), steel(0.62f, 0.62f, 0.65f), dkGlass(0.15f, 0.15f, 0.18f);
    // 3 tripod legs (120° apart), each with 6 sub-segments curving inward
    for (int i = 0; i < 3; i++) {
        float a = glm::radians(i * 120.0f);
        float bx = cosf(a) * 14.0f, bz = sinf(a) * 14.0f;
        for (int sg = 0; sg < 8; sg++) {
            float t0 = sg / 8.0f, t1 = (sg + 1) / 8.0f, tm = (t0 + t1) * 0.5f;
            float inw = 1.0f - tm * 0.92f;
            float cx = bx * inw, cz = bz * inw;
            float y = t0 * 26.0f, ht = 26.0f / 8.0f;
            float thick = 0.85f * (1.0f - tm * 0.5f);
            drawCube(v, s, B, p + glm::vec3(cx, y + ht * 0.5f, cz), glm::vec3(thick, ht, thick), wh);
        }
    }
    // Central column (tapered)
    drawCylinder(s, B, p + glm::vec3(0, 26, 0), 2.0f, 14.0f, wh);
    drawCylinder(s, B, p + glm::vec3(0, 40, 0), 1.5f, 5.0f, wh);
    // Saucer / observation deck
    drawCylinder(s, B, p + glm::vec3(0, 44, 0), 12.0f, 0.7f, wh * 0.95f);  // main disc
    drawCylinder(s, B, p + glm::vec3(0, 44.7f, 0), 9.5f, 1.2f, wh * 0.92f); // upper ring
    drawCylinder(s, B, p + glm::vec3(0, 43.3f, 0), 9.5f, 1.2f, wh * 0.92f); // lower ring
    drawCylinder(s, B, p + glm::vec3(0, 44.0f, 0), 12.1f, 0.8f, dkGlass); // dark glass ring
    // Antenna spire
    drawCylinder(s, B, p + glm::vec3(0, 45.9f, 0), 0.38f, 16.0f, steel);
    // Beacon
    s.setBool("isEmissive", true);
    drawSphere(s, B, p + glm::vec3(0, 62.5f, 0), 0.65f, glm::vec3(1, 0.92f, 0.5f), true);
    s.setBool("isEmissive", false);
}

// ── Garden (X-pattern paths, 3-tier fountain, flower beds, hedges) ──
void drawGarden(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    // 4 lawn quadrants
    drawCube(v, s, B, p + glm::vec3(-7, 0.1f, -7), glm::vec3(12, 0.14f, 12), glm::vec3(0.32f, 0.72f, 0.22f));
    drawCube(v, s, B, p + glm::vec3(7, 0.1f, -7), glm::vec3(12, 0.14f, 12), glm::vec3(0.32f, 0.72f, 0.22f));
    drawCube(v, s, B, p + glm::vec3(-7, 0.1f, 7), glm::vec3(12, 0.14f, 12), glm::vec3(0.32f, 0.72f, 0.22f));
    drawCube(v, s, B, p + glm::vec3(7, 0.1f, 7), glm::vec3(12, 0.14f, 12), glm::vec3(0.32f, 0.72f, 0.22f));
    // X-pattern diagonal paths
    for (int i = -7; i <= 7; i++) {
        float t = i * 1.8f;
        drawCube(v, s, B, p + glm::vec3(t, 0.2f, t), glm::vec3(2.5f, 0.15f, 1.8f), glm::vec3(0.75f, 0.72f, 0.68f), 45);
        drawCube(v, s, B, p + glm::vec3(t, 0.2f, -t), glm::vec3(2.5f, 0.15f, 1.8f), glm::vec3(0.75f, 0.72f, 0.68f), -45);
    }
    // Border wall
    for (int side = 0; side < 4; side++) {
        float a = side * 90.0f;
        float wx = cosf(glm::radians(a)) * 15.0f;
        float wz = sinf(glm::radians(a)) * 15.0f;
        drawCube(v, s, B, p + glm::vec3(wx, 1.2f, wz),
            glm::vec3((side % 2 == 0) ? 0.7f : 30.0f, 2.4f, (side % 2 == 0) ? 30.0f : 0.7f),
            glm::vec3(0.76f, 0.72f, 0.66f));
    }
    // 3-tier fountain
    drawCylinder(s, B, p, 5.5f, 0.9f, glm::vec3(0.68f, 0.65f, 0.6f));
    drawCylinder(s, B, p + glm::vec3(0, 0.9f, 0), 3.2f, 2.0f, glm::vec3(0.68f, 0.65f, 0.6f));
    drawCylinder(s, B, p + glm::vec3(0, 2.9f, 0), 1.5f, 1.5f, glm::vec3(0.68f, 0.65f, 0.6f));
    drawCylinder(s, B, p + glm::vec3(0, 4.4f, 0), 0.3f, 2.2f, glm::vec3(0.68f, 0.65f, 0.6f));
    s.setBool("isWater", true); s.setFloat("time", gTime);
    drawCylinder(s, B, p + glm::vec3(0, 0.4f, 0), 4.8f, 0.25f, glm::vec3(0.3f, 0.6f, 0.88f));
    s.setBool("isWater", false);
    // Flower beds
    glm::vec3 fc[] = { glm::vec3(0.92f,0.2f,0.3f),glm::vec3(0.95f,0.82f,0.2f),
                      glm::vec3(0.82f,0.3f,0.88f),glm::vec3(1,0.55f,0.12f) };
    for (int i = 0; i < 4; i++) {
        float fx = cosf(glm::radians(i * 90.0f + 45.0f)) * 9.0f;
        float fz = sinf(glm::radians(i * 90.0f + 45.0f)) * 9.0f;
        drawSphere(s, B, p + glm::vec3(fx, 0.65f, fz), 0.9f, fc[i]);
        drawSphere(s, B, p + glm::vec3(fx * 0.7f, 0.5f, fz * 0.7f), 0.65f, fc[(i + 1) % 4]);
    }
    // Garden lamp posts
    for (int i = 0; i < 4; i++) {
        float lx = cosf(glm::radians(i * 90.0f)) * 8.0f;
        float lz = sinf(glm::radians(i * 90.0f)) * 8.0f;
        drawCylinder(s, B, p + glm::vec3(lx, 0, lz), 0.12f, 7.5f, glm::vec3(0.35f));
        s.setBool("isEmissive", isNight);
        drawSphere(s, B, p + glm::vec3(lx, 7.8f, lz), 0.45f,
            isNight ? glm::vec3(1, 0.95f, 0.7f) : glm::vec3(0.5f), isNight);
        s.setBool("isEmissive", false);
    }
}

// ════════════════════════════════════════════════════════════
//  INFRASTRUCTURE
// ════════════════════════════════════════════════════════════

// ── Ground (concrete city + only grass in correct zones) ──
void drawGround(unsigned int v, Shader& s, glm::mat4 B)
{
    glm::vec3 conc(0.72f, 0.70f, 0.67f), grass(0.38f, 0.68f, 0.28f);
    // City 1 concrete slab
    drawCube(v, s, B, glm::vec3(0, -0.5f, 65), glm::vec3(310, 1.0f, 58), conc);
    // City 2 concrete slab
    drawCube(v, s, B, glm::vec3(0, -0.5f, -65), glm::vec3(310, 1.0f, 58), conc);
    // Highway corridor ground
    drawCube(v, s, B, glm::vec3(0, -0.5f, 0), glm::vec3(310, 1.0f, 22), conc * 0.90f);
    // Grass strips along highway edges (city side)
    drawCube(v, s, B, glm::vec3(0, 0.1f, 12), glm::vec3(310, 0.15f, 4), grass);
    drawCube(v, s, B, glm::vec3(0, 0.1f, -12), glm::vec3(310, 0.15f, 4), grass);
    // Grass strips along river banks
    drawCube(v, s, B, glm::vec3(-(RIV_HW + 2.5f), 0.1f, 0), glm::vec3(4, 0.15f, 200), grass);
    drawCube(v, s, B, glm::vec3((RIV_HW + 2.5f), 0.1f, 0), glm::vec3(4, 0.15f, 200), grass);
}

// ── Elevated Highway (deck + barriers + median + markings + pillars + connecting roads) ──
void drawHighway(unsigned int v, Shader& s, glm::mat4 B)
{
    glm::vec3 deck(0.32f, 0.32f, 0.32f), barrier(0.78f, 0.78f, 0.76f);
    glm::vec3 median(0.72f, 0.72f, 0.70f), mark(0.95f, 0.95f, 0.95f);
    glm::vec3 pillar(0.65f, 0.65f, 0.62f), road(0.22f, 0.22f, 0.22f);

    float leftCx = -(SCENE_X + RIV_HW) * 0.5f;
    float rightCx = (SCENE_X + RIV_HW) * 0.5f;
    float deckLen = SCENE_X - RIV_HW;

    // ─ Left highway deck (X < -RIV_HW)
    drawCube(v, s, B, glm::vec3(leftCx, HWY_Y - 1.0f, 0), glm::vec3(deckLen, 2.0f, 18.0f), deck);
    // ─ Right highway deck (X > +RIV_HW)
    drawCube(v, s, B, glm::vec3(rightCx, HWY_Y - 1.0f, 0), glm::vec3(deckLen, 2.0f, 18.0f), deck);

    // Crash barriers at Z = ±9.5
    for (float side : {-9.5f, 9.5f}) {
        drawCube(v, s, B, glm::vec3(leftCx, HWY_Y + 0.6f, side), glm::vec3(deckLen, 1.2f, 0.5f), barrier);
        drawCube(v, s, B, glm::vec3(rightCx, HWY_Y + 0.6f, side), glm::vec3(deckLen, 1.2f, 0.5f), barrier);
    }
    // Central median
    drawCube(v, s, B, glm::vec3(leftCx, HWY_Y + 0.5f, 0), glm::vec3(deckLen, 1.0f, 1.2f), median);
    drawCube(v, s, B, glm::vec3(rightCx, HWY_Y + 0.5f, 0), glm::vec3(deckLen, 1.0f, 1.2f), median);

    // Lane markings at Z = ±4.5 every 7 units
    for (float x = -SCENE_X; x <= SCENE_X; x += 7.0f) {
        if (x > -(RIV_HW + 2) && x < (RIV_HW + 2)) continue;
        drawCube(v, s, B, glm::vec3(x, HWY_Y + 0.02f, -4.5f), glm::vec3(4.5f, 0.08f, 0.28f), mark);
        drawCube(v, s, B, glm::vec3(x, HWY_Y + 0.02f, 4.5f), glm::vec3(4.5f, 0.08f, 0.28f), mark);
    }

    // Support pillars every 18 units
    for (float x = -SCENE_X; x <= SCENE_X; x += 18.0f) {
        if (x > -(RIV_HW + 4) && x < (RIV_HW + 4)) continue;
        drawCube(v, s, B, glm::vec3(x, (HWY_Y - 2) * 0.5f, -4.5f), glm::vec3(5.5f, HWY_Y - 2, 3.0f), pillar);
        drawCube(v, s, B, glm::vec3(x, (HWY_Y - 2) * 0.5f, 4.5f), glm::vec3(5.5f, HWY_Y - 2, 3.0f), pillar);
    }

    // Connecting roads (ground level, along Z, under/beside highway)
    float connX[] = { -112,-70,-30, 30, 70, 112 };
    for (float cx : connX) {
        drawCube(v, s, B, glm::vec3(cx, 0.08f, 36), glm::vec3(5.5f, 0.35f, 52), road);
        drawCube(v, s, B, glm::vec3(cx, 0.08f, -36), glm::vec3(5.5f, 0.35f, 52), road);
        // Zebra crossings
        for (int zs = -2; zs <= 2; zs++)
            drawCube(v, s, B, glm::vec3(cx, 0.18f, zs * 1.4f), glm::vec3(5.0f, 0.08f, 0.7f), mark);
    }
}

// ── Bridge: Bezier arch deck + tied arches + suspender cables + pillars ──
void drawBridge(unsigned int v, Shader& s, glm::mat4 B)
{
    glm::vec3 dc(0.55f, 0.55f, 0.58f), arch(0.96f, 0.96f, 0.94f);
    glm::vec3 cable(0.62f, 0.62f, 0.65f), rail(0.72f, 0.18f, 0.12f), mark(1, 0.9f, 0.15f);

    // ─ Bridge deck via Bezier (N=48 segments) ─
    int N = 48;
    for (int i = 0; i < N; i++) {
        float t0 = i / (float)N, t1 = (i + 1) / (float)N;
        glm::vec3 a = bezier3(BP0, BP1, BP2, BP3, t0);
        glm::vec3 b = bezier3(BP0, BP1, BP2, BP3, t1);
        glm::vec3 mid = (a + b) * 0.5f;
        float len = glm::length(b - a);
        float ang = atan2f(b.y - a.y, b.x - a.x);

        glm::mat4 m = glm::translate(B, mid);
        m = myRotate(m, -ang, glm::vec3(0, 0, 1));

        // Deck slab (20 units wide)
        glm::mat4 dk = glm::scale(m, glm::vec3(len * 1.05f, 0.85f, 20.0f));
        s.setMat4("model", dk);
        s.setVec3("material.ambient", dc * 0.2f); s.setVec3("material.diffuse", dc);
        s.setVec3("material.specular", glm::vec3(0.4f)); s.setFloat("material.shininess", 32);
        s.setBool("isEmissive", false); s.setBool("isWater", false);
        glBindVertexArray(v); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Centre road marking
        glm::mat4 mk = glm::scale(m, glm::vec3(len * 0.55f, 0.9f, 0.28f));
        s.setMat4("model", mk); s.setVec3("material.diffuse", mark);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Railings at Z = ±9.5
        glm::mat4 lr = glm::translate(m, glm::vec3(0, 1.1f, -9.5f));
        lr = glm::scale(lr, glm::vec3(len, 2.0f, 0.3f));
        s.setMat4("model", lr); s.setVec3("material.diffuse", rail);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glm::mat4 rr = glm::translate(m, glm::vec3(0, 1.1f, 9.5f));
        rr = glm::scale(rr, glm::vec3(len, 2.0f, 0.3f));
        s.setMat4("model", rr); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    // ─ Tied arches above deck (semicircular, at Z = ±9.5) ─
    float archApex = 26.0f;
    int archSegs = 36;
    for (int side = -1; side <= 1; side += 2) {
        float archZ = side * 9.5f;
        for (int i = 0; i < archSegs; i++) {
            float a0 = PI * i / archSegs, a1 = PI * (i + 1) / archSegs;
            float x0 = -RIV_HW * cosf(PI - a0), y0 = HWY_Y + archApex * sinf(a0);
            float x1 = -RIV_HW * cosf(PI - a1), y1 = HWY_Y + archApex * sinf(a1);
            float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
            float segLen = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            float ang = atan2f(y1 - y0, x1 - x0);

            glm::mat4 am = glm::translate(B, glm::vec3(mx, my, archZ));
            am = myRotate(am, -ang, glm::vec3(0, 0, 1));
            am = glm::scale(am, glm::vec3(segLen * 1.02f, 1.2f, 1.2f));
            s.setMat4("model", am);
            s.setVec3("material.ambient", arch * 0.25f); s.setVec3("material.diffuse", arch);
            s.setVec3("material.specular", glm::vec3(0.5f)); s.setFloat("material.shininess", 64);
            glBindVertexArray(v); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }

    // ─ Suspender cables (from arch down to deck edge) ─
    for (float px = -(RIV_HW - 2.0f); px <= (RIV_HW - 2.0f); px += 4.5f) {
        float t_arch = acosf(glm::clamp(-px / RIV_HW, -1.0f, 1.0f));
        float archY = HWY_Y + archApex * sinf(t_arch);
        float deckY = bridgeDeckY(px);
        float cH = archY - deckY;
        if (cH < 1.0f) continue;
        drawCylinder(s, B, glm::vec3(px, deckY, -9.5f), 0.1f, cH, cable);
        drawCylinder(s, B, glm::vec3(px, deckY, 9.5f), 0.1f, cH, cable);
    }

    // ─ Bridge entry pillars at river edges ─
    glm::vec3 pilCol(0.48f, 0.45f, 0.42f);
    for (float side : {-1.0f, 1.0f}) {
        float px = side * RIV_HW;
        drawCube(v, s, B, glm::vec3(px, HWY_Y * 0.5f, -6.5f), glm::vec3(4.5f, HWY_Y, 3.5f), pilCol);
        drawCube(v, s, B, glm::vec3(px, HWY_Y * 0.5f, 6.5f), glm::vec3(4.5f, HWY_Y, 3.5f), pilCol);
    }
}

// ── River (wide, animated) ──
void drawRiver(unsigned int v, Shader& s, glm::mat4 B)
{
    s.setBool("isWater", true); s.setFloat("time", gTime);
    glm::mat4 m = glm::translate(B, glm::vec3(0, -0.35f, 0));
    m = glm::scale(m, glm::vec3(RIV_HW * 2, 0.5f, 180));
    s.setMat4("model", m);
    s.setVec3("material.ambient", glm::vec3(0.05f, 0.15f, 0.35f));
    s.setVec3("material.diffuse", glm::vec3(0.1f, 0.35f, 0.65f));
    s.setVec3("material.specular", glm::vec3(0.6f));
    s.setFloat("material.shininess", 128);
    s.setBool("isEmissive", false);
    glBindVertexArray(v); glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    s.setBool("isWater", false);
    // River bank retaining walls
    glm::vec3 rw(0.58f, 0.55f, 0.5f);
    drawCube(v, s, B, glm::vec3(-RIV_HW, -0.1f, 0), glm::vec3(0.7f, 0.7f, 180), rw);
    drawCube(v, s, B, glm::vec3(RIV_HW, -0.1f, 0), glm::vec3(0.7f, 0.7f, 180), rw);
}

// ════════════════════════════════════════════════════════════
//  Vehicle & Boat init / update
// ════════════════════════════════════════════════════════════
void initVehicles()
{
    // 12 vehicles per lane, spread across ±SCENE_X
    for (int i = 0; i < 12; i++) {
        Vehicle veh; veh.type = i % 5; veh.lane = 0; veh.dir = 1;
        veh.speed = 10.0f + (i % 3) * 2.0f; veh.x = -130.0f + i * 22.0f;
        vehicles.push_back(veh);
    }
    for (int i = 0; i < 12; i++) {
        Vehicle veh; veh.type = (i + 2) % 5; veh.lane = 1; veh.dir = -1;
        veh.speed = 9.0f + (i % 3) * 1.8f; veh.x = 130.0f - i * 22.0f;
        vehicles.push_back(veh);
    }
    // 6 boats on river
    for (int i = 0; i < 6; i++) {
        Boat b; b.dir = (i % 2 == 0) ? 1 : -1;
        b.speed = 2.5f + i * 0.5f; b.z = -70.0f + i * 28.0f;
        b.xOff = -10.0f + (i % 3) * 10.0f;
        boats.push_back(b);
    }
}
void updateVehicles(float dt)
{
    trafficGreen = fmodf(gTime, 8) < 4;
    float stopLines[] = { -112,-70,-30,30,70,112 };
    for (auto& veh : vehicles) {
        float nx = veh.x + veh.dir * veh.speed * dt;
        // Slow down approaching bridge (river zone)
        if (veh.dir > 0 && veh.x > 17 && veh.x < 25) nx = veh.x + veh.dir * veh.speed * dt * 0.3f;
        if (veh.dir < 0 && veh.x<-17 && veh.x>-25) nx = veh.x + veh.dir * veh.speed * dt * 0.3f;
        // Traffic light stops
        if (!trafficGreen) {
            for (float sl : stopLines) {
                if (veh.dir > 0 && veh.x < sl && nx >= sl) { nx = sl; break; }
                if (veh.dir<0 && veh.x>sl && nx <= sl) { nx = sl; break; }
            }
        }
        veh.x = nx;
        if (veh.x > SCENE_X + 10) veh.x = -(SCENE_X + 10);
        if (veh.x < -(SCENE_X + 10)) veh.x = SCENE_X + 10;
    }
    for (auto& b : boats) {
        b.z += b.dir * b.speed * dt;
        if (b.z > 110) b.z = -110; if (b.z < -110) b.z = 110;
    }
}

// ════════════════════════════════════════════════════════════
//  Point lights (street-light positions on highway deck)
// ════════════════════════════════════════════════════════════
void setupPointLights()
{
    float pos[][3] = {
        {-110,HWY_Y + 15, 10}, { 110,HWY_Y + 15, 10},
        { -80,HWY_Y + 15, 10}, {  80,HWY_Y + 15, 10},
        {-110,HWY_Y + 15,-10}, { 110,HWY_Y + 15,-10},
        { -80,HWY_Y + 15,-10}, {  80,HWY_Y + 15,-10}
    };
    for (int i = 0; i < 8; i++)
        pointLights[i] = PointLight(pos[i][0], pos[i][1], pos[i][2],
            0.05f, 0.05f, 0.04f, 0.6f, 0.55f, 0.4f, 0.8f, 0.8f, 0.7f,
            1, 0.014f, 0.002f, i + 1);
}

// ════════════════════════════════════════════════════════════
//  Render full scene
// ════════════════════════════════════════════════════════════
void renderScene(unsigned int vao, Shader& sh, glm::mat4 proj, glm::mat4 view, int vpMode)
{
    sh.use();
    sh.setMat4("projection", proj); sh.setMat4("view", view);
    sh.setVec3("viewPos", camera.Position); sh.setFloat("time", gTime);
    sh.setInt("viewportMode", vpMode);
    sh.setBool("dirLightOn", dirLightOn); sh.setBool("pointLightsOn", pointLightsOn);
    sh.setBool("spotLightOn", spotLightOn); sh.setBool("ambientOn", ambientOn);
    sh.setBool("diffuseOn", diffuseOn); sh.setBool("specularOn", specularOn);
    sh.setBool("isWater", false); sh.setBool("isEmissive", false);

    dirLight.setUpLight(sh);
    for (int i = 0; i < numPL; i++) pointLights[i].setUpPointLight(sh);
    spotLight.setUpLight(sh);
    glm::mat4 B(1);

    // ─ Infrastructure ─
    drawGround(vao, sh, B);
    drawHighway(vao, sh, B);
    drawRiver(vao, sh, B);
    drawBridge(vao, sh, B);

    // Toll booths at both ends of bridge
    drawTollBooth(vao, sh, B, glm::vec3(-25, 0, 0));
    drawTollBooth(vao, sh, B, glm::vec3(25, 0, 0));

    // ─ City 1  (Z > 0) – safe inter-building gaps ─
    // JFP right edge ≈ X=-90 | Hotel left edge ≈ X=-80 → 10-unit gap
    // Hotel right edge ≈ X=-60 | Stadium left edge ≈ X=-48 → 12-unit gap
    drawJFP(vao, sh, B, glm::vec3(-112, 0, C1Z));
    drawHotel(vao, sh, B, glm::vec3(-70, 0, C1Z));
    drawCricketStadium(vao, sh, B, glm::vec3(-30, 0, C1Z + 8));
    drawApartment(vao, sh, B, glm::vec3(46, 0, C1Z), 38, glm::vec3(0.72f, 0.62f, 0.52f));
    drawApartment(vao, sh, B, glm::vec3(82, 0, C1Z), 50, glm::vec3(0.65f, 0.60f, 0.55f));
    drawApartment(vao, sh, B, glm::vec3(116, 0, C1Z), 30, glm::vec3(0.70f, 0.58f, 0.50f));

    // ─ City 2  (Z < 0) – safe inter-building gaps ─
    // Corp right edge ≈ X=-105 | Uni left wing ≈ X=-93 → 12-unit gap
    // Uni right wing ≈ X=-43   | Hospital left ≈ X=-37 → 6-unit gap
    drawCorporateOffice(vao, sh, B, glm::vec3(-115, 0, C2Z));
    drawUniversity(vao, sh, B, glm::vec3(-68, 0, C2Z));
    drawHospital(vao, sh, B, glm::vec3(-22, 0, C2Z));
    drawPark(vao, sh, B, glm::vec3(46, 0, C2Z));
    drawEiffelTower(vao, sh, B, glm::vec3(82, 0, C2Z));
    drawGarden(vao, sh, B, glm::vec3(116, 0, C2Z));

    // ─ Trees on highway deck edges ─
    for (float x = -140; x <= 140; x += 12.0f) {
        if (x > -(RIV_HW + 4) && x < (RIV_HW + 4)) continue;
        drawTree(vao, sh, B, glm::vec3(x, HWY_Y, 10.5f), 1.8f);
        drawTree(vao, sh, B, glm::vec3(x, HWY_Y, -10.5f), 1.8f);
    }

    // ─ Street lights on highway deck ─
    for (float x = -140; x <= 140; x += 22.0f) {
        if (x > -(RIV_HW + 5) && x < (RIV_HW + 5)) continue;
        drawStreetLight(vao, sh, B, glm::vec3(x, HWY_Y, 11.0f));
        drawStreetLight(vao, sh, B, glm::vec3(x, HWY_Y, -11.0f));
    }

    // ─ Traffic lights at connecting road intersections ─
    float tlX[] = { -112,-70,-30,30,70,112 };
    for (float tx : tlX) {
        drawTrafficLight(vao, sh, B, glm::vec3(tx + 3, HWY_Y, 7.5f));
        drawTrafficLight(vao, sh, B, glm::vec3(tx + 3, HWY_Y, -7.5f));
    }

    // ─ City signboards at highway entry points ─
    drawSignboard(vao, sh, B, glm::vec3(-135, HWY_Y, 11), glm::vec3(0.2f, 0.5f, 0.8f));
    drawSignboard(vao, sh, B, glm::vec3(135, HWY_Y, 11), glm::vec3(0.8f, 0.5f, 0.2f));
    drawSignboard(vao, sh, B, glm::vec3(-135, HWY_Y, -11), glm::vec3(0.5f, 0.2f, 0.6f));
    drawSignboard(vao, sh, B, glm::vec3(135, HWY_Y, -11), glm::vec3(0.3f, 0.6f, 0.3f));

    // ─ Vehicles on highway ─
    for (auto& veh : vehicles) {
        float lz = (veh.lane == 0) ? 3.5f : -3.5f;
        float vy = bridgeDeckY(veh.x) + 0.35f;
        drawVehicle(vao, sh, B, glm::vec3(veh.x, vy, lz), veh.type, veh.dir);
    }

    // ─ Boats on river ─
    for (auto& b : boats)
        drawBoat(vao, sh, B, glm::vec3(b.xOff, -0.12f, b.z), b.dir);

    // ─ Sun / Moon  (positioned to be visible in default camera view) ─
    sh.setBool("isEmissive", true);
    if (!isNight)
        drawSphere(sh, B, glm::vec3(120, 130, -120), 22.0f, glm::vec3(1, 0.95f, 0.40f), true);
    else
        drawSphere(sh, B, glm::vec3(-100, 110, 110), 16.0f, glm::vec3(0.85f, 0.88f, 0.95f), true);
    sh.setBool("isEmissive", false);
}

// ════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H,
        "3D Smart City – CSE 4208 Computer Graphics", NULL, NULL);
    if (!window) { cout << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { cout << "GLAD init failed\n"; return -1; }
    glEnable(GL_DEPTH_TEST);

    Shader lightSh("vertexShaderForPhongShading.vs", "fragmentShaderForPhongShading.fs");
    Shader flatSh("vertexShader.vs", "fragmentShader.fs");
    unsigned int cubeVAO = createCubeVAO();
    buildCylinder(28);
    buildSphere(20, 32);
    setupPointLights();
    initVehicles();

    // Spot light aimed at cricket stadium
    spotLight.position = glm::vec3(-30, 55, C1Z + 8);
    spotLight.direction = glm::vec3(0, -1, 0);
    spotLight.cutOff = cosf(glm::radians(22.0f));
    spotLight.outerCutOff = cosf(glm::radians(32.0f));

    cout << "============================================================\n"
        << "   3D SMART CITY ENVIRONMENT SIMULATION\n"
        << "   CSE 4208 - Computer Graphics\n"
        << "============================================================\n\n"
        << "  CAMERA MOVEMENT:\n"
        << "    W / S        Forward / Backward\n"
        << "    A / D        Left / Right\n"
        << "    E / R        Up / Down\n\n"
        << "  CAMERA ROTATION:\n"
        << "    X            Pitch       (Shift+X = inverse)\n"
        << "    Y            Yaw         (Shift+Y = inverse)\n"
        << "    Z            Roll        (Shift+Z = inverse)\n"
        << "    F            Orbit around target\n"
        << "    Scroll       Zoom (FOV)\n\n"
        << "  LIGHTING:\n"
        << "    1  Directional   2  Point Lights   3  Spot Light\n"
        << "    5  Ambient       6  Diffuse         7  Specular\n\n"
        << "  SCENE:\n"
        << "    L            Day / Night toggle\n"
        << "    4            4-Viewport split toggle\n"
        << "    ESC          Exit\n"
        << "============================================================\n\n";

    while (!glfwWindowShouldClose(window))
    {
        float cf = (float)glfwGetTime();
        deltaTime = cf - lastFrame; lastFrame = cf; gTime = cf;
        processInput(window);
        updateVehicles(deltaTime);

        glClearColor(isNight ? 0.02f : 0.53f, isNight ? 0.02f : 0.81f, isNight ? 0.08f : 0.92f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glm::mat4 view = camera.GetViewMatrix();

        if (!splitViewport)
        {
            glViewport(0, 0, fbW, fbH);
            glm::mat4 proj = myPerspective(glm::radians(camera.Zoom), (float)fbW / fbH, 0.5f, 1500);
            renderScene(cubeVAO, lightSh, proj, view, 0);
        }
        else
        {
            int hW = fbW / 2, hH = fbH / 2;
            glm::mat4 proj = myPerspective(glm::radians(camera.Zoom), (float)hW / hH, 0.5f, 1500);
            // Top-left: combined
            glViewport(0, hH, hW, hH);
            renderScene(cubeVAO, lightSh, proj, view, 0);
            // Top-right: ambient only (top-down view)
            glViewport(hW, hH, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(0, 250, 0.1f), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1)), 1);
            // Bottom-left: diffuse only (front view)
            glViewport(0, 0, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(0, 40, 200), glm::vec3(0, 20, 0), glm::vec3(0, 1, 0)), 2);
            // Bottom-right: directional only (isometric)
            glViewport(hW, 0, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(180, 140, 180), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)), 3);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &cubeVAO);
    glfwTerminate();
    return 0;
}

// ════════════════════════════════════════════════════════════
//  Input
// ════════════════════════════════════════════════════════════
void processInput(GLFWwindow* w)
{
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true);
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) camera.MoveForward(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) camera.MoveBackward(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) camera.MoveLeft(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) camera.MoveRight(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) camera.MoveUp(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS) camera.MoveDown(deltaTime);
    float ra = camera.RotSpeed * deltaTime;
    bool sh = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) camera.RotatePitch(sh ? ra : -ra);
    if (glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) camera.RotateYaw(sh ? -ra : ra);
    if (glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) camera.RotateRoll(sh ? -ra : ra);
    if (glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS) camera.OrbitLeft(deltaTime);
}
void key_callback(GLFWwindow*, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    switch (key) {
    case GLFW_KEY_1: dirLightOn = !dirLightOn;    break;
    case GLFW_KEY_2: pointLightsOn = !pointLightsOn; break;
    case GLFW_KEY_3: spotLightOn = !spotLightOn;   break;
    case GLFW_KEY_4: splitViewport = !splitViewport; break;
    case GLFW_KEY_5: ambientOn = !ambientOn;     break;
    case GLFW_KEY_6: diffuseOn = !diffuseOn;     break;
    case GLFW_KEY_7: specularOn = !specularOn;     break;
    case GLFW_KEY_L:
        isNight = !isNight;
        if (isNight) dirLight.setNight(); else dirLight.setDay();
        break;
    }
}
void framebuffer_size_callback(GLFWwindow*, int, int) {}
void scroll_callback(GLFWwindow*, double, double y) { camera.ProcessMouseScroll((float)y); }
