# Beginnings of a Vulkan rendering engine

Created with the help of vkguide.dev

# Dependencies

The Makefile is designed for compilation through GCC.

The majority of dependencies are included directly in the source code, the only ones that must be installed globally are the Vulkan SDK and SDL2

# Usage

To compile and run the engine, first run the build-shaders.sh bash script to compile shaders

```
chmod +x build-shaders.sh
./build-shaders.sh
```

With the shaders built, compile (and run) the engine with make

```
make run
```

# External Libraries

- VMA (Vulkan Memory Allocator): Header only library for simplified memory allocation
- Vk-bootstrap: Utility functions that simplify the boilerplate code for getting Vulkan up and running
- SDL: Used for window creation and user input
- fmt: String formatting library
- vk\_implementations: Factory functions for Vulkan info structs, from vkguide
- Dear ImGui 1.90.6: Adds a useful GUI outside of my rendering for debug. An outdated version is being used as CreateFontsTexture() was removed in a recent update, and I'm holding off on researching the new implementation
