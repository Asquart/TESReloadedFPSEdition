#version 450

// Workgroup size – must match dispatch from C++
layout(local_size_x = 16, local_size_y = 16) in;

// Binding 0: storage image we will write into
layout(binding = 0, rgba8) uniform writeonly image2D outImage;

// Simple push constant block for color (0..1)
layout(push_constant) uniform SolidColorPush {
    vec4 color;
} solid;

// Main entry point
void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    // Just write the same color everywhere
    imageStore(outImage, coord, solid.color);
}
