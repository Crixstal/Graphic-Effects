
#include <imgui.h>

#include "platform.h"
#include "color.h"
#include "npr_toon_scene.h"

npr_toon_scene::npr_toon_scene(GL::cache& GLCache)
{
    // Init light
    {
        this->Light.Enabled = true;
        this->Light.Position = { 1.f, 3.f, 1.f, 0.f };
        this->Light.Ambient = { 0.2f, 0.2f, 0.2f };
        this->Light.Diffuse = { 1.0f, 1.0f, 1.0f };
        this->Light.Specular = { 0.0f, 0.0f, 0.0f };
        this->Light.Attenuation = { 1.0f, 0.0f, 0.0f };
    }

    // Create mesh
    {
        // Use vbo from GLCache
        MeshBuffer = GLCache.LoadObj("media/teapot.obj", 1.f, &this->MeshVertexCount);

        MeshDesc.Stride = sizeof(vertex_full);
        MeshDesc.HasNormal = true;
        MeshDesc.HasUV = true;
        MeshDesc.PositionOffset = OFFSETOF(vertex_full, Position);
        MeshDesc.UVOffset = OFFSETOF(vertex_full, UV);
        MeshDesc.NormalOffset = OFFSETOF(vertex_full, Normal);
    }

    // Gen light uniform buffer
    {
        glGenBuffers(1, &LightsUniformBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, LightsUniformBuffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GL::light), &Light, GL_DYNAMIC_DRAW);
    }
}

npr_toon_scene::~npr_toon_scene()
{
    glDeleteBuffers(1, &LightsUniformBuffer);
}

static bool EditLight(GL::light* Light)
{
    bool Result =
        ImGui::Checkbox("Enabled", (bool*)&Light->Enabled)
        + ImGui::SliderFloat4("Position", Light->Position.e, -10.f, 10.f);
        + ImGui::ColorEdit3("Ambient", Light->Ambient.e)
        + ImGui::ColorEdit3("Diffuse", Light->Diffuse.e)
        + ImGui::ColorEdit3("Specular", Light->Specular.e);

    return Result;
}

void npr_toon_scene::InspectLights()
{
    const char* warning = "Only position and ambient works if ToonShading true";

    if (ImGui::TreeNode(&Light, "Light"))
    {
        ImGui::Text("%s", warning);

        if (EditLight(&Light))
            glBufferSubData(GL_UNIFORM_BUFFER, 0 * sizeof(GL::light), sizeof(GL::light), &Light);

        ImGui::TreePop();
    }
}
