#pragma once

#include "demo.h"
#include "opengl_headers.h"
#include "camera.h"
#include "shader_scene.h"

class demo_shader : public demo
{
public:
    demo_shader(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_shader();
    virtual void Update(const platform_io& IO);

    void Render(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = { 1.78, 1.63, 5.23, -0.28, -0.28 };

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint VAO = 0;

    shader_scene ShaderScene;

    bool FlatShading = true;
    bool GouraudShading = false;
    bool PhongShading = false;
    bool BlinnPhongShading = false;

    GLfloat ka = 0.0f;
    GLfloat kd = 0.5f;    
    GLfloat ks = 1.0f;
};