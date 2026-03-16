#version 400 core

// ── 全屏三角形（无顶点缓冲，仅用 gl_VertexID）─────────────
// 用法：glBindVertexArray(emptyVAO); glDrawArrays(GL_TRIANGLES, 0, 3);

out vec2 vTexCoord;

void main() {
    // 覆盖整个 NDC 的超大三角形
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 texCoords[3] = vec2[3](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    vTexCoord   = texCoords[gl_VertexID];
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
