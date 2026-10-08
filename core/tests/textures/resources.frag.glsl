#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(set=2, binding=0) uniform sampler2D firstTexture;
layout(set=2, binding=1) uniform sampler2D secondTexture;
layout(set=2, binding=2) uniform texture2D storageTexture;
layout(std430, set=2, binding=3) readonly buffer Values { vec4 offset; } values;
layout(std140, set=3, binding=0) uniform Uniforms { vec4 gain; } uniforms;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    color = (texture(firstTexture, uv) + texture(secondTexture, uv)
           + texelFetch(storageTexture, ivec2(0), 0) + values.offset) * uniforms.gain;
}
