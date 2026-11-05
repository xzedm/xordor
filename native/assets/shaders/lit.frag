// Lit surface, fragment stage: runs once for every pixel a triangle covers.
// Lighting model: Blinn-Phong with one directional light (the sun) plus a
// "hemisphere" ambient term (sky color from above, ground color from below).
// All math happens on linear colors; the very last line converts for display.
#version 410 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;

uniform sampler2D uAlbedo;      // the surface texture (sRGB, decoded to linear by the GPU)
uniform vec3 uBaseColor;        // multiplied with the texture (linear)
uniform float uSpecular;        // strength of the shiny highlight, 0..1
uniform float uShininess;       // highlight tightness: 8 = broad and dull, 128 = small and sharp

uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;   // the direction the light travels (from the sun toward the ground)
uniform vec3 uLightColor;       // color * intensity
uniform vec3 uSkyAmbient;
uniform vec3 uGroundAmbient;

out vec4 fragColor;

void main() {
    // Interpolating unit vectors across a triangle makes them slightly shorter,
    // so normalize again per pixel.
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDirection);                       // toward the light
    vec3 V = normalize(uCameraPosition - vWorldPosition);       // toward the eye

    vec3 albedo = texture(uAlbedo, vUv).rgb * uBaseColor;

    // Diffuse (Lambert): light hitting the surface head-on is brightest;
    // at a grazing angle it spreads over more area, so it's dimmer.
    float diffuse = max(dot(N, L), 0.0);

    // Specular (Blinn-Phong): the "half vector" between light and eye. When it
    // lines up with the normal, we see a mirror-like reflection of the light.
    vec3 H = normalize(L + V);
    float specular = diffuse > 0.0 ? pow(max(dot(N, H), 0.0), uShininess) * uSpecular : 0.0;

    // Hemisphere ambient: surfaces facing up get sky light, facing down get
    // light bounced off the ground. A cheap stand-in for global illumination.
    vec3 ambient = mix(uGroundAmbient, uSkyAmbient, N.y * 0.5 + 0.5);

    vec3 color = albedo * (ambient + uLightColor * diffuse) + uLightColor * specular;

    // Linear -> display. Monitors expect gamma-encoded values; without this,
    // shadows look too dark and light falloff looks harsh. 2.2 approximates sRGB.
    color = pow(color, vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
