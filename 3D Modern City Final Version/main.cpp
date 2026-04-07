// main.cpp
// CSE 4208 Computer Graphics: 3D smart city scene (OpenGL 3.3 core, Phong shading, textures).
// World units: approximately 1 unit = 3 m. Scene half-extents SCENE_X, SCENE_Z; highway HWY_Y;
// river RIV_HW; two urban areas referenced by C1Z and C2Z. Major structures include stadium (outer
// radius 50), Eiffel-style tower, and Burj-style tower; proportions follow assignment scale notes.

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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

// Prototypes
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

// Constants (world units; scale roughly 1 unit = 3 m).
const unsigned int SCR_W = 1600, SCR_H = 900;
const float PI = 3.14159265358979f;

const float HWY_Y = 30.0f;   // highway deck elevation
const float HWY_HW = 22.0f;   // highway half-width in Z
const float RIV_HW = 20.0f;   // river half-width in X
const float SCENE_X = 500.0f; const float SCENE_Z = 400.0f;  // scene half-extent in X
// Grass plane span uses this factor on SCENE_* so the playable ground extends past nominal bounds; river Z matches.
const float GROUND_PLANE_SCALE = 2.2f;
const float RIVER_Z_HALF = SCENE_Z * GROUND_PLANE_SCALE * 0.5f;
const float C1Z = 65.0f;  // city-1 building centre Z
const float C2Z = -65.0f;  // city-2 building centre Z

// Main river bridge deck: half-width along Z, slab thickness; driving surface Y matches former thin deck top.
const float BRIDGE_DECK_HALF_Z = 22.0f * 1.5f;
const float BRIDGE_DECK_THICK = 0.8f * 4.0f;
const float BRIDGE_SURFACE_Y = HWY_Y + 0.4f;
const float BRIDGE_DECK_CENTER_Y = BRIDGE_SURFACE_Y - 0.5f * BRIDGE_DECK_THICK;
const float BRIDGE_CABLE_Z = BRIDGE_DECK_HALF_Z + 2.0f;
const float BRIDGE_LANE_Z = 14.0f * 1.5f;
const float HWY_APPROACH_LANE_Z = 12.0f;
const float BRIDGE_APPROACH_TREE_Z = 30.0f * 2.0f;
const float RIVER_BANK_TREE_Z_PUSH = (BRIDGE_DECK_HALF_Z - 22.0f) * 2.0f;
// First city rows (|Z| was 80): shift along +/-Z so large footprints (stadium, towers) clear the bridge deck in Z.
const float CITY_ROW1_Z_PUSH = 52.0f;
const float Z_ROW1_POS = 80.0f + CITY_ROW1_Z_PUSH;
const float Z_ROW1_NEG = -80.0f - CITY_ROW1_Z_PUSH;
// Equal center-to-center spacing along +/-Z for city rows 1-3 (row1 fixed for bridge clearance).
const float CITY_ROW_Z_GAP = 130.0f;
const float Z_ROW2_POS = Z_ROW1_POS + CITY_ROW_Z_GAP;
const float Z_ROW3_POS = Z_ROW2_POS + CITY_ROW_Z_GAP;
const float Z_ROW2_NEG = Z_ROW1_NEG - CITY_ROW_Z_GAP;
const float Z_ROW3_NEG = Z_ROW2_NEG - CITY_ROW_Z_GAP;

// Bridge deck Bezier control points
const glm::vec3 BP0(-20, HWY_Y, 0);
const glm::vec3 BP1(-7, HWY_Y + 13.0f, 0);
const glm::vec3 BP2(7, HWY_Y + 13.0f, 0);
const glm::vec3 BP3(20, HWY_Y, 0);

// Camera: default position and look-at target (street view toward the bridge).
Camera camera(glm::vec3(-490.0f, 33.0f, 0.0f), glm::vec3(0.0f, 33.0f, 0.0f));

// Lighting
DirectionalLight dirLight;
SpotLight spotLights[12];
int numSpotLights = 12;
PointLight pointLights[8];
int numPL = 8;
bool dirLightOn = true, pointLightsOn = true, spotLightOn = true;
bool ambientOn = true, diffuseOn = true, specularOn = true;
bool isNight = false;

// Timing / state
float deltaTime = 0, lastFrame = 0, gTime = 0;
// Aeroplane world-X from gTime * kAeroplanePathSpeed in renderScene.
static constexpr float kAeroplanePathSpeed = 140.0f;
bool trafficGreen = true;
bool splitViewport = false;

// Geometry IDs
unsigned int cylVAO, cylVBO, cylEBO; int cylIC;
unsigned int sphVAO, sphVBO, sphEBO; int sphIC;

// Vehicles / Boats
struct Vehicle { float x; int type; float speed; int lane; int dir; };
struct Boat { float z; float speed; int dir; float xOff; };
vector<Vehicle> vehicles;
vector<Boat>    boats;

struct Ship { float z; float speed; int dir; float xOff; int type; };
std::vector<Ship> ships;
int viewModeKey8 = 3;   // 3=Normal camera  0=Iso  1=Top  2=Front
// texMode (key 9): 0 off, 1 simple texture, 2 blend, 3 vertex color, 4 fragment color.
// Default texMode is 1 (simple texture).
int texMode = 1;
// OpenGL texture names (loaded in main).
unsigned int texDaySky = 0, texNightSky = 0, texSun = 0, texMoon = 0;
unsigned int texStem = 0, texTree = 0;
unsigned int texBridgeRoad = 0, texPillar = 0, texCable = 0, texRiver = 0;
unsigned int texCargo = 0, texHarmony = 0, texEmirates = 0;
unsigned int texStadium = 0, texStadiumIn = 0;
unsigned int texEiffel = 0, texEiffelBlk = 0;
unsigned int texBurj = 0, texBurjBlk = 0;
unsigned int texCorp = 0, texCorpBlk = 0;
unsigned int texMall = 0, texMallBlk = 0;
unsigned int texUniv = 0, texUnivBlk = 0;
unsigned int texResid = 0, texResidBlk = 0;
unsigned int texHosp = 0, texHospBlk = 0;
unsigned int texGrass = 0;

// Custom math
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
    if (x > -RIV_HW && x < RIV_HW)
        return BRIDGE_SURFACE_Y;
    return HWY_Y;
}

static float vehicleLaneOffsetZ(float x, int lane)
{
    float half = (x > -RIV_HW && x < RIV_HW) ? BRIDGE_LANE_Z : HWY_APPROACH_LANE_Z;
    return (lane == 0) ? half : -half;
}

// Geometry builders
unsigned int createCubeVAO()
{
    // pos(3) + normal(3) + uv(2) = 8 floats, stride=32
    float v[] = {
        // back face  n=(0,0,-1)
        -.5f,-.5f,-.5f, 0,0,-1, 1,0,   .5f,-.5f,-.5f, 0,0,-1, 0,0,
         .5f, .5f,-.5f, 0,0,-1, 0,1,  -.5f, .5f,-.5f, 0,0,-1, 1,1,
         // right face n=(1,0,0)
          .5f,-.5f,-.5f, 1,0,0,  0,0,   .5f, .5f,-.5f, 1,0,0,  0,1,
          .5f,-.5f, .5f, 1,0,0,  1,0,   .5f, .5f, .5f, 1,0,0,  1,1,
          // front face n=(0,0,1)
          -.5f,-.5f, .5f, 0,0,1,  0,0,   .5f,-.5f, .5f, 0,0,1,  1,0,
           .5f, .5f, .5f, 0,0,1,  1,1,  -.5f, .5f, .5f, 0,0,1,  0,1,
           // left face  n=(-1,0,0)
           -.5f,-.5f, .5f,-1,0,0,  1,0,  -.5f, .5f, .5f,-1,0,0,  1,1,
           -.5f, .5f,-.5f,-1,0,0,  0,1,  -.5f,-.5f,-.5f,-1,0,0,  0,0,
           // top face   n=(0,1,0)
            .5f, .5f, .5f, 0,1,0,  1,0,   .5f, .5f,-.5f, 0,1,0,  1,1,
           -.5f, .5f,-.5f, 0,1,0,  0,1,  -.5f, .5f, .5f, 0,1,0,  0,0,
           // bottom face n=(0,-1,0)
           -.5f,-.5f,-.5f, 0,-1,0, 0,0,   .5f,-.5f,-.5f, 0,-1,0, 1,0,
            .5f,-.5f, .5f, 0,-1,0, 1,1,  -.5f,-.5f, .5f, 0,-1,0, 0,1
    };
    unsigned int i[] = { 0,3,2,2,1,0, 4,5,7,7,6,4, 8,9,10,10,11,8,
                      12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20 };
    unsigned int V, B, E;
    glGenVertexArrays(1, &V); glGenBuffers(1, &B); glGenBuffers(1, &E);
    glBindVertexArray(V);
    glBindBuffer(GL_ARRAY_BUFFER, B); glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, E); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24); glEnableVertexAttribArray(2);
    glBindVertexArray(0); return V;
}
void buildCylinder(int seg)
{
    // pos(3)+normal(3)+uv(2)=8 floats, stride=32
    vector<float> vt; vector<unsigned int> id; float d = 2 * PI / seg;
    for (int i = 0; i <= seg; i++) {
        float a = i * d, x = cosf(a), z = sinf(a);
        float u = (float)i / seg;
        // bottom vertex
        vt.insert(vt.end(), { x,0,z, x,0,z, u,0.0f });
        // top vertex
        vt.insert(vt.end(), { x,1,z, x,0,z, u,1.0f });
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}
void buildSphere(int rings, int sectors)
{
    // pos(3)+normal(3)+uv(2)=8 floats, stride=32
    vector<float> vt; vector<unsigned int> id;
    for (int r = 0; r <= rings; r++) for (int s = 0; s <= sectors; s++) {
        float y = sinf(-PI / 2 + PI * r / (float)rings);
        float x = cosf(2 * PI * s / (float)sectors) * sinf(PI * r / (float)rings);
        float z = sinf(2 * PI * s / (float)sectors) * sinf(PI * r / (float)rings);
        float u = (float)s / sectors, v = (float)r / rings;
        vt.insert(vt.end(), { x,y,z, x,y,z, u,v });
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

// Drawing helpers
// Current texture state (set by setObjTex / clearObjTex in renderScene)
unsigned int g_boundTex = 0;

void drawCube(unsigned int vao, Shader& s, glm::mat4 P,
    glm::vec3 pos, glm::vec3 sc, glm::vec3 col, float rotY)
{
    glm::mat4 m = glm::translate(P, pos);
    if (rotY != 0) m = myRotate(m, glm::radians(rotY), glm::vec3(0, 1, 0));
    m = glm::scale(m, sc);
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.55f);
    s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.55f));
    s.setFloat("material.shininess", 48.0f);
    s.setBool("isEmissive", false);
    s.setBool("isWater", false);
    // Texture support
    bool useTex = (texMode > 0 && g_boundTex != 0);
    s.setBool("useTexture", useTex);
    s.setInt("texMode", texMode);
    if (useTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_boundTex);
        s.setInt("texSampler", 0);
    }
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}
void drawCylinder(Shader& s, glm::mat4 P, glm::vec3 pos, float r, float h, glm::vec3 col)
{
    glm::mat4 m = glm::translate(P, pos);
    m = glm::scale(m, glm::vec3(r, h, r));
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.55f); s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.5f)); s.setFloat("material.shininess", 32);
    s.setBool("isEmissive", false); s.setBool("isWater", false);
    bool useTex = (texMode > 0 && g_boundTex != 0);
    s.setBool("useTexture", useTex); s.setInt("texMode", texMode);
    if (useTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_boundTex); s.setInt("texSampler", 0); }
    glBindVertexArray(cylVAO); glDrawElements(GL_TRIANGLES, cylIC, GL_UNSIGNED_INT, 0);
}
void drawSphere(Shader& s, glm::mat4 P, glm::vec3 pos, float r, glm::vec3 col, bool em)
{
    glm::mat4 m = glm::translate(P, pos);
    m = glm::scale(m, glm::vec3(r));
    s.setMat4("model", m);
    s.setVec3("material.ambient", col * 0.55f); s.setVec3("material.diffuse", col);
    s.setVec3("material.specular", glm::vec3(0.6f)); s.setFloat("material.shininess", 80);
    s.setBool("isEmissive", em); s.setBool("isWater", false);
    bool useTex = (texMode > 0 && g_boundTex != 0);
    s.setBool("useTexture", useTex); s.setInt("texMode", texMode);
    if (useTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_boundTex); s.setInt("texSampler", 0); }
    glBindVertexArray(sphVAO); glDrawElements(GL_TRIANGLES, sphIC, GL_UNSIGNED_INT, 0);
}

// Window grid helper
void drawWindowGrid(unsigned int vao, Shader& s, glm::mat4 B, glm::vec3 bldg,
    int axis, float face, float yLo, float yHi,
    float uLo, float uHi, float yStep, float uStep)
{
    glm::vec3 wc(0.65f, 0.85f, 0.98f);
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

// Small scene objects

// Tree: recursive stem (cylinder) and three child branches per level; depth 0 draws a textured leaf sphere.
void drawFractalTree(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float len, float width, int depth) {
    glm::vec3 stemCol(0.40f, 0.25f, 0.10f), leafCol(0.08f, 0.55f, 0.14f);
    if (depth == 0) {
        g_boundTex = texTree;
        drawSphere(s, B, p + glm::vec3(0, len, 0), width * 12.5f, leafCol);
        g_boundTex = 0;
        return;
    }
    g_boundTex = texStem;
    drawCylinder(s, B, p, width, len, stemCol);
    g_boundTex = 0;

    glm::mat4 nB = glm::translate(B, p + glm::vec3(0, len, 0));

    glm::mat4 b1 = myRotate(nB, glm::radians(35.0f), glm::vec3(0, 0, 1));
    drawFractalTree(v, s, b1, glm::vec3(0), len * 0.70f, width * 0.62f, depth - 1);
    glm::mat4 b2 = myRotate(nB, glm::radians(-30.0f), glm::vec3(1, 0, 0));
    drawFractalTree(v, s, b2, glm::vec3(0), len * 0.70f, width * 0.62f, depth - 1);
    glm::mat4 b3 = myRotate(nB, glm::radians(30.0f), glm::vec3(0, 1, 0));
    b3 = myRotate(b3, glm::radians(25.0f), glm::vec3(1, 0, 0));
    drawFractalTree(v, s, b3, glm::vec3(0), len * 0.70f, width * 0.62f, depth - 1);
}

// Entry point for placed trees: fractal parameters and default canopy radius.
void drawTree(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float canopyR = 3.0f) {
    drawFractalTree(v, s, B, p, 5.8f + canopyR, 0.58f, 3);
}


// Street-light (tall pole + horizontal arm + lamp head)
void drawStreetLight(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    drawCylinder(s, B, p, 0.14f, 14.0f, glm::vec3(0.55f, 0.55f, 0.58f));
    drawCube(v, s, B, p + glm::vec3(1.5f, 14.0f, 0), glm::vec3(3.2f, 0.15f, 0.15f), glm::vec3(0.55f));
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

// Highway vehicles: motion along +X / -X; mesh scaled so local forward maps to world X after Y rotation.
void drawVehicle(unsigned int v, Shader& s, glm::mat4 B,
    glm::vec3 pos, int type, int dir)
{
    // 0° = faces +X (dir>0), 180° = faces -X (dir<0)
    float rotY = (dir > 0) ? 0.0f : 180.0f;
    glm::mat4 vm = glm::translate(B, pos);
    // Scale: longer in Z than X or Y for a road vehicle silhouette.
    vm = glm::scale(vm, glm::vec3(2.2f, 2.0f, 3.8f));
    vm = myRotate(vm, glm::radians(rotY), glm::vec3(0, 1, 0));

    glm::vec3 body, top;
    glm::vec3 bsz, tsz, toff;
    switch (type) {
    case 0: body = glm::vec3(0.18f, 0.48f, 0.92f); top = glm::vec3(0.65f, 0.85f, 0.98f);
        bsz = glm::vec3(2.8f, 0.95f, 1.35f); tsz = glm::vec3(1.4f, 0.75f, 1.25f); toff = glm::vec3(-0.1f, 0.82f, 0); break;
    case 1: body = glm::vec3(0.95f, 0.78f, 0.15f); top = glm::vec3(0.98f, 0.92f, 0.45f);
        bsz = glm::vec3(5.2f, 1.90f, 1.55f); tsz = glm::vec3(4.8f, 1.50f, 1.45f); toff = glm::vec3(0, 1.55f, 0); break;
    case 2: body = glm::vec3(0.72f, 0.70f, 0.68f); top = glm::vec3(0.55f, 0.52f, 0.50f);
        bsz = glm::vec3(4.5f, 1.35f, 1.45f); tsz = glm::vec3(2.2f, 1.90f, 1.35f); toff = glm::vec3(-0.7f, 1.55f, 0); break;
    case 3: body = glm::vec3(0.98f, 0.98f, 0.98f); top = glm::vec3(0.95f, 0.22f, 0.22f);
        bsz = glm::vec3(3.8f, 1.35f, 1.45f); tsz = glm::vec3(2.0f, 0.70f, 1.35f); toff = glm::vec3(0.55f, 1.0f, 0); break;
    default:body = glm::vec3(0.95f, 0.18f, 0.10f); top = glm::vec3(0.82f, 0.12f, 0.06f);
        bsz = glm::vec3(4.8f, 1.40f, 1.50f); tsz = glm::vec3(3.0f, 1.10f, 1.40f); toff = glm::vec3(-0.5f, 1.28f, 0); break;
    }

    // Body and cabin.
    drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.5f, 0), bsz, body);
    drawCube(v, s, vm, toff + glm::vec3(0, bsz.y * 0.5f, 0), tsz, top);

    // Wheels.
    glm::vec3 wc(0.12f, 0.12f, 0.12f); glm::vec3 ws(0.50f, 0.50f, 0.22f);
    float wx = bsz.x * 0.35f, wz = bsz.z * 0.5f + 0.08f;
    drawCube(v, s, vm, glm::vec3(-wx, 0.22f, wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(wx, 0.22f, wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(-wx, 0.22f, -wz), ws, wc);
    drawCube(v, s, vm, glm::vec3(wx, 0.22f, -wz), ws, wc);

    // Windshield.
    float fx = bsz.x * 0.48f;
    drawCube(v, s, vm, glm::vec3(fx, bsz.y * 0.7f, 0),
        glm::vec3(0.08f, bsz.y * 0.5f, bsz.z * 0.7f), glm::vec3(0.12f, 0.15f, 0.2f));

    // Headlights and tail lights (night).
    if (isNight) {
        s.setBool("isEmissive", true);
        drawCube(v, s, vm, glm::vec3(fx + 0.02f, bsz.y * 0.45f, -0.28f), glm::vec3(0.06f, 0.18f, 0.18f), glm::vec3(1, 1, 0.78f));
        drawCube(v, s, vm, glm::vec3(fx + 0.02f, bsz.y * 0.45f, 0.28f), glm::vec3(0.06f, 0.18f, 0.18f), glm::vec3(1, 1, 0.78f));
        drawCube(v, s, vm, glm::vec3(-fx - 0.02f, bsz.y * 0.45f, -0.28f), glm::vec3(0.06f, 0.14f, 0.14f), glm::vec3(0.9f, 0.05f, 0.05f));
        drawCube(v, s, vm, glm::vec3(-fx - 0.02f, bsz.y * 0.45f, 0.28f), glm::vec3(0.06f, 0.14f, 0.14f), glm::vec3(0.9f, 0.05f, 0.05f));
        s.setBool("isEmissive", false);
    }

    // Additional geometry per vehicle type.

    if (type == 0)  // Car
    {
        glm::vec3 mirC(0.55f, 0.58f, 0.65f), grimC(0.08f, 0.08f, 0.10f), glassC(0.28f, 0.40f, 0.55f);
        // Front bonnet (slightly raised hood)
        drawCube(v, s, vm, glm::vec3(bsz.x * 0.3f, bsz.y + 0.08f, 0),
            glm::vec3(bsz.x * 0.35f, 0.12f, bsz.z * 0.90f), body * 1.05f);
        // Rear trunk lid
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.3f, bsz.y + 0.06f, 0),
            glm::vec3(bsz.x * 0.3f, 0.10f, bsz.z * 0.88f), body * 0.98f);
        // Side windows (left + right Z-faces of cabin)
        drawCube(v, s, vm, toff + glm::vec3(0, bsz.y * 0.5f + 0.20f, -tsz.z * 0.5f),
            glm::vec3(tsz.x * 0.88f, tsz.y * 0.65f, 0.05f), glassC);
        drawCube(v, s, vm, toff + glm::vec3(0, bsz.y * 0.5f + 0.20f, tsz.z * 0.5f),
            glm::vec3(tsz.x * 0.88f, tsz.y * 0.65f, 0.05f), glassC);
        // Rear windshield
        drawCube(v, s, vm, glm::vec3(-fx, bsz.y * 0.72f, 0),
            glm::vec3(0.08f, bsz.y * 0.48f, bsz.z * 0.68f), glm::vec3(0.12f, 0.15f, 0.20f));
        // Door panel lines (left & right sides)
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.45f, bsz.z * 0.5f + 0.02f),
            glm::vec3(bsz.x * 0.72f, 0.06f, 0.06f), body * 0.78f);
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.45f, -bsz.z * 0.5f - 0.02f),
            glm::vec3(bsz.x * 0.72f, 0.06f, 0.06f), body * 0.78f);
        // Side mirrors
        drawCube(v, s, vm, glm::vec3(fx * 0.6f, bsz.y + 0.12f, bsz.z * 0.5f + 0.12f),
            glm::vec3(0.22f, 0.12f, 0.18f), mirC);
        drawCube(v, s, vm, glm::vec3(fx * 0.6f, bsz.y + 0.12f, -bsz.z * 0.5f - 0.12f),
            glm::vec3(0.22f, 0.12f, 0.18f), mirC);
        // Front grill
        drawCube(v, s, vm, glm::vec3(fx + 0.01f, bsz.y * 0.28f, 0),
            glm::vec3(0.06f, bsz.y * 0.35f, bsz.z * 0.55f), grimC);
        // License plate (rear)
        drawCube(v, s, vm, glm::vec3(-fx - 0.02f, bsz.y * 0.35f, 0),
            glm::vec3(0.04f, 0.18f, 0.38f), glm::vec3(0.92f, 0.88f, 0.80f));
    }
    else if (type == 1)  // School bus
    {
        // Front nose bump
        drawCube(v, s, vm, glm::vec3(bsz.x * 0.5f + 0.2f, bsz.y * 0.5f, 0),
            glm::vec3(0.40f, bsz.y * 0.85f, bsz.z * 0.90f), body * 0.92f);
        // Side window array (~5 panes per side)
        glm::vec3 winC(0.55f, 0.75f, 0.92f);
        for (int wi = 0; wi < 5; wi++) {
            float wx2 = -bsz.x * 0.35f + wi * (bsz.x * 0.72f / 4.0f);
            drawCube(v, s, vm, glm::vec3(wx2, bsz.y + tsz.y * 0.55f, bsz.z * 0.5f + 0.02f),
                glm::vec3(0.6f, tsz.y * 0.65f, 0.05f), winC);
            drawCube(v, s, vm, glm::vec3(wx2, bsz.y + tsz.y * 0.55f, -bsz.z * 0.5f - 0.02f),
                glm::vec3(0.6f, tsz.y * 0.65f, 0.05f), winC);
        }
        // Passenger door (left side rear)
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.28f, bsz.y * 0.6f, bsz.z * 0.5f + 0.02f),
            glm::vec3(0.9f, bsz.y * 0.95f, 0.05f), body * 0.85f);
        // Emergency exit (right side rear)
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.28f, bsz.y * 0.6f, -bsz.z * 0.5f - 0.02f),
            glm::vec3(0.9f, bsz.y * 0.95f, 0.05f), body * 0.80f);
        // Stop sign arm (left side, small red cube)
        drawCube(v, s, vm, glm::vec3(-0.5f, bsz.y * 0.7f, bsz.z * 0.5f + 0.35f),
            glm::vec3(0.55f, 0.55f, 0.08f), glm::vec3(0.92f, 0.12f, 0.10f));
        // Bumper bars (front + rear)
        drawCube(v, s, vm, glm::vec3(fx + 0.08f, 0.18f, 0), glm::vec3(0.12f, 0.22f, bsz.z * 0.95f), glm::vec3(0.25f));
        drawCube(v, s, vm, glm::vec3(-fx - 0.08f, 0.18f, 0), glm::vec3(0.12f, 0.22f, bsz.z * 0.95f), glm::vec3(0.25f));
        // Yellow side stripe
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.22f, bsz.z * 0.5f + 0.01f),
            glm::vec3(bsz.x * 0.98f, 0.10f, 0.04f), glm::vec3(0.95f, 0.85f, 0.10f));
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.22f, -bsz.z * 0.5f - 0.01f),
            glm::vec3(bsz.x * 0.98f, 0.10f, 0.04f), glm::vec3(0.95f, 0.85f, 0.10f));
        // Roof warning lights (red emissive strip at night)
        s.setBool("isEmissive", isNight);
        glm::vec3 wlC = isNight ? glm::vec3(1, 0.05f, 0.05f) : glm::vec3(0.55f, 0.10f, 0.10f);
        drawCube(v, s, vm, glm::vec3(bsz.x * 0.25f, bsz.y + tsz.y + 0.15f, 0),
            glm::vec3(1.2f, 0.18f, bsz.z * 0.5f), wlC);
        s.setBool("isEmissive", false);
    }
    else if (type == 2)  // Cargo truck
    {
        // Distinct cabin (front 35%) vs cargo container (rear 65%)
        float cabL = bsz.x * 0.35f, cargoL = bsz.x * 0.65f;
        float cabX = bsz.x * 0.5f - cabL * 0.5f;
        float carX = -bsz.x * 0.5f + cargoL * 0.5f;
        // Cargo container box (replace top approximation)
        drawCube(v, s, vm, glm::vec3(carX, bsz.y + tsz.y * 0.5f + 0.05f, 0),
            glm::vec3(cargoL * 0.95f, tsz.y * 0.95f, bsz.z * 0.90f), glm::vec3(0.48f, 0.46f, 0.44f));
        // Container frame edge strips
        drawCube(v, s, vm, glm::vec3(carX, bsz.y + tsz.y + 0.08f, 0),
            glm::vec3(cargoL, 0.15f, bsz.z + 0.05f), glm::vec3(0.32f));
        // Container door lines at rear
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.5f - 0.01f, bsz.y + tsz.y * 0.5f, 0.01f),
            glm::vec3(0.06f, tsz.y * 0.9f, 0.06f), glm::vec3(0.30f));
        // Middle axle (extra wheels under cargo)
        float mwx = bsz.x * 0.35f * 0.4f;
        glm::vec3 mws(0.45f, 0.45f, 0.20f);
        drawCube(v, s, vm, glm::vec3(-mwx, 0.20f, wz), mws, wc);
        drawCube(v, s, vm, glm::vec3(-mwx, 0.20f, -wz), mws, wc);
        // Exhaust pipe (top-left of cabin)
        drawCylinder(s, vm, glm::vec3(cabX - cabL * 0.3f, bsz.y + tsz.y + 0.1f, bsz.z * 0.45f),
            0.08f, 0.90f, glm::vec3(0.20f));
        // Fuel tanks (both sides under cargo)
        drawCube(v, s, vm, glm::vec3(carX, 0.28f, bsz.z * 0.5f + 0.12f),
            glm::vec3(cargoL * 0.6f, 0.40f, 0.30f), glm::vec3(0.35f));
        drawCube(v, s, vm, glm::vec3(carX, 0.28f, -bsz.z * 0.5f - 0.12f),
            glm::vec3(cargoL * 0.6f, 0.40f, 0.30f), glm::vec3(0.35f));
        // Front grill
        drawCube(v, s, vm, glm::vec3(fx + 0.02f, bsz.y * 0.35f, 0),
            glm::vec3(0.07f, bsz.y * 0.42f, bsz.z * 0.60f), glm::vec3(0.15f));
        // Mud guards (rear wheels)
        drawCube(v, s, vm, glm::vec3(-wx, bsz.y * 0.12f, wz + 0.12f), glm::vec3(0.55f, 0.12f, 0.12f), glm::vec3(0.20f));
        drawCube(v, s, vm, glm::vec3(-wx, bsz.y * 0.12f, -wz - 0.12f), glm::vec3(0.55f, 0.12f, 0.12f), glm::vec3(0.20f));
    }
    else if (type == 3)  // Ambulance
    {
        // Front cabin (front 40%) slightly narrower; medical box (rear 60%)
        float cabL = bsz.x * 0.40f;
        float boxL = bsz.x * 0.60f;
        float boxX = -bsz.x * 0.5f + boxL * 0.5f;
        // Medical box (white, slightly taller)
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y * 0.5f, 0),
            glm::vec3(boxL * 0.96f, tsz.y * 1.05f, bsz.z * 0.92f), glm::vec3(0.96f, 0.96f, 0.96f));
        // Roof unit on medical box
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y + 0.22f, 0),
            glm::vec3(boxL * 0.75f, 0.28f, bsz.z * 0.70f), glm::vec3(0.88f, 0.88f, 0.88f));
        // Light bar on cabin roof (siren strip)
        s.setBool("isEmissive", isNight);
        glm::vec3 sirenC = isNight ? glm::vec3(1, 0.08f, 0.08f) : glm::vec3(0.75f, 0.12f, 0.12f);
        drawCube(v, s, vm, glm::vec3(bsz.x * 0.15f, bsz.y + tsz.y + 0.18f, 0),
            glm::vec3(cabL * 0.75f, 0.20f, bsz.z * 0.45f), sirenC);
        s.setBool("isEmissive", false);
        // Red cross on medical box side (emissive)
        s.setBool("isEmissive", true);
        glm::vec3 crossC(0.90f, 0.08f, 0.08f);
        // Cross vertical
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y * 0.6f, bsz.z * 0.5f + 0.02f),
            glm::vec3(0.20f, tsz.y * 0.58f, 0.05f), crossC);
        // Cross horizontal
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y * 0.6f, bsz.z * 0.5f + 0.02f),
            glm::vec3(0.70f, 0.20f, 0.05f), crossC);
        // Right side cross
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y * 0.6f, -bsz.z * 0.5f - 0.02f),
            glm::vec3(0.20f, tsz.y * 0.58f, 0.05f), crossC);
        drawCube(v, s, vm, glm::vec3(boxX, bsz.y + tsz.y * 0.6f, -bsz.z * 0.5f - 0.02f),
            glm::vec3(0.70f, 0.20f, 0.05f), crossC);
        s.setBool("isEmissive", false);
        // Reflective stripe along sides
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.22f, bsz.z * 0.5f + 0.01f),
            glm::vec3(bsz.x * 0.95f, 0.12f, 0.04f), glm::vec3(1.0f, 0.45f, 0.05f));
        drawCube(v, s, vm, glm::vec3(0, bsz.y * 0.22f, -bsz.z * 0.5f - 0.01f),
            glm::vec3(bsz.x * 0.95f, 0.12f, 0.04f), glm::vec3(1.0f, 0.45f, 0.05f));
        // Rear double-door frame
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.5f - 0.02f, bsz.y + tsz.y * 0.55f, 0),
            glm::vec3(0.06f, tsz.y * 0.92f, bsz.z * 0.92f), glm::vec3(0.55f, 0.55f, 0.55f));
        drawCube(v, s, vm, glm::vec3(-bsz.x * 0.5f - 0.02f, bsz.y + tsz.y * 0.55f, 0.01f),
            glm::vec3(0.06f, 0.06f, bsz.z * 0.86f), glm::vec3(0.38f));
    }
    else  // Fire truck (default / type 4)
    {
        float cabLen = bsz.x * 0.40f;
        float equLen = bsz.x * 0.60f;
        float cabX = bsz.x * 0.5f - cabLen * 0.5f;
        float equX = -bsz.x * 0.5f + equLen * 0.5f;
        // Equipment body (rear, slightly taller)
        drawCube(v, s, vm, glm::vec3(equX, bsz.y + tsz.y * 0.5f + 0.1f, 0),
            glm::vec3(equLen * 0.96f, tsz.y * 1.12f, bsz.z * 0.92f), body * 0.92f);
        // Ladder flat planks on top of equipment body (5 rungs)
        float ladBase = bsz.y + tsz.y + 0.35f;
        drawCube(v, s, vm, glm::vec3(equX, ladBase, bsz.z * 0.28f),
            glm::vec3(equLen * 0.90f, 0.12f, 0.15f), glm::vec3(0.68f, 0.52f, 0.32f));
        drawCube(v, s, vm, glm::vec3(equX, ladBase, -bsz.z * 0.28f),
            glm::vec3(equLen * 0.90f, 0.12f, 0.15f), glm::vec3(0.68f, 0.52f, 0.32f));
        for (int rung = 0; rung < 5; rung++) {
            float rx = equX - equLen * 0.38f + rung * (equLen * 0.76f / 4.0f);
            drawCube(v, s, vm, glm::vec3(rx, ladBase + 0.10f, 0),
                glm::vec3(0.18f, 0.12f, bsz.z * 0.58f), glm::vec3(0.70f, 0.55f, 0.35f));
        }
        // Siren / light bar on cabin roof (emissive)
        s.setBool("isEmissive", isNight);
        glm::vec3 sirC = isNight ? glm::vec3(1, 0.08f, 0.08f) : glm::vec3(0.80f, 0.08f, 0.08f);
        drawCube(v, s, vm, glm::vec3(cabX, bsz.y + tsz.y + 0.20f, 0),
            glm::vec3(cabLen * 0.70f, 0.22f, bsz.z * 0.40f), sirC);
        s.setBool("isEmissive", false);
        // Hose reel (small cylinder behind cabin)
        drawCylinder(s, vm, glm::vec3(cabX - cabLen * 0.55f, bsz.y, bsz.z * 0.30f),
            0.25f, 0.45f, glm::vec3(0.72f, 0.18f, 0.12f));
        // Side storage compartments (dark slab rows on equipment body)
        for (int comp = 0; comp < 3; comp++) {
            float cx2 = equX - equLen * 0.32f + comp * (equLen * 0.65f / 2.0f);
            drawCube(v, s, vm, glm::vec3(cx2, bsz.y + tsz.y * 0.4f, bsz.z * 0.5f + 0.02f),
                glm::vec3(equLen * 0.28f, tsz.y * 0.72f, 0.05f), glm::vec3(0.25f, 0.08f, 0.06f));
            drawCube(v, s, vm, glm::vec3(cx2, bsz.y + tsz.y * 0.4f, -bsz.z * 0.5f - 0.02f),
                glm::vec3(equLen * 0.28f, tsz.y * 0.72f, 0.05f), glm::vec3(0.25f, 0.08f, 0.06f));
        }
        // Access steps at rear
        for (int st = 0; st < 3; st++) {
            drawCube(v, s, vm, glm::vec3(-bsz.x * 0.5f - 0.06f, bsz.y * 0.22f + st * 0.22f, bsz.z * 0.35f),
                glm::vec3(0.22f, 0.10f, 0.35f), glm::vec3(0.22f));
        }
        // Exhaust pipe (top side, vertical)
        drawCylinder(s, vm, glm::vec3(cabX + cabLen * 0.3f, bsz.y + tsz.y, -bsz.z * 0.38f),
            0.09f, 0.85f, glm::vec3(0.18f));
    }
}

// Boat (hull + cabin + mast + flag)
void drawBoat(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, int dir)
{
    glm::mat4 bm = glm::translate(B, p);
    if (dir < 0) bm = myRotate(bm, glm::radians(180.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, bm, glm::vec3(0, 0, 0), glm::vec3(2.5f, 1.1f, 6.5f), glm::vec3(0.62f, 0.38f, 0.18f));
    drawCube(v, s, bm, glm::vec3(0, 1.3f, 0.8f), glm::vec3(1.4f, 1.3f, 1.8f), glm::vec3(0.95f, 0.92f, 0.88f));
    drawCylinder(s, bm, glm::vec3(0, 1.3f, -2.2f), 0.07f, 2.2f, glm::vec3(0.35f));
    drawCube(v, s, bm, glm::vec3(0.45f, 2.8f, -2.2f), glm::vec3(0.85f, 0.55f, 0.1f), glm::vec3(0.92f, 0.2f, 0.15f));
}

// Landmark and building draw routines (scaled for this project version).

// Jamuna Future Park (JFP): origin p at ground centre; main block 44 x 35 x 40 (halfW 22, halfD 17.5);
// front at p.z - 17.5 faces -Z toward the highway. Uses Bezier facade strips, ruled ramp quads,
// hierarchical draw calls, and Phong materials (concrete, glass, metal).

// JFP: curved glass front (quadratic Bezier strips).
void drawCurvedFacadeBezier(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    // Quadratic Bezier in XZ: P0=(-halfW,0), P1=(0,-protrude), P2=(+halfW,0)
    // x-component traces width; z-component gives forward curvature.
    const float halfW = 22.0f;
    const float h = 40.0f;
    const float protrude = 2.8f;       // centre protrudes this far in -Z
    const float baseZ = -17.5f;     // front face of main block
    const int   N = 26;         // number of vertical strips
    const float sw = (halfW * 2.0f) / (N - 1) * 1.06f;  // strip width with slight overlap

    glm::vec3 glass(0.55f, 0.78f, 0.96f), frame(0.42f, 0.50f, 0.68f);

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

// JFP: glass entrance atrium.
void drawEntrance(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    const float ew = 24.0f, eh = 20.0f, ed = 8.0f;
    const float ez = -17.5f - ed * 0.5f;  // entrance centre Z (relative to p)
    glm::vec3 glass(0.58f, 0.80f, 0.96f), frame(0.42f, 0.50f, 0.68f), lite(0.95f, 0.93f, 0.85f);

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

// JFP: cylindrical columns.
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
    // Interior columns: four rows by three columns inside the main block.
    for (float x : {-14.0f, 0.0f, 14.0f})
        for (float z : {-10.0f, 0.0f, 10.0f})
            drawCylinder(s, B, p + glm::vec3(x, 0, z), 0.75f, 40.0f, conc);
}

// JFP: window bands on side and back faces.
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

// JFP: roof slab, parapet, rooftop equipment and signage.
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
    // Roof HVAC blocks on a 2-by-5 grid.
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

// JFP: side wings and rear extension.
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

// JFP parking ramps: each slab is a quad between corresponding points on two edge polylines (ruled-surface approximation).
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

// JFP: assembly (calls sub-parts above).
void drawJFP(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 conc(0.88f, 0.86f, 0.82f);
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

// Hotel: tower, porte-cochere, penthouse.
void drawHotel(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wall(0.92f, 0.88f, 0.78f), trim(0.7f, 0.65f, 0.55f);
    // Main tower body 20 x 52 x 20.
    drawCube(v, s, B, p + glm::vec3(0, 26.0f, 0), glm::vec3(20, 52, 20), wall);
    // Front windows (-Z face)
    for (float y = 3.0f; y < 50.0f; y += 3.5f)
        for (float x = -7.0f; x <= 7.0f; x += 2.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, -10.01f), glm::vec3(1.4f, 1.8f, 0.12f), glm::vec3(0.68f, 0.85f, 0.98f));
    // Side windows
    for (float y = 3.0f; y < 50.0f; y += 3.5f)
        for (float z = -7.0f; z <= 7.0f; z += 2.5f) {
            drawCube(v, s, B, p + glm::vec3(10.01f, y, z), glm::vec3(0.12f, 1.8f, 1.4f), glm::vec3(0.68f, 0.85f, 0.98f));
            drawCube(v, s, B, p + glm::vec3(-10.01f, y, z), glm::vec3(0.12f, 1.8f, 1.4f), glm::vec3(0.68f, 0.85f, 0.98f));
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

// Cricket stadium: bowl, tiers, canopy, access, VIP, roof, lighting, scoreboard, and detail geometry.
void drawCricketStadium(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    int seg = 48;
    float oR = 50.0f, iR = 35.0f, dth = 2 * PI / seg;
    // Inner pitch: top-down texture (stadium_inside.png).
    g_boundTex = texStadiumIn;
    drawCube(v, s, B, p + glm::vec3(0, 0.12f, 0), glm::vec3(iR * 2, 0.22f, iR * 2), glm::vec3(0.35f, 0.85f, 0.28f));
    g_boundTex = 0;
    drawCube(v, s, B, p + glm::vec3(0, 0.20f, 0), glm::vec3(2.0f, 0.14f, 14.0f), glm::vec3(0.72f, 0.62f, 0.42f));
    float tierH[] = { 0.0f, 10.0f, 22.0f };
    float tierOR[] = { oR, oR - 2.0f, oR - 4.0f };
    glm::vec3 tierC[] = { glm::vec3(0.25f,0.55f,0.90f), glm::vec3(0.92f,0.72f,0.18f), glm::vec3(0.88f,0.35f,0.28f) };
    for (int t = 0; t < 3; t++) {
        glm::vec3 tc = tierC[t];
        for (int i = 0; i < seg; i++) {
            float a = i * dth;
            float rm = (tierOR[t] + iR + t * 1.5f) * 0.5f;
            float mx = cosf(a) * rm, mz = sinf(a) * rm;
            drawCube(v, s, B, p + glm::vec3(mx, tierH[t] + 5.0f, mz),
                glm::vec3(7.0f, 10.0f, (tierOR[t] - iR + 2.0f)), tc, -glm::degrees(a));
        }
    }
    // Outer rim texture (stadium.png).
    g_boundTex = texStadium;
    for (int i = 0; i < seg; i++) {
        float a = i * dth;
        drawCube(v, s, B, p + glm::vec3(cosf(a) * oR, 34.0f, sinf(a) * oR),
            glm::vec3(7.5f, 0.5f, 10.0f), glm::vec3(0.88f, 0.88f, 0.90f), -glm::degrees(a));
    }
    g_boundTex = 0;
    float fp[][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
    for (auto& f : fp) {
        glm::vec3 lp = p + glm::vec3(f[0] * oR * 1.15f, 0, f[1] * oR * 1.15f);
        drawCylinder(s, B, lp, 0.5f, 50.0f, glm::vec3(0.35f));
        s.setBool("isEmissive", isNight);
        drawCube(v, s, B, lp + glm::vec3(0, 51.0f, 0), glm::vec3(3.0f, 1.5f, 3.0f),
            isNight ? glm::vec3(1, 0.95f, 0.72f) : glm::vec3(0.5f));
        s.setBool("isEmissive", false);
    }
    for (float a : {0.0f, PI * 0.5f, PI, PI * 1.5f})
        drawCube(v, s, B, p + glm::vec3(cosf(a) * (oR + 2), 5.0f, sinf(a) * (oR + 2)),
            glm::vec3(8.0f, 10.0f, 3.0f), glm::vec3(0.3f, 0.28f, 0.25f));
}

// Apartment tower.
void drawApartment(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float h, glm::vec3 tint)
{
    drawCube(v, s, B, p + glm::vec3(0, h * 0.5f, 0), glm::vec3(16, h, 16), tint);
    // Balcony slabs every 6 units
    for (float y = 6.0f; y < h; y += 6.0f)
        drawCube(v, s, B, p + glm::vec3(0, y, -8.01f), glm::vec3(13, 0.25f, 1.2f), tint * 0.82f);
    // Front windows (-Z face)
    for (float y = 3.0f; y < h - 1.0f; y += 3.5f)
        for (float x = -5.5f; x <= 5.5f; x += 2.2f)
            drawCube(v, s, B, p + glm::vec3(x, y, -8.01f), glm::vec3(1.1f, 1.8f, 0.14f), glm::vec3(0.62f, 0.82f, 0.96f));
    // Side windows (+X face)
    for (float y = 3.0f; y < h - 1.0f; y += 3.5f)
        for (float z = -5.0f; z <= 5.0f; z += 2.2f)
            drawCube(v, s, B, p + glm::vec3(8.01f, y, z), glm::vec3(0.14f, 1.8f, 1.1f), glm::vec3(0.62f, 0.82f, 0.96f));
    // Roof water-tank + stairwell
    drawCube(v, s, B, p + glm::vec3(2.5f, h + 1.2f, 2.5f), glm::vec3(2.0f, 2.5f, 2.0f), glm::vec3(0.32f));
    drawCube(v, s, B, p + glm::vec3(-2.0f, h + 1.5f, -2.0f), glm::vec3(3.5f, 3.0f, 3.5f), tint * 0.85f);
}

// Corporate office: curtain-wall tower.
void drawCorporateOffice(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 frame(0.50f, 0.62f, 0.78f), glass(0.60f, 0.80f, 0.96f);
    // Main tower 20 x 72 x 18.
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
        drawCube(v, s, B, p + glm::vec3(x, 36.0f, 9.02f), glm::vec3(0.2f, 70, 0.08f), glm::vec3(0.95f));
    // Ground floor lobby
    drawCube(v, s, B, p + glm::vec3(0, 3.5f, 9.2f), glm::vec3(5.5f, 7.0f, 0.3f), glm::vec3(0.32f, 0.5f, 0.68f));
    // Helipad on roof
    drawCube(v, s, B, p + glm::vec3(0, 72.1f, 0), glm::vec3(7.0f, 0.2f, 7.0f), glm::vec3(0.5f, 0.5f, 0.18f));
    drawCylinder(s, B, p + glm::vec3(0, 72.3f, 0), 0.14f, 7.0f, glm::vec3(0.4f));
}

// University: E-plan, portico, dome, lab wing, grounds, gates, lighting.
void drawUniversity(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 brick(0.82f, 0.52f, 0.35f), trim(0.95f, 0.90f, 0.82f);
    glm::vec3 conc(0.72f, 0.70f, 0.68f), asph(0.25f, 0.25f, 0.28f);

    // University: main academic block and classroom wings.
    // Central block 40 x 28 x 24.
    drawCube(v, s, B, p + glm::vec3(0, 14.0f, 0), glm::vec3(40, 28, 24), brick);
    // Left classroom wing
    drawCube(v, s, B, p + glm::vec3(-26.0f, 10.0f, 0), glm::vec3(14, 20, 20), brick * 0.95f);
    // Right classroom wing
    drawCube(v, s, B, p + glm::vec3(26.0f, 10.0f, 0), glm::vec3(14, 20, 20), brick * 0.95f);
    // Floor separator bands on main block (2 bands marking floor levels)
    drawCube(v, s, B, p + glm::vec3(0, 9.5f, 12.1f), glm::vec3(42, 0.5f, 0.5f), trim * 0.90f);
    drawCube(v, s, B, p + glm::vec3(0, 18.5f, 12.1f), glm::vec3(42, 0.5f, 0.5f), trim * 0.90f);
    // Wing window bands (side faces of left/right wings)
    for (float y = 2.5f; y < 19.0f; y += 4.0f) {
        for (float z = -7.0f; z <= 7.0f; z += 3.5f) {
            drawCube(v, s, B, p + glm::vec3(-33.1f, y, z), glm::vec3(0.14f, 2.0f, 1.5f), glm::vec3(0.65f, 0.82f, 0.95f));
            drawCube(v, s, B, p + glm::vec3(33.1f, y, z), glm::vec3(0.14f, 2.0f, 1.5f), glm::vec3(0.65f, 0.82f, 0.95f));
        }
    }

    // University: entrance portico.
    drawCube(v, s, B, p + glm::vec3(0, 12.0f, 13.5f), glm::vec3(24, 0.5f, 3.5f), trim);
    for (float x = -10.0f; x <= 10.0f; x += 4.0f)
        drawCylinder(s, B, p + glm::vec3(x, 0, 14.5f), 0.45f, 13.5f, trim);
    // Pediment
    drawCube(v, s, B, p + glm::vec3(0, 24.0f, 13.0f), glm::vec3(28, 3.5f, 0.6f), trim);

    // University: dome on central block.
    drawSphere(s, B, p + glm::vec3(0, 30.5f, 0), 5.5f, glm::vec3(0.58f, 0.52f, 0.45f));
    // Drum below dome
    drawCylinder(s, B, p + glm::vec3(0, 27.5f, 0), 4.5f, 3.2f, glm::vec3(0.70f, 0.65f, 0.58f));

    // University: window bands on front facade.
    for (float y = 3.0f; y < 24.0f; y += 4.0f)
        for (float x = -14.0f; x <= 14.0f; x += 3.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, 12.01f), glm::vec3(1.8f, 2.2f, 0.14f), glm::vec3(0.65f, 0.82f, 0.95f));

    // University: rear laboratory wing.
    // Lab block: behind and to one side of main block
    drawCube(v, s, B, p + glm::vec3(18.0f, 8.0f, -18.0f), glm::vec3(20, 16, 16), brick * 0.88f);
    // Workshop area (lower, wider)
    drawCube(v, s, B, p + glm::vec3(-16.0f, 5.0f, -18.0f), glm::vec3(24, 10, 14), brick * 0.82f);
    // Lab windows on front face
    for (float y = 2.5f; y < 14.0f; y += 3.5f)
        for (float x = 10.0f; x <= 26.0f; x += 3.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, -10.01f), glm::vec3(1.5f, 2.0f, 0.14f), glm::vec3(0.60f, 0.80f, 0.94f));
    // Equipment/HVAC units on lab roof
    for (float lx : {12.0f, 18.0f, 24.0f})
        drawCube(v, s, B, p + glm::vec3(lx, 17.2f, -18.0f), glm::vec3(3.5f, 1.8f, 3.5f), conc * 0.80f);

    // University: campus paving and ground.
    // Main campus grass lawn
    drawCube(v, s, B, p + glm::vec3(0, 0.08f, 0), glm::vec3(80, 0.14f, 36), glm::vec3(0.42f, 0.82f, 0.32f));
    // Parking zone slab (in front, near road)
    drawCube(v, s, B, p + glm::vec3(0, 0.10f, 26.0f), glm::vec3(70, 0.20f, 14), asph);
    // Parking lane markings
    for (float px = -28.0f; px <= 28.0f; px += 7.0f)
        drawCube(v, s, B, p + glm::vec3(px, 0.20f, 26.0f), glm::vec3(0.35f, 0.08f, 13.0f), glm::vec3(0.92f));
    // Campus walkway (concrete path from gate to entrance)
    drawCube(v, s, B, p + glm::vec3(0, 0.12f, 20.0f), glm::vec3(4.5f, 0.18f, 10.0f), glm::vec3(0.78f, 0.76f, 0.72f));

    // University: garden strip in front of main block.
    drawCube(v, s, B, p + glm::vec3(0, 0.15f, 17.5f), glm::vec3(36, 0.20f, 3.5f), glm::vec3(0.38f, 0.78f, 0.28f));
    // Hedges along garden strip
    for (float gx = -16.0f; gx <= 16.0f; gx += 4.0f)
        drawSphere(s, B, p + glm::vec3(gx, 1.5f, 17.5f), 1.4f, glm::vec3(0.28f, 0.68f, 0.22f));

    // University: entrance gates.
    // Two tall gate pillars at campus boundary (+Z side)
    drawCube(v, s, B, p + glm::vec3(-10.0f, 8.0f, 32.0f), glm::vec3(2.0f, 16.0f, 2.0f), conc);
    drawCube(v, s, B, p + glm::vec3(10.0f, 8.0f, 32.0f), glm::vec3(2.0f, 16.0f, 2.0f), conc);
    // Gate horizontal arch bar
    drawCube(v, s, B, p + glm::vec3(0, 16.5f, 32.0f), glm::vec3(21.5f, 1.0f, 1.5f), conc * 0.92f);
    // Gate name plate (emissive at night)
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(0, 14.5f, 32.1f), glm::vec3(12.0f, 2.2f, 0.4f),
        isNight ? glm::vec3(0.9f, 0.82f, 0.18f) : glm::vec3(0.88f, 0.80f, 0.15f));
    s.setBool("isEmissive", false);
    // Outer boundary fence pillars (simplified, just corner + midpoint pillars)
    float bpX[] = { -35.0f,-16.0f,16.0f,35.0f };
    for (float bx : bpX)
        drawCube(v, s, B, p + glm::vec3(bx, 2.5f, 32.0f), glm::vec3(1.0f, 5.0f, 1.0f), conc * 0.95f);

    // University: department sign.
    s.setBool("isEmissive", isNight);
    drawCube(v, s, B, p + glm::vec3(0, 27.5f, 13.2f), glm::vec3(20, 2.8f, 0.45f),
        isNight ? glm::vec3(0.8f, 0.15f, 0.10f) : glm::vec3(0.75f, 0.12f, 0.08f));
    s.setBool("isEmissive", false);

    // University: campus lamp posts.
    float lpX[] = { -22.0f,-10.0f,10.0f,22.0f,-28.0f,28.0f,-28.0f,28.0f };
    float lpZ[] = { 22.0f, 22.0f,22.0f,22.0f,  8.0f, 8.0f, -8.0f,-8.0f };
    for (int li = 0; li < 8; li++) {
        drawCylinder(s, B, p + glm::vec3(lpX[li], 0, lpZ[li]), 0.15f, 9.0f, glm::vec3(0.38f));
        // Horizontal arm
        drawCube(v, s, B, p + glm::vec3(lpX[li] + 0.8f, 9.0f, lpZ[li]),
            glm::vec3(1.8f, 0.18f, 0.18f), glm::vec3(0.38f));
        s.setBool("isEmissive", isNight);
        drawSphere(s, B, p + glm::vec3(lpX[li] + 1.6f, 8.7f, lpZ[li]), 0.55f,
            isNight ? glm::vec3(1, 0.95f, 0.72f) : glm::vec3(0.55f), isNight);
        s.setBool("isEmissive", false);
    }

    // University: flagpole.
    drawCylinder(s, B, p + glm::vec3(0, 0, 33.5f), 0.15f, 12.0f, glm::vec3(0.42f));
    drawCube(v, s, B, p + glm::vec3(1.2f, 10.5f, 33.5f), glm::vec3(3.0f, 1.8f, 0.08f),
        glm::vec3(0.10f, 0.30f, 0.72f));
}

// Hospital: wings, marking, helipad.
void drawHospital(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    glm::vec3 wh(0.94f, 0.94f, 0.92f);
    // Main tower 24 x 40 x 20.
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
            drawCube(v, s, B, p + glm::vec3(x, y, 21.01f), glm::vec3(1.4f, 2.0f, 0.14f), glm::vec3(0.68f, 0.88f, 0.98f));
    // Window bands on main tower front (+Z half)
    for (float y = 2.5f; y < 38.0f; y += 3.0f)
        for (float x = -8.0f; x <= 8.0f; x += 2.5f)
            drawCube(v, s, B, p + glm::vec3(x, y, 10.01f), glm::vec3(1.2f, 1.8f, 0.14f), glm::vec3(0.68f, 0.88f, 0.98f));
    // Helipad
    drawCylinder(s, B, p + glm::vec3(0, 40.08f, 0), 5.5f, 0.15f, glm::vec3(0.78f, 0.78f, 0.75f));
    drawCube(v, s, B, p + glm::vec3(0, 40.2f, 0), glm::vec3(0.7f, 0.12f, 4.0f), glm::vec3(0.95f));
    drawCube(v, s, B, p + glm::vec3(-1.3f, 40.2f, 0), glm::vec3(0.7f, 0.12f, 0.7f), glm::vec3(0.95f));
    drawCube(v, s, B, p + glm::vec3(1.3f, 40.2f, 0), glm::vec3(0.7f, 0.12f, 0.7f), glm::vec3(0.95f));
    // Ambulance bay overhang
    drawCube(v, s, B, p + glm::vec3(9.0f, 6.5f, 23.5f), glm::vec3(6.5f, 0.25f, 5.0f), glm::vec3(0.6f, 0.58f, 0.55f));
}

// Park: lawn, Bezier path, trees, pond, benches.
void drawPark(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    drawCube(v, s, B, p + glm::vec3(0, 0.08f, 0), glm::vec3(36, 0.14f, 32), glm::vec3(0.40f, 0.85f, 0.32f));
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

// Eiffel Tower: Bezier legs, truss, arches, campanile, spire.
void drawEiffelTower(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p) {
    float S = 145.0f;
    glm::vec3 iron(0.85f, 0.70f, 0.45f), dkI(0.75f, 0.60f, 0.40f), plat(0.88f, 0.82f, 0.72f);
    float sX[4] = { 1,-1,-1,1 }, sZ[4] = { 1,1,-1,-1 };
    int nS = 40;
    glm::vec3 oP[4][41], iP[4][41];
    for (int c = 0; c < 4; c++) {
        // Modified Bezier to converge perfectly to center
        glm::vec3 oA(sX[c] * 0.12f * S, 0, sZ[c] * 0.12f * S);
        glm::vec3 oB(sX[c] * 0.10f * S, 0.30f * S, sZ[c] * 0.10f * S);
        glm::vec3 oC(sX[c] * 0.02f * S, 0.70f * S, sZ[c] * 0.02f * S);
        glm::vec3 oD(0, 1.0f * S, 0);
        glm::vec3 iA(sX[c] * 0.07f * S, 0, sZ[c] * 0.07f * S);
        glm::vec3 iB(sX[c] * 0.05f * S, 0.25f * S, sZ[c] * 0.05f * S);
        glm::vec3 iC(0, 0.65f * S, 0);
        glm::vec3 iD(0, 1.0f * S, 0);
        for (int i = 0; i <= nS; i++) {
            float t = i / (float)nS;
            oP[c][i] = bezier3(oA, oB, oC, oD, t);
            iP[c][i] = bezier3(iA, iB, iC, iD, t);
        }
    }
    // Lattice
    for (int i = 0; i < nS; i++) {
        for (int c = 0; c < 4; c++) {
            glm::vec3 om = (oP[c][i] + oP[c][i + 1]) * 0.5f;
            glm::vec3 im = (iP[c][i] + iP[c][i + 1]) * 0.5f;
            glm::vec3 mid = (om + im) * 0.5f;
            float segH = oP[c][i + 1].y - oP[c][i].y;
            if (segH < 0.1f) segH = 0.1f;
            float wX = fabsf(om.x - im.x) + 0.35f;
            float wZ = fabsf(om.z - im.z) + 0.35f;
            drawCube(v, s, B, p + mid, glm::vec3(wX, segH, wZ), iron);
        }
    }

    // Floors: height clamped to stay inside the leg profile curve.
    float pH[] = { 0.15f * S, 0.35f * S, 0.60f * S, 0.85f * S };
    for (int pl = 0; pl < 4; pl++) {
        float t = pH[pl] / (1.0f * S);
        int idx = (int)(t * nS); if (idx > nS) idx = nS;
        float w = 0;
        for (int c = 0; c < 4; c++) {
            float ax = fabsf(oP[c][idx].x);
            if (ax > w) w = ax;
        }
        w *= 2.0f; // Keep footprint inside outer leg boundary.
        drawCube(v, s, B, p + glm::vec3(0, pH[pl], 0), glm::vec3(w, 0.8f, w), plat);
    }

    // Top Cone
    drawCylinder(s, B, p + glm::vec3(0, 0.95f * S, 0), 0.8f, (0.05f) * S, iron);
}


// Garden: paths, fountain tiers, beds, hedges.
void drawGarden(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p)
{
    // 4 lawn quadrants
    drawCube(v, s, B, p + glm::vec3(-7, 0.1f, -7), glm::vec3(12, 0.14f, 12), glm::vec3(0.42f, 0.85f, 0.32f));
    drawCube(v, s, B, p + glm::vec3(7, 0.1f, -7), glm::vec3(12, 0.14f, 12), glm::vec3(0.42f, 0.85f, 0.32f));
    drawCube(v, s, B, p + glm::vec3(-7, 0.1f, 7), glm::vec3(12, 0.14f, 12), glm::vec3(0.42f, 0.85f, 0.32f));
    drawCube(v, s, B, p + glm::vec3(7, 0.1f, 7), glm::vec3(12, 0.14f, 12), glm::vec3(0.42f, 0.85f, 0.32f));
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

// Burj Khalifa: Y-plan tower; build order is podium, core and wings, setbacks, mechanical bands, facade, spire, lighting.
void drawBurjKhalifa(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p) {
    unsigned int burjSkin = g_boundTex; // Preserve building skin while emissive strips use untextured cubes.
    float totalH = 450.0f; // Total height (dominant skyline element in this scene).
    float hubR = 12.0f, wingLen = 30.0f, wingW = 10.0f;
    int nT = 45;
    float bodyH = totalH - 80.0f;
    float tH = bodyH / nT;
    glm::vec3 oceanBlue(0.1f, 0.5f, 0.9f), frame(0.85f, 0.85f, 0.9f); // Glass tint and frame
    glm::vec3 spire(0.85f, 0.87f, 0.92f);
    glm::vec3 mechBand(0.4f, 0.4f, 0.6f);

    // Podium
    g_boundTex = burjSkin;
    drawCube(v, s, B, p + glm::vec3(0, -1.5f, 0), glm::vec3(100.0f, 6.0f, 100.0f), frame);
    // Tiers
    float wA[3] = { 0.0f, 120.0f, 240.0f };
    float wL[3] = { wingLen, wingLen, wingLen };
    float curY = 4.5f;
    for (int ti = 0; ti < nT; ti++) {
        float prog = (float)ti / nT;
        float tap = 1.0f - prog * 0.8f;
        if (ti > 3 && ti % 2 == 0) wL[(ti / 2) % 3] *= 0.85f;
        float cR = hubR * tap;

        g_boundTex = burjSkin;
        drawCylinder(s, B, p + glm::vec3(0, curY, 0), cR, tH, frame);
        for (int w = 0; w < 3; w++) {
            float wl = wL[w] * tap, ww = wingW * tap;
            if (wl < 1.0f) continue;
            float a = glm::radians(wA[w]);
            float cx = cosf(a) * (cR + wl * 0.5f), cz = sinf(a) * (cR + wl * 0.5f);

            g_boundTex = burjSkin;
            drawCube(v, s, B, p + glm::vec3(cx, curY + tH * 0.5f, cz), glm::vec3(wl, tH, ww), oceanBlue, -wA[w]);
            float tx = cosf(a) * (cR + wl), tz = sinf(a) * (cR + wl);
            drawCylinder(s, B, p + glm::vec3(tx, curY, tz), ww * 0.5f, tH, oceanBlue);

            for (float wy = curY + 1.0f; wy < curY + tH - 0.5f; wy += 2.5f) {
                s.setBool("isEmissive", isNight);
                g_boundTex = 0;
                drawCube(v, s, B, p + glm::vec3(cx, wy, cz), glm::vec3(wl * 0.95f, 0.6f, ww * 1.05f), glm::vec3(0.8f, 0.9f, 1.0f), -wA[w]);
                s.setBool("isEmissive", false);
                g_boundTex = burjSkin;
            }
        }
        g_boundTex = burjSkin;
        drawCylinder(s, B, p + glm::vec3(0, curY + tH, 0), cR * 1.1f, 0.5f, frame);
        curY += tH;
    }
    // Spire
    float sR[] = { 5.0f,4.0f,2.5f,1.0f,0.5f };
    float sH[] = { 20.0f,20.0f,15.0f,15.0f,10.0f };
    g_boundTex = burjSkin;
    for (int i = 0; i < 5; i++) {
        drawCylinder(s, B, p + glm::vec3(0, curY, 0), sR[i], sH[i], spire);
        curY += sH[i];
    }
    s.setBool("isEmissive", true);
    g_boundTex = 0;
    drawSphere(s, B, p + glm::vec3(0, curY + 1.0f, 0), 1.5f, glm::vec3(1, 0.9f, 0.2f), true);
    s.setBool("isEmissive", false);
    g_boundTex = 0;
}


// INFRASTRUCTURE

// Ground: concrete base; grass texture only where assigned.
void drawGround(unsigned int v, Shader& s, glm::mat4 B)
{
    glm::vec3 conc(0.78f, 0.76f, 0.70f);
    // City 1 ground
    drawCube(v, s, B, glm::vec3(280.0f, 4.0f, 0), glm::vec3(440.0f, 8.0f, SCENE_Z * GROUND_PLANE_SCALE), conc);
    // City 2 ground
    drawCube(v, s, B, glm::vec3(-280.0f, 4.0f, 0), glm::vec3(440.0f, 8.0f, SCENE_Z * GROUND_PLANE_SCALE), conc);
}

// Elevated highway: deck, barriers, median, markings, piers, ramps.
void drawHighway(unsigned int v, Shader& s, glm::mat4 B)
{
    glm::vec3 deck(0.52f, 0.52f, 0.55f), barrier(0.92f, 0.92f, 0.90f);
    glm::vec3 median(0.82f, 0.82f, 0.80f), mark(0.98f, 0.98f, 0.96f);
    glm::vec3 pillar(0.78f, 0.76f, 0.72f), road(0.45f, 0.45f, 0.48f);

    float leftCx = -(SCENE_X + RIV_HW) * 0.5f;
    float rightCx = (SCENE_X + RIV_HW) * 0.5f;
    float deckLen = SCENE_X - RIV_HW;
    const float hwyDeckZ = 18.0f * 1.5f;
    const float hwyBarZ = 9.5f * 2.0f;
    const float hwyMarkZ = 4.5f * 2.0f;
    const float rivSkipMark = RIV_HW + 4.0f;
    const float rivSkipPier = RIV_HW + 8.0f;

    // Left highway deck segment (X < -RIV_HW).
    g_boundTex = texBridgeRoad;
    drawCube(v, s, B, glm::vec3(leftCx, HWY_Y - 1.0f, 0), glm::vec3(deckLen, 2.0f, hwyDeckZ), deck);
    // Right highway deck segment (X > +RIV_HW).
    drawCube(v, s, B, glm::vec3(rightCx, HWY_Y - 1.0f, 0), glm::vec3(deckLen, 2.0f, hwyDeckZ), deck);
    g_boundTex = 0;

    for (float side : {-hwyBarZ, hwyBarZ}) {
        drawCube(v, s, B, glm::vec3(leftCx, HWY_Y + 0.6f, side), glm::vec3(deckLen, 1.2f, 0.5f), barrier);
        drawCube(v, s, B, glm::vec3(rightCx, HWY_Y + 0.6f, side), glm::vec3(deckLen, 1.2f, 0.5f), barrier);
    }
    drawCube(v, s, B, glm::vec3(leftCx, HWY_Y + 0.5f, 0), glm::vec3(deckLen, 1.0f, 1.2f), median);
    drawCube(v, s, B, glm::vec3(rightCx, HWY_Y + 0.5f, 0), glm::vec3(deckLen, 1.0f, 1.2f), median);

    for (float x = -SCENE_X; x <= SCENE_X; x += 7.0f) {
        if (x > -rivSkipMark && x < rivSkipMark) continue;
        drawCube(v, s, B, glm::vec3(x, HWY_Y + 0.02f, -hwyMarkZ), glm::vec3(4.5f, 0.08f, 0.28f), mark);
        drawCube(v, s, B, glm::vec3(x, HWY_Y + 0.02f, hwyMarkZ), glm::vec3(4.5f, 0.08f, 0.28f), mark);
    }

    for (float x = -SCENE_X; x <= SCENE_X; x += 18.0f) {
        if (x > -rivSkipPier && x < rivSkipPier) continue;
        drawCube(v, s, B, glm::vec3(x, (HWY_Y - 2) * 0.5f, -hwyMarkZ), glm::vec3(5.5f, HWY_Y - 2, 3.0f), pillar);
        drawCube(v, s, B, glm::vec3(x, (HWY_Y - 2) * 0.5f, hwyMarkZ), glm::vec3(5.5f, HWY_Y - 2, 3.0f), pillar);
    }

    // Connecting roads (ground level, along Z, under/beside highway)
    float connX[] = { -170,-110,-45, 55, 120, 180 };
    for (float cx : connX) {
        drawCube(v, s, B, glm::vec3(cx, 0.08f, 36), glm::vec3(5.5f, 0.35f, 52), road);
        drawCube(v, s, B, glm::vec3(cx, 0.08f, -36), glm::vec3(5.5f, 0.35f, 52), road);
        // Zebra crossings
        for (int zs = -2; zs <= 2; zs++)
            drawCube(v, s, B, glm::vec3(cx, 0.18f, zs * 1.4f), glm::vec3(5.0f, 0.08f, 0.7f), mark);
    }
}

// Bridge: textured deck, towers, catenary cables, suspenders.
void drawBridge(unsigned int v, Shader& s, glm::mat4 B) {
    glm::vec3 asph(0.28f, 0.28f, 0.30f), line(0.9f, 0.9f, 0.2f);
    glm::vec3 conc(0.72f, 0.70f, 0.66f), cableC(0.15f, 0.15f, 0.18f);

    float bridgeLength = SCENE_X * 2.0f;
    g_boundTex = texBridgeRoad;
    drawCube(v, s, B, glm::vec3(0, BRIDGE_DECK_CENTER_Y, 0),
        glm::vec3(bridgeLength, BRIDGE_DECK_THICK, BRIDGE_DECK_HALF_Z * 2.0f), asph);
    g_boundTex = 0;
    drawCube(v, s, B, glm::vec3(0, BRIDGE_SURFACE_Y + 0.05f, 0),
        glm::vec3(bridgeLength, 0.1f, 0.4f), line);

    const float towerH = 88.0f;
    const float cableX0 = -(SCENE_X - 18.0f);
    const float cableX1 = (SCENE_X - 18.0f);
    const float spanTotal = cableX1 - cableX0;

    // Pillar: total height from ground (was HWY_Y + towerH); width is X/Z half-extent.
    const float pillarFullH = (HWY_Y + towerH) * 0.75f;
    const float pillarHalfW = 5.0f * 1.5f;
    const float yCableTop = pillarFullH;

    const int nPillars = 6;
    float pillarX[6];
    for (int pi = 0; pi < nPillars; pi++) {
        float u = (nPillars <= 1) ? 0.5f : (float)pi / (float)(nPillars - 1);
        pillarX[pi] = cableX0 + u * spanTotal;
    }

    auto cableYOnSpan = [](float yTop, float sag, float t01) {
        return yTop - 4.0f * sag * t01 * (1.0f - t01);
        };

    for (int side = -1; side <= 1; side += 2) {
        float z = side * BRIDGE_CABLE_Z;
        g_boundTex = texPillar;
        for (int pi = 0; pi < nPillars; pi++) {
            float px = pillarX[pi];
            drawCube(v, s, B,
                glm::vec3(px, pillarFullH * 0.5f, z),
                glm::vec3(pillarHalfW, pillarFullH, pillarHalfW), conc);
        }
        g_boundTex = 0;

        // One sagging cable segment between each pair of adjacent pillars (parabolic sag).
        for (int pi = 0; pi < nPillars - 1; pi++) {
            float xL = pillarX[pi];
            float xR = pillarX[pi + 1];
            float spanL = xR - xL;
            float sag = glm::clamp(0.14f * spanL, 5.5f, 22.0f);

            int segs = glm::max(12, (int)(spanL / 18.0f) * 4 + 12);
            glm::vec3 lastP(xL, yCableTop, z);
            for (int i = 1; i <= segs; i++) {
                float t = (float)i / (float)segs;
                float cx = xL + spanL * t;
                float cy = cableYOnSpan(yCableTop, sag, t);
                glm::vec3 np(cx, cy, z);

                float dx = np.x - lastP.x;
                float dy = np.y - lastP.y;
                float segL = sqrtf(dx * dx + dy * dy);
                float angle = atan2f(dy, dx);

                glm::mat4 cm = glm::translate(B, (lastP + np) * 0.5f);
                cm = myRotate(cm, angle, glm::vec3(0, 0, 1));
                g_boundTex = texCable;
                drawCube(v, s, cm, glm::vec3(0),
                    glm::vec3(segL * 1.05f, 1.65f, 1.65f), cableC);
                g_boundTex = 0;
                lastP = np;
            }

            int nSuspSpan = glm::max(5, (int)(spanL / 55.0f) + 4);
            for (int sj = 1; sj < nSuspSpan; sj++) {
                float t = (float)sj / (float)nSuspSpan;
                float sx = xL + spanL * t;
                float sy = cableYOnSpan(yCableTop, sag, t);
                float sLen = sy - BRIDGE_SURFACE_Y;
                if (sLen > 0.5f) {
                    g_boundTex = texCable;
                    drawCylinder(s, B, glm::vec3(sx, BRIDGE_SURFACE_Y, z), 0.48f, sLen, cableC);
                    g_boundTex = 0;
                }
            }
        }
    }
}


// River: water volume, vertex animation, surface ripples.
void drawRiver(unsigned int v, Shader& s, glm::mat4 B) {
    s.setBool("isWater", true);
    s.setFloat("time", gTime);
    g_boundTex = texRiver;
    drawCube(v, s, B, glm::vec3(0, -2.0f, 0), glm::vec3(RIV_HW * 4.0f, 4.0f, SCENE_Z * GROUND_PLANE_SCALE), glm::vec3(0.05f, 0.35f, 0.75f));
    g_boundTex = 0;
    s.setBool("isWater", false);

    // Surface detail: thin animated cubes; count trades cost versus visual density.
    int numLines = 24;
    float len = RIV_HW * 4.0f;
    float riverLenZ = SCENE_Z * GROUND_PLANE_SCALE;
    for (int i = 0; i < numLines; i++) {
        float lz = -RIVER_Z_HALF + fmod((float)i / numLines * riverLenZ + gTime * 10.5f, riverLenZ);
        float lx = -len / 2.0f + fmod((float)(i * 7) / numLines * len, len);
        float ly = 0.1f + sin(gTime * 2.6f + i) * 0.1f;
        drawCube(v, s, B, glm::vec3(lx, ly, lz), glm::vec3(0.5f, 0.05f, 2.0f + (i % 3) * 1.5f), glm::vec3(0.6f, 0.8f, 0.9f));
    }
}


// Vehicle & Boat init / update

void drawCargoShip(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, int dir) {
    glm::mat4 m = glm::translate(B, p);
    m = myRotate(m, glm::radians(dir > 0 ? 90.0f : -90.0f), glm::vec3(0, 1, 0));
    glm::vec3 hull(0.15f, 0.2f, 0.25f), deck(0.3f), cont1(0.8f, 0.2f, 0.2f), cont2(0.2f, 0.5f, 0.8f), cab(0.9f);

    int segs = 15;
    glm::vec3 hA(18.0f, 0, 0), hB(14.0f, 0, 4.0f), hC(-14.0f, 0, 4.0f), hD(-16.0f, 0, 0);
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
        glm::vec3 p0 = bezier3(hA, hB, hC, hD, t0);
        glm::vec3 p1 = bezier3(hA, hB, hC, hD, t1);
        float w0 = bezier3(glm::vec3(0, 0, 0), glm::vec3(0, 0, 4.5f), glm::vec3(0, 0, 4.5f), glm::vec3(0, 0, 0), t0).z;
        float w1 = bezier3(glm::vec3(0, 0, 0), glm::vec3(0, 0, 4.5f), glm::vec3(0, 0, 4.5f), glm::vec3(0, 0, 0), t1).z;
        glm::mat4 sm = glm::translate(m, glm::vec3((p0.x + p1.x) * 0.5f, 2.5f, 0));
        drawCube(v, s, sm, glm::vec3(0, 0, 0), glm::vec3(p0.x - p1.x + 0.1f, 5.0f, (w0 + w1)), hull);
    }
    drawCube(v, s, m, glm::vec3(0, 5.1f, 0), glm::vec3(34.0f, 0.4f, 8.5f), deck);

    for (float cx = -10.0f; cx <= 10.0f; cx += 4.2f) {
        for (float cz = -3.0f; cz <= 3.0f; cz += 2.2f) {
            drawCube(v, s, m, glm::vec3(cx, 6.5f, cz), glm::vec3(4.0f, 2.4f, 2.0f), cx > 0 ? cont1 : cont2);
            drawCube(v, s, m, glm::vec3(cx, 9.0f, cz), glm::vec3(4.0f, 2.4f, 2.0f), cx < 0 ? cont1 : cont2);
        }
    }
    drawCube(v, s, m, glm::vec3(-14.0f, 8.0f, 0), glm::vec3(4.0f, 6.0f, 6.0f), cab);
}

void drawHarmonyShip(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, int dir) {
    glm::mat4 m = glm::translate(B, p);
    m = myRotate(m, glm::radians(dir > 0 ? 90.0f : -90.0f), glm::vec3(0, 1, 0));
    glm::vec3 wht(0.95f), blue(0.1f, 0.4f, 0.8f), wood(0.6f, 0.4f, 0.2f), glass(0.6f, 0.8f, 0.9f);

    int segs = 15;
    glm::vec3 hA(22.0f, 0, 0), hB(18.0f, 0, 5.5f), hC(-18.0f, 0, 5.5f), hD(-20.0f, 0, 0);
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
        glm::vec3 p0 = bezier3(hA, hB, hC, hD, t0);
        glm::vec3 p1 = bezier3(hA, hB, hC, hD, t1);
        float w0 = bezier3(glm::vec3(0, 0, 0), glm::vec3(0, 0, 6.0f), glm::vec3(0, 0, 6.0f), glm::vec3(0, 0, 0), t0).z;
        float w1 = bezier3(glm::vec3(0, 0, 0), glm::vec3(0, 0, 6.0f), glm::vec3(0, 0, 6.0f), glm::vec3(0, 0, 0), t1).z;
        glm::mat4 sm = glm::translate(m, glm::vec3((p0.x + p1.x) * 0.5f, 3.0f, 0));
        drawCube(v, s, sm, glm::vec3(0, 0, 0), glm::vec3(p0.x - p1.x + 0.1f, 6.0f, (w0 + w1)), wht);
        drawCube(v, s, sm, glm::vec3(0, 5.0f, 0), glm::vec3(p0.x - p1.x + 0.1f, 0.8f, (w0 + w1) + 0.2f), blue);
    }

    for (int level = 0; level <= 4; level++) {
        float y = 6.0f + level * 2.5f;
        float lW = 36.0f - level * 1.5f;
        drawCube(v, s, m, glm::vec3(0, y + 1.2f, 0), glm::vec3(lW, 2.3f, 10.0f), wht);
        drawCube(v, s, m, glm::vec3(0, y + 1.2f, 0), glm::vec3(lW + 0.2f, 1.2f, 10.2f), glass);
        drawCube(v, s, m, glm::vec3(0, y, 0), glm::vec3(lW + 1.0f, 0.2f, 11.0f), wood);
    }

    drawCylinder(s, m, glm::vec3(-10.0f, 19.0f, 0), 1.5f, 4.0f, wht);
}

void drawAeroplane(unsigned int v, Shader& s, glm::mat4 B, glm::vec3 p, float rotY) {
    glm::mat4 base = glm::translate(B, p);
    base = myRotate(base, glm::radians(rotY), glm::vec3(0, 1, 0));
    base = glm::scale(base, glm::vec3(14.0f)); // Scale up 14x for Emirates texture

    glm::vec3 white(0.95f), blue(0.1f, 0.3f, 0.7f), grey(0.6f);

    // Main fuselage (long cylinder-ish)
    drawCube(v, s, base, glm::vec3(0, 0, 0), glm::vec3(4.5f, 0.8f, 0.75f), white);
    // Aerodynamic Nose
    drawCube(v, s, base, glm::vec3(2.45f, -0.1f, 0), glm::vec3(0.4f, 0.6f, 0.6f), white);
    drawCube(v, s, base, glm::vec3(2.75f, -0.2f, 0), glm::vec3(0.3f, 0.4f, 0.4f), white); // pointy tip

    // Aesthetic Blue Livery Stripe along the side
    drawCube(v, s, base, glm::vec3(-0.2f, 0.05f, 0.38f), glm::vec3(4.8f, 0.15f, 0.05f), blue);
    drawCube(v, s, base, glm::vec3(-0.2f, 0.05f, -0.38f), glm::vec3(4.8f, 0.15f, 0.05f), blue);

    // Passenger Windows (emissive squares)
    s.setBool("isEmissive", true);
    for (int i = -4; i <= 4; i++) {
        drawCube(v, s, base, glm::vec3(i * 0.4f, 0.1f, 0.38f), glm::vec3(0.15f, 0.15f, 0.02f), glm::vec3(0.8f, 0.9f, 1.0f));
        drawCube(v, s, base, glm::vec3(i * 0.4f, 0.1f, -0.38f), glm::vec3(0.15f, 0.15f, 0.02f), glm::vec3(0.8f, 0.9f, 1.0f));
    }
    // Cockpit Front Windows
    drawCube(v, s, base, glm::vec3(2.55f, 0.15f, 0.15f), glm::vec3(0.3f, 0.2f, 0.05f), glm::vec3(0.2f, 0.4f, 0.8f));
    drawCube(v, s, base, glm::vec3(2.55f, 0.15f, -0.15f), glm::vec3(0.3f, 0.2f, 0.05f), glm::vec3(0.2f, 0.4f, 0.8f));
    s.setBool("isEmissive", false);

    // Main wings (swept).
    glm::mat4 wingM = glm::translate(base, glm::vec3(-0.2f, -0.15f, 0));
    glm::mat4 wingL = myRotate(wingM, glm::radians(30.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, wingL, glm::vec3(0, 0, 1.8f), glm::vec3(1.2f, 0.06f, 3.6f), white);
    glm::mat4 wingR = myRotate(wingM, glm::radians(-30.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, wingR, glm::vec3(0, 0, -1.8f), glm::vec3(1.2f, 0.06f, 3.6f), white);

    // Engine nacelles.
    drawCube(v, s, base, glm::vec3(0.5f, -0.35f, 1.5f), glm::vec3(0.7f, 0.35f, 0.35f), grey);
    drawCube(v, s, base, glm::vec3(0.5f, -0.35f, -1.5f), glm::vec3(0.7f, 0.35f, 0.35f), grey);

    // Tail: vertical and horizontal stabilizers.
    glm::mat4 tailV = glm::translate(base, glm::vec3(-2.2f, 0.65f, 0));
    tailV = myRotate(tailV, glm::radians(-35.0f), glm::vec3(0, 0, 1));
    drawCube(v, s, tailV, glm::vec3(0, 0, 0), glm::vec3(0.5f, 1.3f, 0.15f), blue);

    glm::mat4 tailHL = glm::translate(base, glm::vec3(-2.1f, 0.2f, 0));
    tailHL = myRotate(tailHL, glm::radians(30.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, tailHL, glm::vec3(0, 0, 0.8f), glm::vec3(0.6f, 0.05f, 1.6f), white);
    glm::mat4 tailHR = glm::translate(base, glm::vec3(-2.1f, 0.2f, 0));
    tailHR = myRotate(tailHR, glm::radians(-30.0f), glm::vec3(0, 1, 0));
    drawCube(v, s, tailHR, glm::vec3(0, 0, -0.8f), glm::vec3(0.6f, 0.05f, 1.6f), white);
}

// initVehicles: bridge speeds in sp[]; ship speeds in ships.push_back.
void initVehicles() {
    float sp[5] = { 95.0f, 72.0f, 62.0f, 88.0f, 70.0f }; // bridge vehicle speeds
    float x = -SCENE_X;
    while (x < SCENE_X) {
        int t1 = rand() % 5, t2 = rand() % 5;
        vehicles.push_back({ x, t1, sp[t1], 0, 1 });
        vehicles.push_back({ x + (rand() % 20 - 10), t2, sp[t2], 1, -1 });
        x += 40.0f + rand() % 30;
    }
    // Four ships: two cargo and two passenger (harmony) types; see struct Ship for field order.
    ships.push_back({ -280.0f, 58.0f,  1, -32.0f, 0 }); // cargo lane A
    ships.push_back({ 140.0f, 62.0f, -1,  32.0f, 0 }); // cargo lane B
    ships.push_back({ -90.0f, 54.0f,  1, -10.0f, 1 }); // harmony lane A
    ships.push_back({ 220.0f, 56.0f, -1,  14.0f, 1 }); // harmony lane B
}

void updateVehicles(float dt) {
    if (trafficGreen) {
        for (auto& v : vehicles) {
            v.x += v.dir * v.speed * dt;
            if (v.dir > 0 && v.x > SCENE_X) v.x = -SCENE_X;
            if (v.dir < 0 && v.x < -SCENE_X) v.x = SCENE_X;
        }
    }
    for (auto& s : ships) {
        s.z += s.dir * s.speed * dt;
        if (s.dir > 0 && s.z > RIVER_Z_HALF) s.z = -RIVER_Z_HALF;
        if (s.dir < 0 && s.z < -RIVER_Z_HALF) s.z = RIVER_Z_HALF;
    }
}


// Point lights (street-light positions on highway deck)
void setupPointLights()
{
    float pos[][3] = {
        {-170,HWY_Y + 15, 20}, { 170,HWY_Y + 15, 20},
        { -80,HWY_Y + 15, 20}, {  80,HWY_Y + 15, 20},
        {-170,HWY_Y + 15,-20}, { 170,HWY_Y + 15,-20},
        { -80,HWY_Y + 15,-20}, {  80,HWY_Y + 15,-20}
    };
    for (int i = 0; i < 8; i++)
        pointLights[i] = PointLight(pos[i][0], pos[i][1], pos[i][2],
            0.10f, 0.10f, 0.08f, 0.85f, 0.80f, 0.65f, 0.95f, 0.95f, 0.85f,
            1, 0.012f, 0.0015f, i + 1);
}

// loadTexture helper
// Resolves paths relative to several working directories and image extensions.
static unsigned int loadTextureSafe(const char* basePath)
{
    std::string base(basePath);
    // If caller passed only a name like "day_sky.jpg", try several roots
    const char* roots[] = {
        "",                    // cwd (e.g. SmartCity or Debug)
        "./",
        "../",                 // parent (repo root if cwd = SmartCity)
        "../../",              // repo root if cwd = SmartCity/x64/Debug
        "../../../",
        "textures/",
        "./textures/",
        "../textures/",
        "../../textures/",
    };

    auto tryLoad = [](const std::string& path, int& w, int& h, int& nc) -> unsigned char* {
        stbi_set_flip_vertically_on_load(true);
        return stbi_load(path.c_str(), &w, &h, &nc, 0);
        };

    auto loadFromPath = [&](const std::string& p) -> unsigned int {
        int w, h, nc;
        unsigned char* d = tryLoad(p, w, h, nc);
        if (!d) return 0;
        GLenum fmt = (nc == 1) ? GL_RED : (nc == 3) ? GL_RGB : GL_RGBA;
        unsigned int id; glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, d);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(d);
        std::cout << "[TEX] Loaded: " << p << "\n";
        return id;
        };

    // Candidate relative paths: as-given, then stem + .jpg/.png/.jpeg/.bmp
    std::vector<std::string> names;
    names.push_back(base);
    // If base has no extension, try common ones
    if (base.find('.') == std::string::npos) {
        names.push_back(base + ".jpg");
        names.push_back(base + ".png");
        names.push_back(base + ".jpeg");
        names.push_back(base + ".bmp");
    }

    for (const std::string& name : names) {
        // 1) Exact path as written (e.g. user passed "../foo.jpg")
        unsigned int id = loadFromPath(name);
        if (id) return id;

        for (const char* root : roots) {
            std::string full = std::string(root) + name;
            id = loadFromPath(full);
            if (id) return id;
        }
    }

    // Fallback: 1x1 white texture
    std::cout << "[TEX] NOT FOUND (tried cwd, ./, ../, ../../, textures/): " << basePath << "\n";
    unsigned char white[4] = { 255,255,255,255 };
    unsigned int id; glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

// Sky dome (large sphere drawn around scene, emissive)
void drawSkyDome(unsigned int /*vao*/, Shader& sh, glm::mat4 B)
{
    unsigned int skyTex = isNight ? texNightSky : texDaySky;
    if (skyTex == 0) return; // nothing to draw

    sh.setBool("isEmissive", true);
    sh.setBool("isWater", false);
    sh.setBool("isNightSky", isNight && texNightSky != 0);
    sh.setBool("useTexture", true);
    sh.setInt("texMode", 1);
    sh.setFloat("time", gTime);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyTex);
    sh.setInt("texSampler", 0);

    // Inverted skydome: radius chosen to match the far clip plane for a closed horizon.
    glm::mat4 m = glm::translate(B, glm::vec3(0, 0, 0));
    m = glm::scale(m, glm::vec3(-12000.0f, -12000.0f, -12000.0f));
    sh.setMat4("model", m);
    sh.setVec3("material.ambient", glm::vec3(1));
    sh.setVec3("material.diffuse", glm::vec3(1));
    sh.setVec3("material.specular", glm::vec3(0));
    sh.setFloat("material.shininess", 1);
    glBindVertexArray(sphVAO);
    glDrawElements(GL_TRIANGLES, sphIC, GL_UNSIGNED_INT, 0);

    sh.setBool("isNightSky", false);
    sh.setBool("isEmissive", false);
    sh.setBool("useTexture", false);
    sh.setInt("texMode", texMode);
}

// Render full scene
void renderScene(unsigned int vao, Shader& sh, glm::mat4 proj, glm::mat4 view, int vpMode) {
    // Preset views when key 8 cycling is active.
    glm::mat4 newView = view;
    glm::mat4 newProj = proj;
    if (viewModeKey8 == 0) { // Isometric
        newView = glm::lookAt(camera.Position + glm::vec3(SCENE_X * 0.7f, SCENE_X * 0.5f, SCENE_X * 0.7f), camera.Position, glm::vec3(0, 1, 0));
        newProj = glm::ortho(-200.0f, 200.0f, -150.0f, 150.0f, 0.1f, 1500.0f);
    }
    else if (viewModeKey8 == 1) { // Top View
        newView = glm::lookAt(camera.Position + glm::vec3(0, SCENE_X, 0), camera.Position, glm::vec3(0, 0, -1));
        newProj = glm::ortho(-SCENE_X * 0.8f, SCENE_X * 0.8f, -SCENE_X * 0.5f, SCENE_X * 0.5f, 0.1f, 1500.0f);
    }
    else if (viewModeKey8 == 2) { // Front Bird's
        newView = glm::lookAt(camera.Position + glm::vec3(SCENE_X * 0.8f, 180.0f, 0), camera.Position, glm::vec3(0, 1, 0));
    }
    else { // Inside / Default
        newView = view; newProj = proj;
    }

    sh.use();
    sh.setMat4("projection", newProj);
    sh.setMat4("view", newView);
    sh.setVec3("viewPos", camera.Position);
    sh.setFloat("time", gTime);
    sh.setInt("viewportMode", vpMode);
    sh.setInt("texMode", texMode);
    sh.setBool("useTexture", false);
    sh.setBool("isNightSky", false);

    // Lighting variables
    sh.setBool("dirLightOn", dirLightOn);
    sh.setBool("pointLightsOn", pointLightsOn);
    sh.setBool("spotLightOn", spotLightOn);
    sh.setBool("ambientOn", ambientOn);
    sh.setBool("diffuseOn", diffuseOn);
    sh.setBool("specularOn", specularOn);
    sh.setBool("isWater", false);
    sh.setBool("isEmissive", false);

    dirLight.setUpLight(sh);
    for (int i = 0; i < numPL; i++) pointLights[i].setUpPointLight(sh);
    // Update stadium floodlights positions
    glm::vec3 stadiumPos = glm::vec3(80, 1.5f, Z_ROW1_POS);
    float fp[][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
    for (int i = 0; i < 4; i++) {
        spotLights[i].position = stadiumPos + glm::vec3(fp[i][0] * 55.0f, 58.5f, fp[i][1] * 55.0f);
        spotLights[i].direction = glm::normalize(stadiumPos - spotLights[i].position);
    }

    // Update vehicle headlights
    int vIdx = 4;
    for (int i = 0; i < vehicles.size() && vIdx < 12; i++) {
        if (vehicles[i].dir > 0) { // Only attach to some vehicles
            float lz = vehicleLaneOffsetZ(vehicles[i].x, vehicles[i].lane);
            float shuffle = ((vehicles[i].type * 7) % 3) - 1.0f;
            float vy = bridgeDeckY(vehicles[i].x) + 0.35f;
            spotLights[vIdx].position = glm::vec3(vehicles[i].x + 4.0f, vy + 1.0f, lz + shuffle);
            spotLights[vIdx].direction = glm::vec3(1, -0.2f, 0);
            vIdx++;
        }
    }

    sh.setInt("numSpotLights", numSpotLights);
    for (int i = 0; i < numSpotLights; i++) {
        spotLights[i].setUpLight(sh, i);
    }
    glm::mat4 B(1);

    // Sky dome: depth test off so the skydome is not clipped by the far plane.
    if (texMode > 0) {
        GLboolean depthOn = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        drawSkyDome(vao, sh, B);
        glDepthMask(GL_TRUE);
        if (depthOn) glEnable(GL_DEPTH_TEST);
    }

    // Infrastructure: ground plane, river, bridge.
    // Grass background plane
    g_boundTex = texGrass;
    drawCube(vao, sh, B, glm::vec3(0, -2.5f, 0), glm::vec3(SCENE_X * GROUND_PLANE_SCALE, 0.2f, SCENE_Z * GROUND_PLANE_SCALE), glm::vec3(0.38f, 0.58f, 0.32f));
    g_boundTex = 0;
    drawRiver(vao, sh, B);
    drawBridge(vao, sh, B);

    // Static city layout: fixed positions (no per-frame random placement).
    auto drawBlock = [&](glm::vec3 pos, glm::vec3 size, glm::vec3 color) {
        drawCube(vao, sh, B, pos, size, color);
        };

    // City 1: east of river (X > 80).

    // Landmark row (Z pushed away from bridge for deck / stadium clearance).
    g_boundTex = texBurjBlk;  drawBlock(glm::vec3(120, 1, Z_ROW1_POS), glm::vec3(90, 2, 90), glm::vec3(0.72f, 0.68f, 0.62f)); g_boundTex = 0;
    g_boundTex = texBurj;     drawBurjKhalifa(vao, sh, B, glm::vec3(120, 2, Z_ROW1_POS));       g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(160, 2, 60 + CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(90, 2, 55 + CITY_ROW1_Z_PUSH));

    g_boundTex = texMallBlk;  drawBlock(glm::vec3(260, 1, Z_ROW1_POS), glm::vec3(110, 2, 90), glm::vec3(0.65f, 0.60f, 0.55f)); g_boundTex = 0;
    g_boundTex = texMall;     drawJFP(vao, sh, B, glm::vec3(260, 2, Z_ROW1_POS));              g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(300, 2, 55 + CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(225, 2, 60 + CITY_ROW1_Z_PUSH));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(420, 1, Z_ROW1_POS), glm::vec3(150, 2, 150), glm::vec3(0.60f, 0.57f, 0.52f)); g_boundTex = 0;
    drawCricketStadium(vao, sh, B, glm::vec3(420, 2, Z_ROW1_POS));

    // Row 2 (+Z): Z_ROW2_POS; trees keep same offsets from row center as original layout.
    g_boundTex = texCorpBlk; drawBlock(glm::vec3(120, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.45f, 0.42f, 0.40f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(120, 2, Z_ROW2_POS));    g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(155, 2, Z_ROW2_POS - 20)); drawTree(vao, sh, B, glm::vec3(88, 2, Z_ROW2_POS + 25));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(260, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.70f, 0.62f, 0.50f)); g_boundTex = 0;
    g_boundTex = texResid;    drawHotel(vao, sh, B, glm::vec3(260, 2, Z_ROW2_POS));             g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(295, 2, Z_ROW2_POS - 20)); drawTree(vao, sh, B, glm::vec3(228, 2, Z_ROW2_POS + 28));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(390, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.55f, 0.52f, 0.50f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(390, 2, Z_ROW2_POS), 65, glm::vec3(0.7f, 0.6f, 0.55f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(425, 2, Z_ROW2_POS - 20));

    // Row 3 (+Z): Z_ROW3_POS.
    g_boundTex = texCorpBlk; drawBlock(glm::vec3(120, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.62f, 0.58f, 0.52f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(120, 2, Z_ROW3_POS));    g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(90, 2, Z_ROW3_POS - 30)); drawTree(vao, sh, B, glm::vec3(155, 2, Z_ROW3_POS - 32));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(260, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.50f, 0.48f, 0.45f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(260, 2, Z_ROW3_POS));    g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(295, 2, Z_ROW3_POS - 30));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(390, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.68f, 0.60f, 0.48f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(390, 2, Z_ROW3_POS), 55, glm::vec3(0.85f, 0.72f, 0.60f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(355, 2, Z_ROW3_POS + 25)); drawTree(vao, sh, B, glm::vec3(425, 2, Z_ROW3_POS + 25));

    // City 1, negative Z bands (row1 pushed for bridge; rows 2–3 spaced by CITY_ROW_Z_GAP).
    g_boundTex = texResidBlk; drawBlock(glm::vec3(120, 1, Z_ROW1_NEG), glm::vec3(90, 2, 90), glm::vec3(0.60f, 0.56f, 0.52f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(120, 2, Z_ROW1_NEG), 50, glm::vec3(0.65f, 0.80f, 0.78f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(155, 2, -55 - CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(90, 2, -55 - CITY_ROW1_Z_PUSH));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(260, 1, Z_ROW1_NEG), glm::vec3(90, 2, 90), glm::vec3(0.48f, 0.46f, 0.44f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(260, 2, Z_ROW1_NEG));    g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(295, 2, -55 - CITY_ROW1_Z_PUSH));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(390, 1, Z_ROW1_NEG), glm::vec3(90, 2, 90), glm::vec3(0.55f, 0.52f, 0.50f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(390, 2, Z_ROW1_NEG));    g_boundTex = 0;

    g_boundTex = texResidBlk; drawBlock(glm::vec3(120, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.65f, 0.60f, 0.56f)); g_boundTex = 0;
    g_boundTex = texResid;    drawHotel(vao, sh, B, glm::vec3(120, 2, Z_ROW2_NEG));            g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(90, 2, Z_ROW2_NEG + 20)); drawTree(vao, sh, B, glm::vec3(155, 2, Z_ROW2_NEG + 20));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(260, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.52f, 0.50f, 0.46f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(260, 2, Z_ROW2_NEG), 50, glm::vec3(0.75f, 0.70f, 0.65f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(228, 2, Z_ROW2_NEG - 25)); drawTree(vao, sh, B, glm::vec3(295, 2, Z_ROW2_NEG - 20));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(390, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.68f, 0.64f, 0.58f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(390, 2, Z_ROW2_NEG), 60, glm::vec3(0.55f, 0.80f, 0.55f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(355, 2, Z_ROW2_NEG + 62));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(120, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.58f, 0.54f, 0.50f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(120, 2, Z_ROW3_NEG));   g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(90, 2, Z_ROW3_NEG - 25)); drawTree(vao, sh, B, glm::vec3(155, 2, Z_ROW3_NEG - 25));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(260, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.70f, 0.65f, 0.58f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(260, 2, Z_ROW3_NEG), 60, glm::vec3(0.88f, 0.78f, 0.65f)); g_boundTex = 0;

    g_boundTex = texResidBlk; drawBlock(glm::vec3(390, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.55f, 0.52f, 0.48f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(390, 2, Z_ROW3_NEG), 70, glm::vec3(0.78f, 0.62f, 0.88f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(355, 2, Z_ROW3_NEG + 25)); drawTree(vao, sh, B, glm::vec3(425, 2, Z_ROW3_NEG + 25));

    // City 2: west of river (X < -80).

    // Landmark row (Z pushed toward -Z away from bridge).
    g_boundTex = texUnivBlk;  drawBlock(glm::vec3(-120, 1, Z_ROW1_NEG), glm::vec3(110, 2, 90), glm::vec3(0.68f, 0.63f, 0.58f)); g_boundTex = 0;
    g_boundTex = texUniv;     drawUniversity(vao, sh, B, glm::vec3(-120, 2, Z_ROW1_NEG));       g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-85, 2, -55 - CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-155, 2, -55 - CITY_ROW1_Z_PUSH));

    g_boundTex = texHospBlk;  drawBlock(glm::vec3(-270, 1, Z_ROW1_NEG), glm::vec3(90, 2, 90), glm::vec3(0.62f, 0.60f, 0.57f)); g_boundTex = 0;
    g_boundTex = texHosp;     drawHospital(vao, sh, B, glm::vec3(-270, 2, Z_ROW1_NEG));         g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-235, 2, -55 - CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-305, 2, -55 - CITY_ROW1_Z_PUSH));

    g_boundTex = texEiffelBlk; drawBlock(glm::vec3(-420, 1, Z_ROW1_NEG), glm::vec3(130, 2, 130), glm::vec3(0.58f, 0.55f, 0.52f)); g_boundTex = 0;
    g_boundTex = texEiffel;    drawEiffelTower(vao, sh, B, glm::vec3(-420, 2, Z_ROW1_NEG));      g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-380, 2, -55 - CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-460, 2, -55 - CITY_ROW1_Z_PUSH));

    // City 2 row 2 (-Z).
    g_boundTex = texResidBlk; drawBlock(glm::vec3(-120, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.65f, 0.60f, 0.55f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(-120, 2, Z_ROW2_NEG), 60, glm::vec3(0.85f, 0.55f, 0.65f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-85, 2, Z_ROW2_NEG + 20)); drawTree(vao, sh, B, glm::vec3(-155, 2, Z_ROW2_NEG + 20));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(-270, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.48f, 0.46f, 0.44f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(-270, 2, Z_ROW2_NEG));  g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-235, 2, Z_ROW2_NEG - 28)); drawTree(vao, sh, B, glm::vec3(-305, 2, Z_ROW2_NEG - 28));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-390, 1, Z_ROW2_NEG), glm::vec3(90, 2, 90), glm::vec3(0.72f, 0.66f, 0.55f)); g_boundTex = 0;
    g_boundTex = texResid;    drawHotel(vao, sh, B, glm::vec3(-390, 2, Z_ROW2_NEG));           g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-355, 2, Z_ROW2_NEG + 62));

    // City 2 row 3 (-Z).
    g_boundTex = texGrass;   drawBlock(glm::vec3(-120, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.42f, 0.50f, 0.42f)); g_boundTex = 0;
    drawGarden(vao, sh, B, glm::vec3(-120, 2, Z_ROW3_NEG));
    drawTree(vao, sh, B, glm::vec3(-90, 2, Z_ROW3_NEG + 25)); drawTree(vao, sh, B, glm::vec3(-150, 2, Z_ROW3_NEG + 25));
    drawTree(vao, sh, B, glm::vec3(-90, 2, Z_ROW3_NEG - 25)); drawTree(vao, sh, B, glm::vec3(-150, 2, Z_ROW3_NEG - 25));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-270, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.60f, 0.56f, 0.52f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(-270, 2, Z_ROW3_NEG), 65, glm::vec3(0.75f, 0.58f, 0.80f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-235, 2, Z_ROW3_NEG + 25)); drawTree(vao, sh, B, glm::vec3(-305, 2, Z_ROW3_NEG + 25));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(-390, 1, Z_ROW3_NEG), glm::vec3(90, 2, 90), glm::vec3(0.50f, 0.48f, 0.46f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(-390, 2, Z_ROW3_NEG));  g_boundTex = 0;

    // City 2, positive Z bands (first band Z pushed away from bridge).
    g_boundTex = texResidBlk; drawBlock(glm::vec3(-120, 1, Z_ROW1_POS), glm::vec3(90, 2, 90), glm::vec3(0.62f, 0.58f, 0.52f)); g_boundTex = 0;
    g_boundTex = texResid;    drawHotel(vao, sh, B, glm::vec3(-120, 2, Z_ROW1_POS));             g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-85, 2, 55 + CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-155, 2, 55 + CITY_ROW1_Z_PUSH));

    g_boundTex = texGrass;   drawBlock(glm::vec3(-270, 1, Z_ROW1_POS), glm::vec3(90, 2, 90), glm::vec3(0.40f, 0.48f, 0.40f)); g_boundTex = 0;
    drawPark(vao, sh, B, glm::vec3(-270, 2, Z_ROW1_POS));
    drawTree(vao, sh, B, glm::vec3(-235, 2, 55 + CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-305, 2, 55 + CITY_ROW1_Z_PUSH));
    drawTree(vao, sh, B, glm::vec3(-270, 2, 100 + CITY_ROW1_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-250, 2, 105 + CITY_ROW1_Z_PUSH));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-390, 1, Z_ROW1_POS), glm::vec3(90, 2, 90), glm::vec3(0.68f, 0.62f, 0.54f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(-390, 2, Z_ROW1_POS), 55, glm::vec3(0.82f, 0.75f, 0.58f)); g_boundTex = 0;

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(-120, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.46f, 0.44f, 0.43f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(-120, 2, Z_ROW2_POS));   g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-85, 2, Z_ROW2_POS - 20)); drawTree(vao, sh, B, glm::vec3(-155, 2, Z_ROW2_POS - 20));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-270, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.62f, 0.60f, 0.58f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(-270, 2, Z_ROW2_POS), 55, glm::vec3(0.88f, 0.88f, 0.92f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-235, 2, Z_ROW2_POS + 28)); drawTree(vao, sh, B, glm::vec3(-305, 2, Z_ROW2_POS + 28));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(-390, 1, Z_ROW2_POS), glm::vec3(90, 2, 90), glm::vec3(0.55f, 0.52f, 0.48f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(-390, 2, Z_ROW2_POS)); g_boundTex = 0;

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-120, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.65f, 0.60f, 0.56f)); g_boundTex = 0;
    g_boundTex = texResid;    drawApartment(vao, sh, B, glm::vec3(-120, 2, Z_ROW3_POS), 55, glm::vec3(0.82f, 0.65f, 0.72f)); g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-85, 2, Z_ROW3_POS - 25)); drawTree(vao, sh, B, glm::vec3(-155, 2, Z_ROW3_POS - 25));

    g_boundTex = texCorpBlk; drawBlock(glm::vec3(-270, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.50f, 0.48f, 0.45f)); g_boundTex = 0;
    g_boundTex = texCorp;    drawCorporateOffice(vao, sh, B, glm::vec3(-270, 2, Z_ROW3_POS));   g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-235, 2, Z_ROW3_POS + 25)); drawTree(vao, sh, B, glm::vec3(-305, 2, Z_ROW3_POS + 25));

    g_boundTex = texResidBlk; drawBlock(glm::vec3(-390, 1, Z_ROW3_POS), glm::vec3(90, 2, 90), glm::vec3(0.70f, 0.65f, 0.58f)); g_boundTex = 0;
    g_boundTex = texResid;    drawHotel(vao, sh, B, glm::vec3(-390, 2, Z_ROW3_POS));            g_boundTex = 0;
    drawTree(vao, sh, B, glm::vec3(-355, 2, Z_ROW3_POS - 25)); drawTree(vao, sh, B, glm::vec3(-425, 2, Z_ROW3_POS - 25));

    // Trees at bridge approaches.
    drawTree(vao, sh, B, glm::vec3(-80, 2, BRIDGE_APPROACH_TREE_Z)); drawTree(vao, sh, B, glm::vec3(-80, 2, -BRIDGE_APPROACH_TREE_Z));
    drawTree(vao, sh, B, glm::vec3(80, 2, BRIDGE_APPROACH_TREE_Z)); drawTree(vao, sh, B, glm::vec3(80, 2, -BRIDGE_APPROACH_TREE_Z));

    // River bank: eight trees; |Z| increased with wider bridge to keep clearance along the span.
    drawTree(vao, sh, B, glm::vec3(52, 2, 95 + RIVER_BANK_TREE_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(65, 2, 135 + RIVER_BANK_TREE_Z_PUSH));
    drawTree(vao, sh, B, glm::vec3(52, 2, -(95 + RIVER_BANK_TREE_Z_PUSH))); drawTree(vao, sh, B, glm::vec3(65, 2, -(135 + RIVER_BANK_TREE_Z_PUSH)));
    drawTree(vao, sh, B, glm::vec3(-52, 2, 95 + RIVER_BANK_TREE_Z_PUSH)); drawTree(vao, sh, B, glm::vec3(-65, 2, 135 + RIVER_BANK_TREE_Z_PUSH));
    drawTree(vao, sh, B, glm::vec3(-52, 2, -(95 + RIVER_BANK_TREE_Z_PUSH))); drawTree(vao, sh, B, glm::vec3(-65, 2, -(135 + RIVER_BANK_TREE_Z_PUSH)));

    // Animated vehicles on the bridge deck.
    for (auto& veh : vehicles) {
        float lz = vehicleLaneOffsetZ(veh.x, veh.lane);
        float vy = bridgeDeckY(veh.x) + 0.35f;
        drawVehicle(vao, sh, B, glm::vec3(veh.x, vy, lz), veh.type, veh.dir);
    }

    // Ships on the river.
    for (auto& s : ships) {
        if (s.type == 0) {
            g_boundTex = texCargo;
            drawCargoShip(vao, sh, B, glm::vec3(s.xOff, 0.5f, s.z), s.dir);
        }
        else {
            g_boundTex = texHarmony;
            drawHarmonyShip(vao, sh, B, glm::vec3(s.xOff, -0.5f, s.z), s.dir);
        }
        g_boundTex = 0;
    }

    // Aeroplane (simplified airliner mesh).
    g_boundTex = texEmirates;
    drawAeroplane(vao, sh, B, glm::vec3(fmod(gTime * kAeroplanePathSpeed, 2000.0f) - 1000.0f, 280.0f, 50.0f), 0.0f);
    g_boundTex = 0;

    // Sun or moon disc and optional sun rays.
    sh.setBool("isEmissive", true);
    if (!isNight) {
        glm::vec3 sunP = glm::vec3(120, 320, 150);
        g_boundTex = texSun;
        drawSphere(sh, B, sunP, 24.0f, glm::vec3(1.0f, 0.95f, 0.5f), true);
        g_boundTex = 0;
        // Sun: radial ray quads.
        glm::vec3 rayC(1.0f, 0.92f, 0.15f);
        for (int ri = 0; ri < 8; ri++) {
            float ang = glm::radians(ri * 45.0f + 90.0f); // 90° rotation
            glm::mat4 m = glm::translate(B, sunP);
            m = myRotate(m, ang, glm::vec3(0, 0, 1));
            drawCube(vao, sh, m, glm::vec3(0, 32.0f, 0), glm::vec3(1.2f, 18.0f, 1.2f), rayC);
        }
    }
    else {
        g_boundTex = texMoon;
        drawSphere(sh, B, glm::vec3(-400, 200, -150), 18.0f, glm::vec3(1.0f, 0.98f, 0.95f), true);
        g_boundTex = 0;
    }
    sh.setBool("isEmissive", false);
}


// MAIN
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
        "3D Smart City - CSE 4208 Computer Graphics", NULL, NULL);
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
    buildCylinder(18);
    buildSphere(12, 18);
    setupPointLights();
    initVehicles();

    // Texture loading.
    // Image files: project directory, textures/ subfolder, or parent paths (see loadTextureSafe).
    std::cout << "[TEX] Working dir hint: place JPG/PNG next to .vcxproj or in textures\\ subfolder.\n";
    auto L = [&](const char* n) { return loadTextureSafe(n); };

    texDaySky = L("day_sky.jpg");
    texNightSky = L("night_sky.jpg");
    texSun = L("sun.png");
    texMoon = L("moon.png");
    texStem = L("stem.jpg");
    texTree = L("tree.jpg");
    texBridgeRoad = L("bridge_highway.jpg");
    texPillar = L("pillar.png");
    texCable = L("cable.jpg");
    texRiver = L("river.jpeg");
    texCargo = L("cargo.png");
    texHarmony = L("harmony.png");
    texEmirates = L("emirates.png");
    texStadium = L("stadium.png");
    texStadiumIn = L("stadium_inside.png");
    texEiffel = L("eye_fell.png");
    texEiffelBlk = L("eye_fell_block.jpg");
    texBurj = L("burj_facade.jpg");
    texBurjBlk = L("burj_khalifa_block.jpg");
    texCorp = L("corporate.jpg");
    texCorpBlk = L("corporate_block.jpg");
    texMall = L("jfp_building.png");
    texMallBlk = L("shopping_mall_block.jpg");
    texUniv = L("university_building.jpg");
    texUnivBlk = L("university_block.jpg");
    texResid = L("residential_apartment_building.jpg");
    texResidBlk = L("residential_apartment_block.jpg");
    texHosp = L("hospital_building.jpg");
    texHospBlk = L("hospital_block.jpg");
    texGrass = L("green_grass.jpg");
    std::cout << "All textures loaded. Press Key 9 to cycle texture modes.\n";
    std::cout << "\n=== Animation speed and input (main.cpp, camera.h); line numbers change if the file is edited ===\n"
        << "  Bridge vehicle speeds      initVehicles()   sp[5] array                    line 1712\n"
        << "  Ship speeds / lanes        initVehicles()   ships.push_back {...}          lines 1722-1725\n"
        << "  Vehicle + ship motion      updateVehicles() v.x += ... ; s.z += ...       lines 1731-1740\n"
        << "  Aeroplane along-X speed    global           kAeroplanePathSpeed            line 94\n"
        << "  Aeroplane position         renderScene()    fmod(gTime*kAeroplanePathSpeed,...) line 2154\n"
        << "  River surface waves        vertexShaderForPhongShading.vs  isWater + uniform time (lines 14-26)\n"
        << "  River ripple drift         drawRiver()      gTime * 10.5f, gTime * 2.6f   lines 1588-1590\n"
        << "  Camera move / rotate       camera.h         MoveSpeed, RotSpeed            line 33\n"
        << "  Continuous keys (WASD)     processInput()                                  line 2371\n"
        << "  Toggle keys (1-9,L,T,4,8) key_callback()                                  line 2395\n"
        << "================================================================================\n\n";

    // Spot lights 0-3: stadium floodlights.
    for (int i = 0; i < 4; i++) {
        spotLights[i].cutOff = cosf(glm::radians(15.0f));
        spotLights[i].outerCutOff = cosf(glm::radians(15.0f)); // Same as inner cone (hard edge).
        spotLights[i].ambient = glm::vec3(0.1f);
        spotLights[i].diffuse = glm::vec3(1.0f, 1.0f, 0.9f);
        spotLights[i].specular = glm::vec3(1.0f);
        spotLights[i].k_l = 0.005f;
        spotLights[i].k_q = 0.0002f;
    }
    // Spot lights 4-11: vehicle headlights.
    for (int i = 4; i < 12; i++) {
        spotLights[i].cutOff = cosf(glm::radians(20.0f));
        spotLights[i].outerCutOff = cosf(glm::radians(20.0f)); // Same as inner cone (hard edge).
        spotLights[i].ambient = glm::vec3(0.0f);
        spotLights[i].diffuse = glm::vec3(0.9f, 0.9f, 0.8f);
        spotLights[i].specular = glm::vec3(0.5f);
        spotLights[i].k_l = 0.02f;
        spotLights[i].k_q = 0.001f;
    }

    cout << "============================================================\n"
        << "   3D SMART CITY ENVIRONMENT SIMULATION\n"
        << "   CSE 4208 - Computer Graphics\n"
        << "============================================================\n\n"
        << "  CAMERA MOVEMENT (speed: MoveSpeed=" << camera.MoveSpeed << "):\n"
        << "    W / S        Forward / Backward\n"
        << "    A / D        Left / Right\n"
        << "    E / R        Up / Down\n\n"
        << "  CAMERA ROTATION (speed: RotSpeed=" << camera.RotSpeed << "):\n"
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
        << "    9            Texture modes: OFF / simple / blend / vertex / fragment color\n"
        << "                 (default: SIMPLE texture mode; key 9 cycles modes)\n"
        << "============================================================\n"
        << "  Animation line refs: see block after \"All textures loaded\" (sp[], ships, kAeroplanePathSpeed, drawRiver, shaders)\n"
        << "  Texture code: loadTextureSafe() + g_boundTex variable in renderScene()\n"
        << "============================================================\n\n";

    while (!glfwWindowShouldClose(window))
    {
        float cf = (float)glfwGetTime();
        deltaTime = cf - lastFrame;
        lastFrame = cf;
        gTime = cf;
        // deltaTime uncapped except below: large hitches clamped to avoid a single huge integration step.
        if (deltaTime > 0.25f) deltaTime = 0.25f;  // clamp after long stalls
        processInput(window);
        updateVehicles(deltaTime);

        // Textured sky: clear color matched to horizon to limit visible background gaps.
        if (texMode > 0 && !isNight)
            glClearColor(0.53f, 0.78f, 0.92f, 1.0f);
        else if (texMode > 0 && isNight)
            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
        else
            glClearColor(isNight ? 0.01f : 0.42f, isNight ? 0.01f : 0.72f, isNight ? 0.06f : 0.98f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glm::mat4 view = camera.GetViewMatrix();

        if (!splitViewport)
        {
            glViewport(0, 0, fbW, fbH);
            glm::mat4 proj = myPerspective(glm::radians(camera.Zoom), (float)fbW / fbH, 0.5f, 15000.0f);
            renderScene(cubeVAO, lightSh, proj, view, 0);
        }
        else
        {
            int hW = fbW / 2, hH = fbH / 2;
            glm::mat4 proj = myPerspective(glm::radians(camera.Zoom), (float)hW / hH, 0.5f, 15000.0f);
            // Top-left: combined
            glViewport(0, hH, hW, hH);
            renderScene(cubeVAO, lightSh, proj, view, 0);
            // Top-right: ambient only (top-down view)
            glViewport(hW, hH, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(0, 450, 0.1f), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1)), 1);
            // Bottom-left: diffuse only (front view)
            glViewport(0, 0, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(0, 80, 320), glm::vec3(0, 40, 0), glm::vec3(0, 1, 0)), 2);
            // Bottom-right: directional only (isometric)
            glViewport(hW, 0, hW, hH);
            renderScene(cubeVAO, lightSh, proj,
                glm::lookAt(glm::vec3(260, 200, 260), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)), 3);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &cubeVAO);
    glfwTerminate();
    return 0;
}

// Input
void processInput(GLFWwindow* w)
{
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true);

    // Translation along camera axes.
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) camera.MoveForward(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) camera.MoveBackward(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) camera.MoveLeft(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) camera.MoveRight(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) camera.MoveUp(deltaTime);
    if (glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS) camera.MoveDown(deltaTime);

    // Rotation: hold Shift for opposite direction.
    float ra = camera.RotSpeed * deltaTime;
    bool  sh = (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) camera.RotatePitch(sh ? -ra : ra);
    if (glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) camera.RotateYaw(sh ? -ra : ra);
    if (glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) camera.RotateRoll(sh ? -ra : ra);

    // Orbit mode (F key).
    if (glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS) camera.OrbitLeft(deltaTime);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_T && action == GLFW_PRESS)
        trafficGreen = !trafficGreen;
    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
        dirLightOn = !dirLightOn;
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
        pointLightsOn = !pointLightsOn;
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
        spotLightOn = !spotLightOn;
        std::cout << "Spot lights: " << (spotLightOn ? "ON" : "OFF") << " (see spotLight.h default cone cosines; main() overrides for stadium and vehicles).\n";
    }
    if (key == GLFW_KEY_4 && action == GLFW_PRESS)
        splitViewport = !splitViewport; // Four-pane diagnostic layout; independent of key 8 camera presets.
    if (key == GLFW_KEY_5 && action == GLFW_PRESS)
        ambientOn = !ambientOn;
    if (key == GLFW_KEY_6 && action == GLFW_PRESS)
        diffuseOn = !diffuseOn;
    if (key == GLFW_KEY_7 && action == GLFW_PRESS)
        specularOn = !specularOn;
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        isNight = !isNight;
        if (isNight) dirLight.setNight();
        else dirLight.setDay();
    }
    if (key == GLFW_KEY_8 && action == GLFW_PRESS) {
        viewModeKey8 = (int)(viewModeKey8 + 1) % 4; // 0=Iso, 1=Top, 2=Front, 3=Inside
        const char* vNames[] = { "Isometric","Top","Front","Normal" };
        std::cout << "View: " << vNames[viewModeKey8] << "\n";
    }
    if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
        texMode = (texMode + 1) % 5; // 0=off 1=simple 2=blend 3=vertex 4=frag
        const char* tNames[] = { "OFF","Simple Texture","Blended Texture+Color","Vertex Color","Fragment Color" };
        std::cout << "Texture Mode [9]: " << tNames[texMode] << "\n";
    }
}

void framebuffer_size_callback(GLFWwindow*, int, int) {}
void scroll_callback(GLFWwindow*, double, double y) { camera.ProcessMouseScroll((float)y); }


