#version 330 core

out vec4 FragColor;

// Depth received from the vertex shader.
in float vDepth;

// Values sent from C++.
uniform vec3 particleColor;
uniform vec3 fogColor;
uniform float fogDensity;

void main() {
    // GL_POINTS are square by default.
    // gl_PointCoord goes from (0, 0) to (1, 1) inside each point.
    // This remaps it so the center is (0, 0).
    vec2 centeredCoord = gl_PointCoord * 2.0 - 1.0;

    // Distance from the center of the point.
    float radiusSquared = dot(centeredCoord, centeredCoord);

    // Remove pixels outside the circle.
    // This makes the particle round instead of square.
    if (radiusSquared > 1.0) {
        discard;
    }

    // Soft edge.
    // Center is more opaque, edge fades out.
    float alpha = exp(-radiusSquared * 3.0);

    // Exponential distance fog.
    // Bigger vDepth = farther away = more fog.
    // Bigger fogDensity = thicker fog.
    float fogAmount = 1.0 - exp(-fogDensity * vDepth);
    fogAmount = clamp(fogAmount, 0.0, 1.0);

    // Blend particle color toward fog color.
    vec3 finalColor = mix(particleColor, fogColor, fogAmount);

    FragColor = vec4(finalColor, alpha);
}