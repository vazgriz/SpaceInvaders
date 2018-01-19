#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv; 

layout(set = 0, binding = 0) uniform sampler2D texture1;

layout(location = 0) out vec4 fragColor;

void main(){
    fragColor = texture(texture1, uv);
}
