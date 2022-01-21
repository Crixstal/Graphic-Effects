
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "opengl_helpers_wireframe.h"

#include "color.h"
#include "maths.h"
#include "mesh.h"

#include "demo_npr_toon.h"

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

// Varyings
out vec2 vUV;
out vec3 vPos;    // Vertex position in view-space
out vec3 vNormal; // Vertex normal in view-space

void main()
{
    vUV = aUV;
    vec4 pos4 = (uModel * vec4(aPosition, 1.0));
    vPos = pos4.xyz / pos4.w;
    vNormal = (uModelNormalMatrix * vec4(aNormal, 0.0)).xyz;
    gl_Position = uProjection * uView * pos4;
})GLSL";

#pragma endregion

#pragma region FRAGMENT SHADER

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;
in vec3 vPos;
in vec3 vNormal;

// Uniforms
uniform mat4 uProjection;
uniform vec3 uViewPosition;
uniform bool uIsOutline;
uniform bool uToonShading;
uniform bool uFiveTone;

uniform sampler2D uDiffuseTexture;
uniform sampler2D uEmissiveTexture;

// Uniform blocks
layout(std140) uniform uLightBlock
{
	light uLight;
};

// Shader outputs
out vec4 oColor;

light_shade_result get_lights_shading()
{
    light_shade_result lightResult = light_shade_result(vec3(0.0), vec3(0.0), vec3(0.0));

    light_shade_result light = light_shade(uLight, gDefaultMaterial.shininess, uViewPosition, vPos, normalize(vNormal));
    lightResult.ambient  += light.ambient;
    lightResult.diffuse  += light.diffuse;
    lightResult.specular += light.specular;

    return lightResult;
}

void main()
{
    float intensity = dot(normalize(uLight.position.xyz), normalize(vNormal));
    vec4 color1 = vec4(uLight.diffuse, 1.0);
    //vec4 color1 = texture(uDiffuseTexture, vUV);
    vec4 color2;
    
    if (uToonShading)
    {
        if (uFiveTone)
        {
            if (intensity > 0.95)       color2 = vec4(1.0, 1.0, 1.0, 1.0);
            else if (intensity > 0.75)  color2 = vec4(0.8, 0.8, 0.8, 1.0);
            else if (intensity > 0.50)  color2 = vec4(0.6, 0.6, 0.6, 1.0);
            else if (intensity > 0.25)  color2 = vec4(0.4, 0.4, 0.4, 1.0);
            else                        color2 = vec4(0.2, 0.2, 0.2, 1.0);
        }
        else
        {
            if (intensity > 0.95)       color2 = vec4(1.0, 1.0, 1.0, 1.0);
            else if (intensity > 0.50)  color2 = vec4(0.6, 0.6, 0.6, 1.0);
            else if (intensity > 0.25)  color2 = vec4(0.4, 0.4, 0.4, 1.0);
            else                        color2 = vec4(0.2, 0.2, 0.2, 1.0);
        }
       
        oColor = color1 * color2;
    }
    else
    {
        // Compute phong shading
        light_shade_result lightResult = get_lights_shading();
        
        vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse; // * texture(uDiffuseTexture, vUV).rgb;
        vec3 ambientColor  = gDefaultMaterial.ambient * lightResult.ambient;
        vec3 specularColor = gDefaultMaterial.specular * lightResult.specular;
        vec3 emissiveColor = gDefaultMaterial.emission; // + texture(uEmissiveTexture, vUV).rgb;
        
        // Apply light color
        oColor = vec4((ambientColor + diffuseColor + specularColor + emissiveColor), 1.0);
    }

    if (uIsOutline)
        oColor = vec4(0.0, 0.0, 0.0, 1.0);

})GLSL";
#pragma endregion

demo_npr_toon::demo_npr_toon(GL::cache& GLCache, GL::debug& GLDebug)
    : GLDebug(GLDebug), NPRScene(GLCache)
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
        glGenVertexArrays(1, &VAO_NPR);
        glBindVertexArray(VAO_NPR);

        glBindBuffer(GL_ARRAY_BUFFER, NPRScene.MeshBuffer);

        vertex_descriptor& Desc = NPRScene.MeshDesc;
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
        glUniform1i(glGetUniformLocation(Program, "uDiffuseTexture"), 0);
        glUniform1i(glGetUniformLocation(Program, "uEmissiveTexture"), 1);
        glUniformBlockBinding(Program, glGetUniformBlockIndex(Program, "uLightBlock"), LIGHT_BLOCK_BINDING_POINT);
    }
}

demo_npr_toon::~demo_npr_toon()
{
    // Cleanup GL
    glDeleteVertexArrays(1, &VAO_NPR);
    glDeleteProgram(Program);
}

void demo_npr_toon::Update(const platform_io& IO)
{
    const float AspectRatio = (float)IO.WindowWidth / (float)IO.WindowHeight;
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);

    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Clear screen
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), AspectRatio, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    mat4 ModelMatrix = Mat4::Scale({ 1.0f, 1.0f, 1.0f });

    // Render Model
    this->RenderNPRModel(ProjectionMatrix, ViewMatrix, ModelMatrix);

    // Display debug UI
    this->DisplayDebugUI();
}

void demo_npr_toon::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_npr_toon", ImGuiTreeNodeFlags_Framed))
    {
        ImGui::Checkbox("ToonShading", &ToonShading);

        if (ToonShading)
        {
            ImGui::Checkbox("Outline", &Outline);
            ImGui::Checkbox("FiveTone", &FiveTone);
        }
        else
        {
            Outline = false;
            FiveTone = false;
        }

        // Debug display
        if (ImGui::TreeNodeEx("Camera"))
        {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", Camera.Position.x, Camera.Position.y, Camera.Position.z);
            ImGui::Text("Pitch: %.2f", Math::ToDegrees(Camera.Pitch));
            ImGui::Text("Yaw: %.2f", Math::ToDegrees(Camera.Yaw));
            ImGui::TreePop();
        }
        NPRScene.InspectLights();

        ImGui::TreePop();
    }
}

void demo_npr_toon::RenderNPRModel(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Use shader and configure its uniforms
    glUseProgram(Program);

    // Set uniforms
    mat4 NormalMatrix = Mat4::Transpose(Mat4::Inverse(ModelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(Program, "uProjection"), 1, GL_FALSE, ProjectionMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModel"), 1, GL_FALSE, ModelMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uView"), 1, GL_FALSE, ViewMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModelNormalMatrix"), 1, GL_FALSE, NormalMatrix.e);
    glUniform3fv(glGetUniformLocation(Program, "uViewPosition"), 1, Camera.Position.e);
    glUniform1i(glGetUniformLocation(Program, "uToonShading"), ToonShading);
    glUniform1i(glGetUniformLocation(Program, "uFiveTone"), FiveTone);

    // Bind uniform buffer and textures
    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHT_BLOCK_BINDING_POINT, NPRScene.LightsUniformBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, NPRScene.DiffuseTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, NPRScene.EmissiveTexture);
    glActiveTexture(GL_TEXTURE0); // Reset active texture just in case
   
   if (Outline)
   {
        //DRAW MESH A FIRST TIME
        glBindVertexArray(VAO_NPR);
        glDrawArrays(GL_TRIANGLES, 0, NPRScene.MeshVertexCount);

        glUniform1i(glGetUniformLocation(Program, "uIsOutline"), 1);

        glCullFace(GL_FRONT);
        glDepthFunc(GL_LEQUAL);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glLineWidth(4);

        //DRAW MESH A SECOND TIME
        glBindVertexArray(VAO_NPR);
        glDrawArrays(GL_TRIANGLES, 0, NPRScene.MeshVertexCount);

        glUniform1i(glGetUniformLocation(Program, "uIsOutline"), 0);

        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }
   else
   {
       glBindVertexArray(VAO_NPR);
       glDrawArrays(GL_TRIANGLES, 0, NPRScene.MeshVertexCount);
   }
}
