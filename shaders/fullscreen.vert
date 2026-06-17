#version 300 es
// Fullscreen-triangle vertex shader.
//
// Emits a single oversized triangle that covers the whole viewport with no
// vertex buffer at all: the three positions are derived from gl_VertexID. The
// host draws this with glDrawArrays(GL_TRIANGLES, 0, 3) and an empty VAO bound,
// so there is no VBO to allocate, bind, or churn per frame.
//
// v_uv is the normalized 0..1 screen coordinate (origin bottom-left), handed to
// the fragment shader. The clip-space positions span -1..3 so the triangle's
// hypotenuse falls outside the screen and the visible region is exactly the
// fullscreen quad.

out vec2 v_uv;

void main() {
    // id 0 -> (0,0), id 1 -> (2,0), id 2 -> (0,2) in UV space.
    v_uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
}
