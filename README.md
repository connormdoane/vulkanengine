# A Vulkan rendering engine

![A screenshot of the application, showing a shader gradient, a lowpoly model of a monkey head made of multicolored triangles, and a Dear ImGui debug window displaying shader settings](screenshot2.png?raw=true "Screenshot2")

![A screenshot of the application, showing a space station GLTF Scene, and a Dear ImGui debug window displaying shader settings](screenshot4.png?raw=true "Screenshot4")

# Dependencies

The Makefile is designed for compilation through GCC.

The majority of dependencies are included directly in the source code, the only ones that must be installed globally are the Vulkan SDK, SDL2, and fmt

Currently the engine has only been tested on Arch Linux

# Demo

```
make BUILD=release run
```

# External Libraries

- VMA (Vulkan Memory Allocator): Header only library for simplified memory allocation
- Vk-bootstrap: Utility functions that simplify the boilerplate code for getting Vulkan up and running
- SDL: Used for window creation and user input
- fmt: String formatting library
- vk\_implementations: Factory functions for Vulkan info structs, from vkguide
- Dear ImGui 1.90.6: Adds a useful GUI outside of my rendering for debug. An outdated version is being used as CreateFontsTexture() was removed in a recent update, and I'm holding off on researching the new implementation
