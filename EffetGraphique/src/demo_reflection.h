#pragma once

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

class demo_reflection : public demo
{
public:
    demo_reflection();
    virtual ~demo_reflection();
    virtual void Update(const platform_io& IO);
    void DisplayDebugUI();
    void Render(const platform_io& IO, bool renderMirrorEffects, camera* customCamera = nullptr, mat4* projMat = nullptr);
private:
    void CreateCubemapFromModelMat(mat4 modelMat, const platform_io& IO);
    void CreateCubemapFromPos(v3 camPos, const platform_io& IO);

    // 3d camera
    camera Camera = {};
    
    // Shader programs
    GLuint Program = 0; // Base
    GLuint SBProgram = 0;
    GLuint RFXProgram = 0; // Reflection shader
    GLuint RFRProgram = 0; // Refraction shader

    // Textures/cubemaps
    GLuint Texture = 0;
    GLuint customTexture = 0;
    GLuint skybox = 0;
    GLuint reflectionCubemap = 0;

    // Meshes
    // Quad
    GLuint VAO = 0;
    GLuint VertexBuffer = 0;
    int VertexCount = 0;

    // Cube
    GLuint cubeVAO = 0;
    GLuint cubeVertexBuffer = 0;
    int cubeVertexCount = 0;

    GLuint sphereVAO = 0;
    GLuint sphereVertexBuffer = 0;
    int sphereVertexCount = 0;

    int texWidth = 0;
    int texHeight = 0;

    bool showDebugTextures = false;

    // Misc
    float timeScale = 0.75f;
    bool showRefraction = false;
};
