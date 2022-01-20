
#include <vector>
#include <iostream>

#include <imgui.h>

#include "opengl_helpers.h"
#include "maths.h"
#include "mesh.h"
#include "color.h"

#include "demo_instancing.h"

#include "pg.h"

// Vertex format
// ==================================================
struct vertex
{
    v3 Position;
    v3 Normal;
    v2 UV;
};

#pragma region SHADERS
#pragma region BASE SHADER
static const char* gVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

// Uniforms
uniform mat4 uModelViewProj;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = uModelViewProj * vec4(aPosition, 1.0);
})GLSL";

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;

// Uniforms
uniform sampler2D uColorTexture;

// Shader outputs
out vec4 oColor;

void main()
{
    oColor = texture(uColorTexture, vUV);
})GLSL";
#pragma endregion
#pragma region SKYBOX SHADER
static const char* sbVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;

// Uniforms
uniform mat4 uViewProj;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec3 vUV;

void main()
{
    vUV = vec3(aPosition.xy, -aPosition.z);
    vec4 pos = uViewProj * vec4(aPosition, 1.0);
    gl_Position = pos.xyww;
})GLSL";

static const char* sbFragmentShaderStr = R"GLSL(
// Varyings
in vec3 vUV;

// Uniforms
uniform samplerCube uSkybox;

// Shader outputs
out vec4 oColor;

void main()
{
    oColor = texture(uSkybox, vUV);
})GLSL";
#pragma endregion
#pragma region INSTANCED SHADER
static const char* instVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aOffset;

// Uniforms
uniform mat4 uViewProj;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec2 vUV;

void main()
{
    vUV = aUV;
    vec4 pos = vec4(aPosition + aOffset, 1.0);
    gl_Position = uViewProj * pos;
})GLSL";

static const char* instFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;

// Uniforms
uniform sampler2D uColorTexture;

// Shader outputs
out vec4 oColor;

void main()
{
    oColor = texture(uColorTexture, vUV);
})GLSL";
#pragma endregion
#pragma endregion

static void DrawQuad(GLuint Program, mat4 ModelViewProj)
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, ModelViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#pragma region CONSTRUCTOR/DESTRUCTOR
demo_instancing::demo_instancing()
{
    // Create render pipeline
    this->Program = GL::CreateProgram(gVertexShaderStr, gFragmentShaderStr);
    SBProgram = GL::CreateProgram(sbVertexShaderStr, sbFragmentShaderStr);
    INSTProgram = GL::CreateProgram(instVertexShaderStr, instFragmentShaderStr);

    // Create a descriptor based on the `struct vertex` format
    vertex_descriptor Descriptor = {};
    Descriptor.Stride = sizeof(vertex);
    Descriptor.HasNormal = true;
    Descriptor.HasUV = true;
    Descriptor.PositionOffset = OFFSETOF(vertex, Position);
    Descriptor.NormalOffset = OFFSETOF(vertex, Normal);
    Descriptor.UVOffset = OFFSETOF(vertex, UV);

    // Gen quad
    {
        // Create a quad in RAM
        vertex Quad[6];
        this->VertexCount = 6;
        Mesh::BuildQuad(Quad, Quad + this->VertexCount, Descriptor);

        // Upload quad to gpu (VRAM)
        glGenBuffers(1, &this->VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, this->VertexCount * sizeof(vertex), Quad, GL_STATIC_DRAW);
    
        // Create quad vertex array
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, UV));
    }

    // Gen cube
    {
        // Create a cube in RAM
        vertex Cube[36];
        cubeVertexCount = 36;
        Mesh::BuildNormalizedCube(Cube, Cube + cubeVertexCount, Descriptor);

        // Upload cube to gpu (VRAM)
        glGenBuffers(1, &cubeVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, cubeVertexCount * sizeof(vertex), Cube, GL_STATIC_DRAW);
    
        // Create cube vertex array
        glGenVertexArrays(1, &cubeVAO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, this->cubeVertexBuffer);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, UV));
    }

    // Gen texture
    {
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        GL::UploadCheckerboardTexture(64, 64, 8);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // Gen custom texture
    {
        glGenTextures(1, &customTexture);
        glBindTexture(GL_TEXTURE_2D, customTexture);
        GL::UploadTexture("media/roh.png", image_flags::IMG_FLIP, &texWidth, &texHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // Gen skybox
    {
        std::string sbName = "media/skybox/skybox";
        std::string fileExtension = ".jpg";

        glGenTextures(1, &skybox);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);

        for (int i = 0; i < 6; i++)
        {
            std::string texName = sbName + std::to_string(i) + fileExtension;
            GL::UploadCubemapTexture(texName.c_str(), i, image_flags::IMG_FORCE_RGB, &texWidth, &texHeight);
        }

        // Texture filters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        // Clamp UV Coords to [0;1], avoids white edges
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    // Gen instance positions
    {
        int i = 0;
        for (int y = -10; y < 10; y += 2)
            for (int x = -10; x < 10; x += 2)
            {
                translations[i] = { (float)x, (float)y, 0.f };
                i++;
            }
    }

    // Gen sphere
    {
        const int lon = 25;
        const int lat = 25;
        // Create a sphere in RAM
        vertex Sphere[lon * lat * 6];
        sphereVertexCount = lon * lat * 6;
        Mesh::BuildSphere(Sphere, Sphere + sphereVertexCount, Descriptor, lon, lat);

        // Upload sphere to gpu (VRAM)
        glGenBuffers(1, &sphereVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sphereVertexCount * sizeof(vertex), Sphere, GL_STATIC_DRAW);

        glGenBuffers(1, &instanceBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v3) * 100, &translations[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Create sphere vertex array
        glGenVertexArrays(1, &sphereVAO);
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVertexBuffer);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, UV));

        // Bind instance relative pos
        glEnableVertexAttribArray(3);
        glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer); 
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glVertexAttribDivisor(3, 1);

    }
}

demo_instancing::~demo_instancing()
{
    // Cleanup GL
    glDeleteTextures(1, &Texture);
    glDeleteTextures(1, &customTexture);
    glDeleteTextures(1, &skybox);

    glDeleteBuffers(1, &VertexBuffer);
    glDeleteBuffers(1, &cubeVertexBuffer);
    glDeleteBuffers(1, &sphereVertexBuffer);
    glDeleteBuffers(1, &instanceBuffer);

    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &sphereVAO);

    glDeleteProgram(Program);
    glDeleteProgram(SBProgram);
}
#pragma endregion

void demo_instancing::Update(const platform_io& IO)
{
    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Bind main buffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Setup GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);

    // Clear screen
    glClearColor(0.2f, 0.2f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    Render(IO);

    // ImGui
    DisplayDebugUI();
}

void demo_instancing::Render(const platform_io& IO)
{
    // Compute model-view-proj and send it to shader
    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), (float)IO.WindowWidth / (float)IO.WindowHeight, 0.1f, 1000.f);
    
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);

    mat4 vp = ProjectionMatrix * ViewMatrix;

    // Draw origin
    PG::DebugRenderer()->DrawAxisGizmo(Mat4::Translate({ 0.f, 0.f, 0.f }), true, true);

    mat4 ModelMatrix = Mat4::Translate({ 0.f, -1.f * sinf(IO.Time), -1.f });
    mat4 mvp = ProjectionMatrix * ViewMatrix * ModelMatrix;
    glUseProgram(Program);
    glBindTexture(GL_TEXTURE_2D, Texture);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, mvp.e);
    glBindVertexArray(sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);

    // Spheres
    {
        glUseProgram(INSTProgram);
        glBindTexture(GL_TEXTURE_2D, customTexture);
        glUniformMatrix4fv(glGetUniformLocation(INSTProgram, "uViewProj"), 1, GL_FALSE, vp.e);

        for (int i = 3; i < 4; i++)
        {
            glUseProgram(INSTProgram);
            std::string uniformName = "uOffsets[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(INSTProgram, uniformName.c_str()), 1, translations[i].e);
        }

        glBindVertexArray(sphereVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, sphereVertexCount, 100);
    }

    // Skybox
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);

        glDepthMask(GL_FALSE);

        mat4 rotateOnlyViewMatrix = ViewMatrix;
        // Sets translation to 0
        rotateOnlyViewMatrix.c[3].x = 0;
        rotateOnlyViewMatrix.c[3].y = 0;
        rotateOnlyViewMatrix.c[3].z = 0;
        vp = ProjectionMatrix * rotateOnlyViewMatrix;

        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
        glUseProgram(SBProgram);
        glUniformMatrix4fv(glGetUniformLocation(SBProgram, "uViewProj"), 1, GL_FALSE, vp.e);
        glBindVertexArray(cubeVAO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glDepthMask(GL_TRUE);
    }
}

void demo_instancing::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_skybox", ImGuiTreeNodeFlags_Framed))
    {
        // Debug display
        if (ImGui::TreeNodeEx("Camera"))
        {
            if (ImGui::Button("Reset camera position"))
            {
                Camera.Position = {};
                Camera.Pitch = 0;
                Camera.Yaw = 0;
            }

            ImGui::Text("Position: (%.2f, %.2f, %.2f)", Camera.Position.x, Camera.Position.y, Camera.Position.z);
            ImGui::Text("Pitch: %.2f", Math::ToDegrees(Camera.Pitch));
            ImGui::Text("Yaw: %.2f", Math::ToDegrees(Camera.Yaw));
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Skybox"))
        {
            ImGui::Text("VAO: %d", cubeVAO);
            ImGui::Text("Program: %d", SBProgram);
            ImGui::Text("Cubemap: %d", skybox);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("demo_instancing", ImGuiTreeNodeFlags_Framed))
    {
        ImGui::Checkbox("Show loaded textures", &showDebugTextures);
        if (showDebugTextures)
        {
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(128, 128));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)customTexture, ImVec2(128, 128));
            //ImGui::Image((void*)(intptr_t)skybox, ImVec2(128, 128));
        }
        
        ImGui::TreePop();
    }
}
