#version 450
layout(local_size_x = 16, local_size_y = 16) in;

// Binding 0: sampled input (the copied game image)
layout(binding = 0) uniform sampler2D uSrc;

// Binding 1: storage output (desaturated)
layout(binding = 1, rgba8) uniform writeonly image2D uDst;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    // Get destination size
    ivec2 size = imageSize(uDst);
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    // Normalized UV
    vec2 uv = (vec2(coord) + 0.5) / vec2(size);

    vec4 color = texture(uSrc, uv);
    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    vec4 gray = vec4(luma, luma, luma, color.a);

    imageStore(uDst, coord, gray);
}
