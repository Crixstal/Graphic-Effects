#pragma once

#include <array>

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

#include "npr_scene.h"

class demo_npr : public demo
{
public:
    demo_npr(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_npr();
    virtual void Update(const platform_io& IO);

    void RenderNPRModel(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {};

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint VAO_NPR = 0;

    npr_scene NPRScene;

    bool Wireframe = false;
};
