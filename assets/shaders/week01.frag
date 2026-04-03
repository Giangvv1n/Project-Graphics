#version 330 core
out vec4 FragColor;
uniform float uTime;
void main() {
    float r = 0.5 + 0.5 * sin(uTime);
    float g = 0.5 + 0.5 * sin(uTime + 2.094); // +120deg
    float b = 0.5 + 0.5 * sin(uTime + 4.188); // +240deg
    FragColor = vec4(r, g, b, 1.0);
}
