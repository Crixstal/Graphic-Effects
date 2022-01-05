
#include <vector>

#include "opengl_helpers.h"

// NPR scene data (mapped on GPU)
class npr_scene
{
public:
    npr_scene(GL::cache& GLCache);
    ~npr_scene();
    
    // Mesh
    GLuint MeshBuffer = 0;
    int MeshVertexCount = 0;
    vertex_descriptor MeshDesc;

    // Lights buffer
    GLuint LightsUniformBuffer = 0;
    int LightCount = 8;

    // Textures
    GLuint DiffuseTexture = 0;
    GLuint EmissiveTexture = 0;

    // ImGui debug function to edit lights
    void InspectLights();

private:
    // Lights data
    std::vector<GL::light> Lights;
};