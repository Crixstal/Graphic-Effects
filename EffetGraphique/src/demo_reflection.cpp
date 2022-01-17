
#include <vector>
#include <iostream>

#include <imgui.h>

#include "opengl_helpers.h"
#include "maths.h"
#include "mesh.h"
#include "color.h"

#include "demo_reflection.h"

#include "pg.h"

#define REFLECTION_RES 512
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
uniform samplerCube uCubemap;

// Shader outputs
out vec4 oColor;

void main()
{
    vec3 viewVec = normalize(vPos - uCamPos);
    vec3 reflectVec = reflect(viewVec, normalize(vNormal));
    //reflectVec.z = -reflectVec.z;
    oColor = texture(uCubemap, reflectVec);
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
uniform samplerCube uCubemap;

// Shader outputs
out vec4 oColor;

void main()
{
    // Common refraction indexes
    //Air = 1.00, Water = 1.33, Ice = 1.309, Glass = 1.52, Diamond = 2.42
    
    float rfrRatio = 1.0 / 1.52;
    vec3 viewVec = normalize(vPos - uCamPos);
    vec3 reflectVec = refract(viewVec, normalize(vNormal), rfrRatio);
    //reflectVec.z = -reflectVec.z;
    oColor = texture(uCubemap, reflectVec);
})GLSL";
#pragma endregion
#pragma endregion

static void DrawQuad(GLuint Program, mat4 ModelViewProj)
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, ModelViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void DrawMirror(GLuint Program, mat4 Model)
{
    glUseProgram(Program);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModel"), 1, GL_FALSE, Model.e);
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
 
    // Reflection cubemap
    {
        glGenTextures(1, &reflectionCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        
        for (int i = 0; i < 6; i++)
            GL::UploadBlankCubemapTexture(REFLECTION_RES, i);

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        // Texture filters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        // Clamp UV Coords to [0;1], avoids white edges
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

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
    glDeleteTextures(1, &customTexture);
    glDeleteTextures(1, &skybox);
    glDeleteTextures(1, &reflectionCubemap);

    glDeleteBuffers(1, &VertexBuffer);
    glDeleteBuffers(1, &cubeVertexBuffer);
    glDeleteBuffers(1, &sphereVertexBuffer);

    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &sphereVAO);

    glDeleteProgram(Program);
    glDeleteProgram(SBProgram);
    glDeleteProgram(RFXProgram);
    glDeleteProgram(RFRProgram);
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

    // Spheres
    {
        glUseProgram(Program);
        mat4 ModelMatrix = Mat4::Translate({ 0.f, -1.f * sinf(IO.Time), -1.f });
        mat4 mvp = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, mvp.e);
        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
        ModelMatrix = Mat4::Translate({ 3.f, 0.f, 0.f });
        mvp = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(glGetUniformLocation(Program, "uModelViewProj"), 1, GL_FALSE, mvp.e);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);

    // Mirrors
    if (renderMirrorEffects)
    {
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

        // Bind ViewProj here to bind only once
        glUniformMatrix4fv(glGetUniformLocation(showRefraction ? RFRProgram : RFXProgram, "uViewProj"), 1, GL_FALSE, (ProjectionMatrix * ViewMatrix).e);
        
        glBindVertexArray(VAO); // Bind quad mesh
        v3 ObjectPosition = { 0.f, 0.f, -3.f };

        mat4 ModelMatrix = Mat4::Translate(ObjectPosition);
        
        CreateCubemapFromModelMat(ModelMatrix, IO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);

        DrawMirror(showRefraction ? RFRProgram : RFXProgram, ModelMatrix);

        ModelMatrix = Mat4::RotateY(Math::HalfPi()) * Mat4::Translate({ 0.f, 0.f, -3.f });
        CreateCubemapFromModelMat(ModelMatrix, IO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        DrawMirror(showRefraction ? RFRProgram : RFXProgram, ModelMatrix);

        ModelMatrix = Mat4::Translate({ 0.f, 0.f, 0.f });
        CreateCubemapFromModelMat(ModelMatrix, IO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        glUseProgram(showRefraction ? RFRProgram : RFXProgram);
        glUniformMatrix4fv(glGetUniformLocation(showRefraction ? RFRProgram : RFXProgram, "uModel"), 1, GL_FALSE, ModelMatrix.e);

        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
    }

    // Skybox
    {
        glDepthMask(GL_FALSE);

        mat4 rotateOnlyViewMatrix = ViewMatrix;
        // Sets translation to 0
        rotateOnlyViewMatrix.c[3].x = 0;
        rotateOnlyViewMatrix.c[3].y = 0;
        rotateOnlyViewMatrix.c[3].z = 0;
        mat4 vp = ProjectionMatrix * rotateOnlyViewMatrix;
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

    mat4 ProjectionMatrix = Mat4::Scale(-1.f) *  Mat4::Perspective(Math::ToRadians(90.f), 1.f, 0.1f, REFLECTION_FAR_PLANE);
    mat4 ViewMatrix = CameraGetInverseMatrix(tempCamera[0]);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, REFLECTION_RES, REFLECTION_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glViewport(0, 0, REFLECTION_RES, REFLECTION_RES);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "[ERROR] Framebuffer is not complete!" << std::endl;

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    for (int i = 0; i < 6; i++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, reflectionCubemap, 0);
     
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        glClearColor(0.5f, 0.1f, 0.1f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        Render(IO, false, &tempCamera[i], &ProjectionMatrix);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        // Set cubemap face
        // glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        // glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);
}

void demo_reflection::CreateCubemapFromModelMat(mat4 modelMat, const platform_io& IO)
{
    v3 camPos = {};
    camPos.x = modelMat.c[3].x;
    camPos.y = modelMat.c[3].y;
    camPos.z = modelMat.c[3].z;

    // Camera angles
    camera tempCamera[6] = {
        {camPos, Math::HalfPi(), 0}, // Right
        {camPos,-Math::HalfPi(), 0}, // Left
        {camPos, Math::Pi(), Math::HalfPi()}, // Top
        {camPos, Math::Pi(),-Math::HalfPi()}, // Bottom
        {camPos, Math::Pi(), 0},     // Back
        {camPos, 0, 0},              // Front
    };

    mat4 ProjectionMatrix = Mat4::Scale(-1.f) * Mat4::Perspective(Math::ToRadians(90.f), 1.f, 0.1f, REFLECTION_FAR_PLANE);
    mat4 ViewMatrix = CameraGetInverseMatrix(tempCamera[0]);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, REFLECTION_RES, REFLECTION_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glViewport(0, 0, REFLECTION_RES, REFLECTION_RES);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "[ERROR] Framebuffer is not complete!" << std::endl;

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    for (int i = 0; i < 6; i++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, reflectionCubemap, 0);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        glClearColor(0.5f, 0.1f, 0.1f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        Render(IO, false, &tempCamera[i], &ProjectionMatrix);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        // Set cubemap face
        // glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
        // glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);

    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
}

