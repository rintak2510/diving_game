#version 330 core
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_center;
uniform vec3 baseColor;

void main() {
    vec2 pos = gl_FragCoord.xy / 800.0;  // 仮に800x600の画面解像度前提
    float dist = distance(pos, u_center / 800.0);
    float wave = sin(30.0 * dist - u_time * 5.0) * 0.1;
    float alpha = 0.6 - dist * 0.5 + wave;
    FragColor = vec4(baseColor, clamp(alpha, 0.0, 1.0));
}
