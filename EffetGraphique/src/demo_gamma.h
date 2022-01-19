#pragma once

#include <array>

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

#include "tavern_scene.h"

class demo_gamma : public demo
{
public:
    demo_gamma(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_gamma();
    virtual void Update(const platform_io& IO);

    void RenderTavern(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {};

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint VAO = 0;

    tavern_scene TavernScene;

    bool Wireframe = false;
    bool useGamma = false;
    bool useCustomGamma = false;
};
