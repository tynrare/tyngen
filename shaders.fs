// File version: 1.0.0 | Last updated: 2026-10-03 | Purpose: Composite transparent images over a viewer checkerboard.
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
out vec4 finalColor;

void main() {
    vec4 image = texture(texture0, fragTexCoord) * fragColor;
    float tile = mod(floor(gl_FragCoord.x / 12.0) + floor(gl_FragCoord.y / 12.0), 2.0);
    vec3 checker = mix(vec3(0.12), vec3(0.19), tile);
    finalColor = vec4(mix(checker, image.rgb, image.a), 1.0);
}
