#!/bin/bash

# Create output directory
mkdir -p build/shaders/

# Loop through each shader file with the desired extensions
for shader in shaders/*.{comp,frag,vert}; do
    # Skip if no match
    [ -e "$shader" ] || continue

    # Get the base filename (e.g., foo.vert)
    filename=$(basename "$shader")

    # Output filename: foo.vert.spv
    output="build/shaders/$filename.spv"

    # Check if the output doesn't exist or is older than the source
    if [ ! -f "$output" ] || [ "$shader" -nt "$output" ]; then
        echo "Compiling $shader -> $output"
        glslangValidator -V "$shader" -o "$output"
    fi
done

