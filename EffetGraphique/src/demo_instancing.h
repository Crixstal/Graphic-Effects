#pragma once

#include "demo.h"

#include "opengl_headers.h"

#include "maths.h"
#include "camera.h"

class demo_instancing : public demo
{
public:
    demo_instancing();
    virtual ~demo_instancing();
    virtual void Update(const platform_io& IO);
    void DisplayDebugUI();
    void Render(const platform_io& IO);
private:

    // 3d camera
    camera Camera = {};
    
    // Shader programs
    GLuint Program = 0;     // Base shader
    GLuint SBProgram = 0;   // Skybox shader
    GLuint INSTProgram = 0; // Instantiate shader

    // Textures/cubemaps
    GLuint Texture = 0;
    GLuint customTexture = 0;
    GLuint skybox = 0;

    // Meshes
    // Quad
    GLuint VAO = 0;
    GLuint VertexBuffer = 0;
    int VertexCount = 0;

    // Cube
    GLuint cubeVAO = 0;
    GLuint cubeVertexBuffer = 0;
    int cubeVertexCount = 0;

    // Sphere
    GLuint sphereVAO = 0;
    GLuint sphereVertexBuffer = 0;
    GLuint instanceBuffer = 0;
    int sphereVertexCount = 0;

    int texWidth = 0;
    int texHeight = 0;

    bool showDebugTextures = false;

    // Misc
    float timeScale = 0.75f;
    int amountToInstantiate = 100;
    v3 translations[100] = {};
};
