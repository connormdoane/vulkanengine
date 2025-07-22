#!/bin/bash
mkdir -p build/
mkdir -p build/shaders/
glslangValidator -V shaders/gradient.comp -o build/shaders/gradient.comp.spv
glslangValidator -V shaders/gradient_color.comp -o build/shaders/gradient-color.comp.spv
glslangValidator -V shaders/sky.comp -o build/shaders/sky.comp.spv
