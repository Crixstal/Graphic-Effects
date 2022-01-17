
#include <imgui.h>

#include "platform.h"
#include "color.h"
#include "npr_scene.h"

npr_scene::npr_scene(GL::cache& GLCache)
{
    // Init light
    {
        this->Light.Enabled = true;
        this->Light.Position = { 1.f, 3.f, 1.f, 0.f };
        this->Light.Ambient = { 0.2f, 0.2f, 0.2f };
        //this->Light.Diffuse = { 0.7f, 0.7f, 0.2f };
        this->Light.Diffuse = { 1.0f, 1.0f, 01.0f };
        this->Light.Specular = { 0.0f, 0.0f, 0.0f };
        this->Light.Attenuation = { 1.0f, 0.0f, 0.0f };
    }

    // Create mesh
    {
        // Use vbo from GLCache
        MeshBuffer = GLCache.LoadObj("media/T-Rex/T-Rex.obj", 1.f, &this->MeshVertexCount);

        MeshDesc.Stride = sizeof(vertex_full);
        MeshDesc.HasNormal = true;
        MeshDesc.HasUV = true;
        MeshDesc.PositionOffset = OFFSETOF(vertex_full, Position);
        MeshDesc.UVOffset = OFFSETOF(vertex_full, UV);
        MeshDesc.NormalOffset = OFFSETOF(vertex_full, Normal);
    }

    // Gen texture
    //{
    //    DiffuseTexture = GLCache.LoadTexture("media/fantasy_game_inn_diffuse.png", IMG_FLIP | IMG_GEN_MIPMAPS);
    //    EmissiveTexture = GLCache.LoadTexture("media/fantasy_game_inn_emissive.png", IMG_FLIP | IMG_GEN_MIPMAPS);
    //}

    // Gen light uniform buffer
    {
        glGenBuffers(1, &LightsUniformBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, LightsUniformBuffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GL::light), &Light, GL_DYNAMIC_DRAW);
    }
}

npr_scene::~npr_scene()
{
    glDeleteBuffers(1, &LightsUniformBuffer);
    //glDeleteTextures(1, &Texture);   // From cache
    //glDeleteBuffers(1, &MeshBuffer); // From cache
}

static bool EditLight(GL::light* Light)
{
    bool Result =
        ImGui::Checkbox("Enabled", (bool*)&Light->Enabled)
        + ImGui::SliderFloat4("Position", Light->Position.e, -4.f, 4.f);
        + ImGui::ColorEdit3("Ambient", Light->Ambient.e)
        + ImGui::ColorEdit3("Diffuse", Light->Diffuse.e)
        + ImGui::ColorEdit3("Specular", Light->Specular.e);

    return Result;
}

void npr_scene::InspectLights()
{
    const char* warning = "Only position works if GoochShading true";

    if (ImGui::TreeNode(&Light, "Light"))
    {
        ImGui::Text("%s", warning);

        if (EditLight(&Light))
            glBufferSubData(GL_UNIFORM_BUFFER, 0 * sizeof(GL::light), sizeof(GL::light), &Light);
    
        ImGui::TreePop();
    }
}
