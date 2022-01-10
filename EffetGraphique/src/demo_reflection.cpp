
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "maths.h"
#include "mesh.h"
#include "color.h"

#include "demo_reflection.h"

#include "pg.h"

#define REFLECTION_RES_WIDTH 1440
#define REFLECTION_RES_HEIGHT 900
#define REFLECTION_FAR_PLANE 50.f

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
#pragma region REFLECTION SHADER
static const char* rfxVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

// Uniforms
uniform mat4 uModel;
uniform mat4 uViewProj;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec3 vNormal;
out vec3 vPos;

void main()
{
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vPos = vec3(uModel * vec4(aPosition, 1.0));
    gl_Position = uViewProj * vec4(vPos, 1.0);
})GLSL";

static const char* rfxFragmentShaderStr = R"GLSL(
// Varyings
in vec3 vNormal;
in vec3 vPos;

// Uniforms
uniform vec3 uCamPos;
uniform samplerCube uSkybox;

// Shader outputs
out vec4 oColor;

void main()
{
    vec3 viewVec = normalize(vPos - uCamPos);
    vec3 reflectVec = reflect(viewVec, normalize(vNormal));
    reflectVec.z = -reflectVec.z;
    oColor = texture(uSkybox, reflectVec);
})GLSL";
#pragma endregion
#pragma region REFRACTION SHADER
// Vertex shader is the same as reflection shader, avoiding copy
static const char* rfrFragmentShaderStr = R"GLSL(
// Varyings
in vec3 vNormal;
in vec3 vPos;

// Uniforms
uniform vec3 uCamPos;
uniform samplerCube uSkybox;

// Shader outputs
out vec4 oColor;

void main()
{
    // Common refraction indexes
    //Air = 1.00, Water = 1.33, Ice = 1.309, Glass = 1.52, Diamond = 2.42
    
    float rfrRatio = 1.0 / 1.52;
    vec3 viewVec = normalize(vPos - uCamPos);
    vec3 reflectVec = refract(viewVec, normalize(vNormal), rfrRatio);
    reflectVec.z = -reflectVec.z;
    oColor = texture(uSkybox, reflectVec);
})GLSL";
#pragma endregion
#pragma endregion

static void DrawQuad(GLuint Program, mat4 ModelViewProj)
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, ModelViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void DrawQuad(GLuint Program, mat4 Model, mat4 ViewProj)
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModel"), 1, GL_FALSE, Model.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uViewProj"), 1, GL_FALSE, ViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#pragma region CONSTRUCTOR/DESTRUCTOR
demo_reflection::demo_reflection()
{
    // Create render pipeline
    this->Program = GL::CreateProgram(gVertexShaderStr, gFragmentShaderStr);
    SBProgram = GL::CreateProgram(sbVertexShaderStr, sbFragmentShaderStr);
    RFXProgram = GL::CreateProgram(rfxVertexShaderStr, rfxFragmentShaderStr);
    RFRProgram = GL::CreateProgram(rfxVertexShaderStr, rfrFragmentShaderStr);
 
    // Framebuffer/texture used to generate reflection/refraction cubemaps
    for (int i = 0; i < 6; i++)
    {
        // Framebuffer
        glGenFramebuffers(1, &reflectionFramebuffers[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionFramebuffers[i]);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        // Texture
        glGenTextures(1, &reflectionTextures[i]);
        glBindTexture(GL_TEXTURE_2D, reflectionTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, REFLECTION_RES_WIDTH, REFLECTION_RES_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        // Attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionTextures[i], 0);
    }

    glGenTextures(1, &reflectionCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);

    // Create a descriptor based on the `struct vertex` format
    vertex_descriptor Descriptor = {};
    Descriptor.Stride = sizeof(vertex);
    Descriptor.HasNormal = true;
    Descriptor.HasUV = true;
    Descriptor.PositionOffset = OFFSETOF(vertex, Position);
    Descriptor.NormalOffset = OFFSETOF(vertex, Normal);
    Descriptor.UVOffset = OFFSETOF(vertex, UV);

    // Gen mesh
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

        // Create sphere vertex array
        glGenVertexArrays(1, &sphereVAO);
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, this->sphereVertexBuffer);
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
}

demo_reflection::~demo_reflection()
{
    // Cleanup GL
    glDeleteTextures(1, &Texture);
    glDeleteTextures(1, &skybox);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(Program);
    glDeleteProgram(SBProgram);
}
#pragma endregion

void demo_reflection::Update(const platform_io& IO)
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
    
    Render(IO, true);

    // ImGui
    DisplayDebugUI();
}

void demo_reflection::Render(const platform_io& IO, bool renderMirrorEffects, camera* customCamera, mat4* projMat)
{
    camera renderCamera = {};
    if (customCamera == nullptr)
        renderCamera = Camera;
    else
        renderCamera = *customCamera;

    // Compute model-view-proj and send it to shader
    mat4 ProjectionMatrix;
    if (projMat == nullptr)
        ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), (float)IO.WindowWidth / (float)IO.WindowHeight, 0.1f, 100.f);
    else
        ProjectionMatrix = *projMat;
    mat4 ViewMatrix = CameraGetInverseMatrix(renderCamera);

    // Draw origin
    PG::DebugRenderer()->DrawAxisGizmo(Mat4::Translate({ 0.f, 0.f, 0.f }), true, true);

    glBindTexture(GL_TEXTURE_2D, customTexture);

    // Sphere
    {
        glUseProgram(Program);
        mat4 ModelMatrix = Mat4::Translate({ 0.f, -1.f * sinf(IO.Time), -1.f });
        mat4 mvp = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, mvp.e);
        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);

    // Use reflection/refraction shader and send data
    if (showRefraction)
    {
        glUseProgram(RFRProgram);
        glUniform1f(glGetUniformLocation(RFRProgram, "uTime"), (float)IO.Time);
        glUniform3fv(glGetUniformLocation(RFRProgram, "uCamPos"), 1, renderCamera.Position.e);
    }
    else
    {
        glUseProgram(RFXProgram);
        glUniform1f(glGetUniformLocation(RFXProgram, "uTime"), (float)IO.Time);
        glUniform3fv(glGetUniformLocation(RFXProgram, "uCamPos"), 1, renderCamera.Position.e);
    }

    // Mirrors
    if (renderMirrorEffects)
    {
        glBindVertexArray(VAO); // Bind quad mesh
        v3 ObjectPosition = { 0.f, 0.f, -3.f };

        CreateCubemapFromPos(ObjectPosition, IO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);

        mat4 ModelMatrix = Mat4::Translate(ObjectPosition);
        ModelMatrix = ModelMatrix * Mat4::RotateY(IO.Time * timeScale);

        DrawQuad(showRefraction ? RFRProgram : RFXProgram, ModelMatrix, ProjectionMatrix * ViewMatrix);

        ModelMatrix = ModelMatrix * Mat4::RotateY(Math::Pi());
        DrawQuad(showRefraction ? RFRProgram : RFXProgram, ModelMatrix, ProjectionMatrix * ViewMatrix);

        //CreateCubemapFromPos(ObjectPosition, IO);
        //glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        ModelMatrix = Mat4::Identity() * Mat4::RotateY(Math::HalfPi()) * Mat4::Translate({ 0.f, 0.f, -3.f });
        DrawQuad(showRefraction ? RFRProgram : RFXProgram, ModelMatrix, ProjectionMatrix * ViewMatrix);

        //CreateCubemapFromPos(ObjectPosition, IO);
        //glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        ModelMatrix = Mat4::Identity() * Mat4::RotateX(Math::HalfPi()) * Mat4::Translate({ 0.f, 0.f, -3.f });
        DrawQuad(showRefraction ? RFRProgram : RFXProgram, ModelMatrix, ProjectionMatrix * ViewMatrix);
    }

    // Skybox
    {
        mat4 rotateOnlyViewMatrix = ViewMatrix;
        // Sets translation to 0
        rotateOnlyViewMatrix.c[3].x = 0;
        rotateOnlyViewMatrix.c[3].y = 0;
        rotateOnlyViewMatrix.c[3].z = 0;
        mat4 vp = ProjectionMatrix * rotateOnlyViewMatrix;
        glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
        glUseProgram(SBProgram);
        glUniformMatrix4fv(glGetUniformLocation(SBProgram, "uViewProj"), 1, GL_FALSE, vp.e);
        glBindVertexArray(cubeVAO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
    }
}

void demo_reflection::DisplayDebugUI()
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
    if (ImGui::TreeNodeEx("demo_reflection", ImGuiTreeNodeFlags_Framed))
    {
        ImGui::Checkbox("Show refraction", &showRefraction);

        if (ImGui::TreeNodeEx("Show last reflection cubemap"))
        {
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)reflectionTextures[2], ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));
            
            ImGui::Image((void*)(intptr_t)reflectionTextures[0], ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)reflectionTextures[4], ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)reflectionTextures[1], ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)reflectionTextures[5], ImVec2(96, 96));

            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)reflectionTextures[3], ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)Texture, ImVec2(96, 96));

            ImGui::TreePop();
        }

        ImGui::Checkbox("Show debug texture", &showDebugTextures);
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

void demo_reflection::CreateCubemapFromPos(v3 camPos, const platform_io& IO)
{
    // Camera angles
    camera tempCamera[6] = {
        {camPos, Math::HalfPi(), 0}, // Right
        {camPos,-Math::HalfPi(), 0}, // Left
        {camPos, 0, Math::HalfPi()}, // Top
        {camPos, 0,-Math::HalfPi()}, // Bottom
        {camPos, Math::Pi(), 0},     // Back
        {camPos, 0, 0},              // Front
    };

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(90.f), (float)REFLECTION_RES_WIDTH / (float)REFLECTION_RES_HEIGHT, 0.1f, REFLECTION_FAR_PLANE);
    mat4 ViewMatrix = CameraGetInverseMatrix(tempCamera[0]);

    glGenTextures(1, &reflectionCubemap);

    for (int i = 0; i < 6; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionFramebuffers[i]);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // we're not using the stencil buffer now
        glEnable(GL_DEPTH_TEST);
        Render(IO, false, &tempCamera[i], &ProjectionMatrix);
        
        // Set cubemap face
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, REFLECTION_RES_WIDTH, REFLECTION_RES_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    }

    // Texture filters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // Clamp UV Coords to [0;1], avoids white edges
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
