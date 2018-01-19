#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tex;

layout(push_constant) uniform Model {
    mat4 matrix;
} model;

layout(location = 0) out vec2 uv; 

void main(){
    gl_Position = model.matrix * vec4(pos, 0.0, 1.0);
    uv = tex;
}
