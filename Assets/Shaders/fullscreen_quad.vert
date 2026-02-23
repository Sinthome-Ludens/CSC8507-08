#version 400 core

// Fullscreen triangle — no VAO needed, just glDrawArrays(GL_TRIANGLES, 0, 3)
// Covers the entire screen with a single triangle (more efficient than a quad)

out vec2 vTexCoord;

void main() {
    // gl_VertexID: 0, 1, 2
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vTexCoord = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
