#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "opengl_helpers_wireframe.h"

#include "color.h"
#include "maths.h"
#include "mesh.h"

#include "demo_shader.h"

const int LIGHT_BLOCK_BINDING_POINT = 0;

#pragma region VERTEX SHADER

static const char* gVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;

// Uniforms
uniform mat4 uProjection;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uModelNormalMatrix;

uniform vec3 uViewPosition;

uniform float ka;
uniform float kd;
uniform float ks;

// Uniform blocks
layout(std140) uniform uLightBlock
{
	light uLight;
};

// Varyings
out vec2 vUV;
out vec3 vPos;    // Vertex position in view-space
out vec3 vNormal; // Vertex normal in view-space

flat out vec2 vUVFlat;
flat out vec3 vPosFlat;
flat out vec3 vNormalFlat;

out vec4 vGouraudColor;

light_shade_result get_lights_shading(vec3 Pos, vec3 Normal)
{
    light_shade_result lightResult = light_shade_result(vec3(0.0), vec3(0.0), vec3(0.0));
    
    light_shade_result light = light_shade(uLight, gDefaultMaterial.shininess, uViewPosition, Pos, normalize(Normal));
    lightResult.ambient  += light.ambient;
    lightResult.diffuse  += light.diffuse;
    lightResult.specular += light.specular;

    return lightResult;
}

void main()
{
    vUV = aUV;
    vec4 pos4 = (uModel * vec4(aPosition, 1.0));
    vPos = pos4.xyz / pos4.w;
    vNormal = (uModelNormalMatrix * vec4(aNormal, 0.0)).xyz;
    gl_Position = uProjection * uView * pos4;

    vUVFlat = vUV;
    vPosFlat = vPos;
    vNormalFlat = vNormal;

    light_shade_result lightResult = get_lights_shading(vPos, vNormal);

    vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse;
    vec3 ambientColor  = gDefaultMaterial.ambient * lightResult.ambient;
    vec3 specularColor = gDefaultMaterial.specular * lightResult.specular;
    vec3 emissiveColor = gDefaultMaterial.emission;

    vGouraudColor = vec4((ka * ambientColor + kd * diffuseColor + ks * specularColor + emissiveColor), 1.0);

})GLSL";

#pragma endregion

#pragma region FRAGMENT SHADER

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;
in vec3 vPos;
in vec3 vNormal;

flat in vec2 vUVFlat;
flat in vec3 vPosFlat;
flat in vec3 vNormalFlat;

in vec4 vGouraudColor;

// Uniforms
uniform mat4 uProjection;
uniform vec3 uViewPosition;
uniform bool uFlatShading;
uniform bool uGouraudShading;
uniform bool uPhongShading;
uniform bool uBlinnPhongShading;
uniform float uShininess;

// Uniform blocks
layout(std140) uniform uLightBlock
{
	light uLight;
};

// Shader outputs
out vec4 oColor;

light_shade_result get_lights_shading(vec3 Pos, vec3 Normal)
{
    light_shade_result lightResult = light_shade_result(vec3(0.0), vec3(0.0), vec3(0.0));
    
    light_shade_result light = light_shade(uLight, gDefaultMaterial.shininess, uViewPosition, Pos, normalize(Normal));
    lightResult.ambient  += light.ambient;
    lightResult.diffuse  += light.diffuse;
    lightResult.specular += light.specular;

    return lightResult;
}

void main()
{
    light_shade_result lightResult = light_shade_result(vec3(0.0), vec3(0.0), vec3(0.0));

    if (uFlatShading)
        lightResult = get_lights_shading(vPosFlat, vNormalFlat);

    if (uPhongShading || uBlinnPhongShading)
        lightResult = get_lights_shading(vPos, vNormal);
    
    vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse;
    vec3 ambientColor  = gDefaultMaterial.ambient * lightResult.ambient;
    vec3 specularColor = gDefaultMaterial.specular * lightResult.specular;
    vec3 emissiveColor = gDefaultMaterial.emission;
    
    oColor = vec4((ambientColor + diffuseColor + specularColor + emissiveColor), 1.0);

    if (uGouraudShading)
        oColor = vGouraudColor;

    if (uBlinnPhongShading)
    {
        vec3 lightDir = normalize(uLight.position.xyz);
        vec3 viewDir = normalize(uViewPosition - vPos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float specular = pow(max(dot(normalize(vNormal), halfDir), 0.0), uShininess);

        oColor = vec4((ambientColor + diffuseColor + specularColor * specular + emissiveColor), 1.0);
    }
})GLSL";

#pragma endregion

demo_shader::demo_shader(GL::cache& GLCache, GL::debug& GLDebug)
    : GLDebug(GLDebug), ShaderScene(GLCache)
{
    // Create shader
    {
        // Assemble fragment shader strings (defines + code)
        char FragmentShaderConfig[] = "";
        const char* FragmentShaderStrs[2] = { FragmentShaderConfig, gFragmentShaderStr };

        this->Program = GL::CreateProgramEx(1, &gVertexShaderStr, 2, FragmentShaderStrs, true);
    }

    // Create a vertex array and bind attribs onto the vertex buffer
    {
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, ShaderScene.MeshBuffer);

        vertex_descriptor& Desc = ShaderScene.MeshDesc;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.PositionOffset);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.UVOffset);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.NormalOffset);
    }

    // Set uniforms that won't change
    {
        glUseProgram(Program);
        glUniformBlockBinding(Program, glGetUniformBlockIndex(Program, "uLightBlock"), LIGHT_BLOCK_BINDING_POINT);
    }
}

demo_shader::~demo_shader()
{
    // Cleanup GL
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(Program);
}

void demo_shader::Update(const platform_io& IO)
{
    const float AspectRatio = (float)IO.WindowWidth / (float)IO.WindowHeight;
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);

    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Clear screen
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), AspectRatio, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    mat4 ModelMatrix = Mat4::Scale({ 15.0f, 15.0f, 15.0f });

    // Render Model
    this->Render(ProjectionMatrix, ViewMatrix, ModelMatrix);

    // Display debug UI
    this->DisplayDebugUI();
}

void demo_shader::DisplayDebugUI()
{
    const char* warning = "You can't use multiple shading";

    if (ImGui::TreeNodeEx("demo_shader", ImGuiTreeNodeFlags_Framed))
    {
        ImGui::Text("%s", warning);

        ImGui::Checkbox("Flat Shading", &FlatShading);

        ImGui::Checkbox("Gouraud Shading", &GouraudShading);
        if (GouraudShading)
        {
            ImGui::SliderFloat("ka", &ka, 0.f, 1.f);
            ImGui::SliderFloat("kd", &kd, 0.f, 1.f);
            ImGui::SliderFloat("ks", &ks, 0.f, 1.f);
        }

        ImGui::Checkbox("Phong Shading", &PhongShading);

        ImGui::Checkbox("Blinn-Phong Shading", &BlinnPhongShading);
        if (BlinnPhongShading)
            ImGui::SliderFloat("Shininess", &shininess, 0.f, 100.f);

        if (ImGui::TreeNodeEx("Camera"))
        {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", Camera.Position.x, Camera.Position.y, Camera.Position.z);
            ImGui::Text("Pitch: %.2f", Math::ToDegrees(Camera.Pitch));
            ImGui::Text("Yaw: %.2f", Math::ToDegrees(Camera.Yaw));
            ImGui::TreePop();
        }
        ShaderScene.InspectLights();

        ImGui::TreePop();
    }
}

void demo_shader::Render(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix)
{
    glEnable(GL_DEPTH_TEST);

    // Use shader and configure its uniforms
    glUseProgram(Program);

    // Set uniforms
    mat4 NormalMatrix = Mat4::Transpose(Mat4::Inverse(ModelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(Program, "uProjection"), 1, GL_FALSE, ProjectionMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModel"), 1, GL_FALSE, ModelMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uView"), 1, GL_FALSE, ViewMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelNormalMatrix"), 1, GL_FALSE, NormalMatrix.e);
    glUniform3fv(glGetUniformLocation(Program, "uViewPosition"), 1, Camera.Position.e);

    glUniform1i(glGetUniformLocation(Program, "uFlatShading"), FlatShading);
    glUniform1i(glGetUniformLocation(Program, "uGouraudShading"), GouraudShading);
    glUniform1i(glGetUniformLocation(Program, "uPhongShading"), PhongShading);
    glUniform1i(glGetUniformLocation(Program, "uBlinnPhongShading"), BlinnPhongShading);

    glUniform1f(glGetUniformLocation(Program, "ka"), ka);
    glUniform1f(glGetUniformLocation(Program, "kd"), kd);
    glUniform1f(glGetUniformLocation(Program, "ks"), ks);
    glUniform1f(glGetUniformLocation(Program, "uShininess"), shininess);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, ShaderScene.MeshVertexCount);
}