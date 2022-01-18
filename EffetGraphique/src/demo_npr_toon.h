#pragma once

#include <array>

#include "demo.h"
#include "opengl_headers.h"
#include "camera.h"
#include "npr_toon_scene.h"

class demo_npr_toon : public demo
{
public:
    demo_npr_toon(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_npr_toon();
    virtual void Update(const platform_io& IO);

    void RenderNPRModel(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {3.55, 6.36, 27.14, -0.24, -0.31};

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint VAO_NPR = 0;

    npr_toon_scene NPRScene;

    bool Outline = false;
    bool FiveTone = false;
    bool ToonShading = false;
};

