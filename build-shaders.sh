#!/bin/bash
mkdir -p build/
mkdir -p build/shaders/
glslangValidator -V shaders/gradient.comp -o shaders/gradient.comp.spv
glslangValidator -V shaders/gradient_color.comp -o shaders/gradient-color.comp.spv
glslangValidator -V shaders/sky.comp -o shaders/sky.comp.spv
