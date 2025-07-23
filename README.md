# Beginnings of a Vulkan rendering engine

Created with the help of vkguide.dev

# External Libraries

- VMA (Vulkan Memory Allocator): Header only library for simplified memory allocation
- Vk-bootstrap: Utility functions that simplify the boilerplate code for getting Vulkan up and running
- SDL: Used for window creation and user input
- fmt: String formatting library
- vk\_implementations: Factory functions for Vulkan info structs, from vkguide
- Dear ImGui 1.90.6: Adds a useful GUI outside of my rendering for debug. An outdated version is being used as CreateFontTexture() was removed in a recent update, and I'm holding off on researching the new implementation
