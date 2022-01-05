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
private:
    // 3d camera
    camera Camera = {};
    
    // Shader programs
    GLuint Program = 0; // Base
    GLuint SBProgram = 0;
    GLuint RFXProgram = 0; // Reflection shader

    // Textures/cubemaps
    GLuint Texture = 0;
    GLuint textureCamille = 0;
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

    int texWidth = 0;
    int texHeight = 0;

    bool showDebugTextures = false;

    // Misc
    float timeScale = 0.75f;
};
