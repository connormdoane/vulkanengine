#!/bin/bash

# Create build directories if they don't exist
mkdir -p build/shaders/

# Loop through all .comp, .frag, and .vert files in the shaders directory
for shader in shaders/*.{comp,frag,vert}; do
    # Skip if no matching files found
    [ -e "$shader" ] || continue

    # Extract the filename (e.g., gradient.comp)
    filename=$(basename "$shader")

    # Output path (e.g., build/shaders/gradient.comp.spv)
    output="build/shaders/$filename.spv"

    # Compile the shader
    echo "Compiling $shader -> $output"
    glslangValidator -V "$shader" -o "$output"
done
