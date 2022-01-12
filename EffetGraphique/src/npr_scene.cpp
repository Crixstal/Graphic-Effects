
#include <imgui.h>

#include "platform.h"
#include "color.h"
#include "npr_scene.h"

npr_scene::npr_scene(GL::cache& GLCache)
{
    // Init lights
    {
        this->LightCount = 1;
        this->Lights.resize(this->LightCount);

        // (Default light, standard values)
        GL::light DefaultLight = {};
        DefaultLight.Enabled = true;
        DefaultLight.Position = { 0.0f, 0.0f, 0.0f, 1.f };
        DefaultLight.Ambient = { 0.2f, 0.2f, 0.2f };
        DefaultLight.Diffuse = { 1.0f, 1.0f, 1.0f };
        DefaultLight.Specular = { 0.0f, 0.0f, 0.0f };
        DefaultLight.Attenuation = { 1.0f, 0.0f, 0.0f };

        // Sun light
        this->Lights[0] = DefaultLight;
        this->Lights[0].Position = { 1.f, 3.f, 1.f, 0.f }; // Directional light
        this->Lights[0].Diffuse = Color::RGB(0x374D58);
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
        glBufferData(GL_UNIFORM_BUFFER, LightCount * sizeof(GL::light), Lights.data(), GL_DYNAMIC_DRAW);
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
        + ImGui::SliderFloat4("Position", Light->Position.e, -10.f, 10.f);

    return Result;
}

void npr_scene::InspectLights()
{
    if (ImGui::TreeNode(&Lights[0], "Light"))
    {
        GL::light& Light = Lights[0];
        if (EditLight(&Light))
            glBufferSubData(GL_UNIFORM_BUFFER, 0 * sizeof(GL::light), sizeof(GL::light), &Light);
    
        ImGui::TreePop();
    }
}
