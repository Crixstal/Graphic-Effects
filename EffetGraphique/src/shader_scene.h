#pragma once

#include <vector>

#include "opengl_helpers.h"

class shader_scene
{
public:
    shader_scene(GL::cache& GLCache);
    ~shader_scene();

    // Mesh
    GLuint MeshBuffer = 0;
    int MeshVertexCount = 0;
    vertex_descriptor MeshDesc;

    // Lights buffer
    GLuint LightsUniformBuffer = 0;
    int LightCount = 1;

    // ImGui debug function to edit lights
    void InspectLights();

    // Lights data
    GL::light Light;
};