
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "opengl_helpers_wireframe.h"

#include "color.h"
#include "maths.h"
#include "mesh.h"

#include "demo_npr.h"

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

// Uniform blocks
layout(std140) uniform uLightBlock
{
	light uLight[LIGHT_COUNT];
};

// Shader outputs
out vec4 oColor;

vec4 gooch_shading(vec4 m_color, //color of the mesh
                   float m_shine, //shininess of the surface
                   vec3 l_direction, //light direction
                   vec3 v_normal, //normal
                   vec3 c_direction) //camera direction
{
    //diffuse
    float kd = 1;
    float a = 0.2;
    float b = 0.6;

    float NL = dot(normalize(v_normal), normalize(l_direction));
    
    float it = ((1 + NL) / 2);
    vec3 color = (1-it) * (vec3(0, 0, 0.4) + a*m_color.xyz) + it * (vec3(0.4, 0.4, 0) + b*m_color.xyz);
    
    //Highlights
    vec3 R = reflect( -normalize(l_direction), normalize(v_normal) );
    float ER = clamp( dot( normalize(c_direction),  normalize(R)), 0, 1);
    
    vec4 spec = vec4(1) * pow(ER, m_shine);

    return vec4(color+spec.xyz, m_color.a);
}

void main()
{
    if (uIsOutline)
        oColor = vec4(0.0, 0.0, 0.0, 1.0);

    else
        oColor = gooch_shading(vec4(gDefaultMaterial.ambient, 1.0), gDefaultMaterial.shininess, uLight[0].position.xyz, vNormal, uViewPosition);
})GLSL";
#pragma endregion

demo_npr::demo_npr(GL::cache& GLCache, GL::debug& GLDebug)
    : GLDebug(GLDebug), NPRScene(GLCache)
{
    // Create shader
    {
        // Assemble fragment shader strings (defines + code)
        char FragmentShaderConfig[] = "#define LIGHT_COUNT %d\n";
        snprintf(FragmentShaderConfig, ARRAY_SIZE(FragmentShaderConfig), "#define LIGHT_COUNT %d\n", NPRScene.LightCount);
        const char* FragmentShaderStrs[2] = {
            FragmentShaderConfig,
            gFragmentShaderStr,
        };

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
        //glUniform1i(glGetUniformLocation(Program, "uDiffuseTexture"), 0);
        //glUniform1i(glGetUniformLocation(Program, "uEmissiveTexture"), 1);
        glUniformBlockBinding(Program, glGetUniformBlockIndex(Program, "uLightBlock"), LIGHT_BLOCK_BINDING_POINT);
    }
}

demo_npr::~demo_npr()
{
    // Cleanup GL
    glDeleteVertexArrays(1, &VAO_NPR);
    glDeleteProgram(Program);
}

void demo_npr::Update(const platform_io& IO)
{
    const float AspectRatio = (float)IO.WindowWidth / (float)IO.WindowHeight;
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);

    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Clear screen
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), AspectRatio, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    mat4 ModelMatrix = Mat4::Scale({ 0.01f, 0.01f, 0.01f });

    // Render Model
    this->RenderNPRModel(ProjectionMatrix, ViewMatrix, ModelMatrix);
    
    // Display debug UI
    this->DisplayDebugUI();
}

void demo_npr::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_npr", ImGuiTreeNodeFlags_Framed))
    {
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

void demo_npr::RenderNPRModel(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix)
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

    // Bind uniform buffer and textures
    //glBindBufferBase(GL_UNIFORM_BUFFER, LIGHT_BLOCK_BINDING_POINT, NPRScene.LightsUniformBuffer);
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, NPRScene.DiffuseTexture);
    //glActiveTexture(GL_TEXTURE1);
    //glBindTexture(GL_TEXTURE_2D, NPRScene.EmissiveTexture);
    //glActiveTexture(GL_TEXTURE0); // Reset active texture just in case

    //DRAW MESH A FIRST TIME
    glBindVertexArray(VAO_NPR);
    glDrawArrays(GL_TRIANGLES, 0, NPRScene.MeshVertexCount);

    glUniform1i(glGetUniformLocation(Program, "uIsOutline"), 1);

    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    glLineWidth(6);

    //DRAW MESH A SECOND TIME
    glBindVertexArray(VAO_NPR);
    glDrawArrays(GL_TRIANGLES, 0, NPRScene.MeshVertexCount);

    glUniform1i(glGetUniformLocation(Program, "uIsOutline"), 0);

    glCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
