#pragma once

#include <array>

#include "demo.h"
#include "opengl_headers.h"
#include "camera.h"
#include "npr_gooch_scene.h"

class demo_npr_gooch : public demo
{
public:
    demo_npr_gooch(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_npr_gooch();
    virtual void Update(const platform_io& IO);

    void RenderNPRModel(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {2.44, 2.54, -0.66, -0.59, -0.35};

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint VAO_NPR = 0;

    npr_gooch_scene NPRScene;

    bool GoochShading = false;
};
