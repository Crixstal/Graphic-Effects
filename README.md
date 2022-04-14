# **Graphic effects techniques**

## **Summary**
- [Informations](#Informations)
- [Implemented features](#Implemented-features)
- [References](#References)

<br>

## **Informations**

The program runs in x64, Debug and Release.  
Concerning demo_shader (13/13), you must modify the light position or disable/enable it to apply the correct colors.
Same goes for demo_npr_toon (10/13).

You can use the fantasy game inn with the demo_npr_toon/gooch, for this you have to :
- uncomment the second MeshBuffer in npr_gooch/toon_scene.cpp (line 23)
- uncomment color1 in the fragment shader in demo_npr_toon.cpp (line 90)
- uncomment texture parts in diffuseColor and emissiveColor in demo_npr_toon/gooch.cpp
in the fragShader
``` c++       
vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse; // * texture(uDiffuseTexture, vUV).rgb;
```
```c++
vec3 emissiveColor = gDefaultMaterial.emission; // + texture(uEmissiveTexture, vUV).rgb;
```

<br>

## **Implemented features**

Feature                            | Files (.cpp / .h)
-------                            | ------
Skybox, reflection, refraction     | demo_skybox <br> demo_reflection
NPR                                | demo_npr_gooch<br>demo_npr_toon <br>                                  npr_gooch_scene<br>npr_toon_scene
Gamma correction                   | demo_gamma
Instancing                         | demo_instancing
Classic shading modes              | demo_shading <br> shader_scene

<br>

## **References**

Skybox
---

- [cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps)

NPR (Non Photorealistic Rendering)
---
- [Gooch Shading](https://rendermeapangolin.wordpress.com/2015/05/07/gooch-shading/)
- [Toon Shading](http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/toon-shading/)
- [Toon Shading](https://stackoverflow.com/questions/5795829/using-opengl-toon-shader-in-glsl)
- [Toon Shading](https://github.com/aglobus/toon-shading)

Instancing
---
- [Instancing](https://learnopengl.com/Advanced-OpenGL/Instancing)

Gamma correction
---
- [Gamma Correction](https://learnopengl.com/Advanced-Lighting/Gamma-Correction)

Classic shading modes
---
- [Flat shading](https://www.mauriciopoppe.com/notes/computer-graphics/surface-shading/flat-shading/)

- [Basic lighting](https://learnopengl.com/Lighting/Basic-Lighting)

- [WebGL - Gouraud / Phong shading](http://www.cs.toronto.edu/~jacobson/phong-demo/)

Other
---
- [How to use RenderDoc](https://www.youtube.com/watch?v=ngz4NHiigIw)

<br>

[Top of the page](#Summary)
