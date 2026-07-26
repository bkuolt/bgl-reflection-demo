#version 460 core

in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform bool uUseTexture;
uniform bool uIsCheckerboard;
uniform bool uEnableLighting;
uniform vec3 uViewPos;

void main() {
    if (uIsCheckerboard) {
        // High performance procedural checkerboard pattern in OpenGL 4.6 Core
        vec2 st = gl_FragCoord.xy / 24.0;
        float checker = mod(floor(st.x) + floor(st.y), 2.0);
        vec3 col = mix(vec3(0.12, 0.12, 0.16), vec3(0.26, 0.26, 0.32), checker);
        FragColor = vec4(col, 1.0);
        return;
    }

    vec4 baseColor = uUseTexture ? (texture(uTexture, TexCoord) * uColor) : uColor;

    if (!uEnableLighting) {
        FragColor = baseColor;
        return;
    }

    // Two-sided smooth normal handling
    vec3 N = normalize(Normal);
    if (length(N) < 0.001) {
        N = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    }
    if (length(N) < 0.001) {
        N = vec3(0.0, 1.0, 0.0);
    }

    // Directional Sun Light (Primary + Secondary fill light for vivid illumination)
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));
    float diff1 = max(dot(N, lightDir), 0.0);
    float diff2 = max(dot(-N, lightDir), 0.0) * 0.4;
    float diff = diff1 + diff2;

    // Rich Ambient & Diffuse lighting
    vec3 ambient = 0.55 * baseColor.rgb;
    vec3 diffuse = 0.65 * diff * baseColor.rgb;

    // Specular Component (Blinn-Phong)
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(N, halfDir), 0.0), 32.0);
    vec3 specular = vec3(0.3) * spec;

    vec3 finalColor = ambient + diffuse + specular;
    FragColor = vec4(finalColor, baseColor.a);
}
