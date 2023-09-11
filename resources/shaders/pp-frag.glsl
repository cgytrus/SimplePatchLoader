#version 150

in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D screen;
uniform bool fast;
uniform bool first;
uniform float radius;

void main() {
    float scaledRadius = radius * textureSize(screen, 0).y * 0.5;
    vec2 texOffset = 1.0 / textureSize(screen, 0); // gets size of single texel

    vec3 result = texture(screen, TexCoords.st).rgb;
    if (fast) {
        scaledRadius *= radius * 10.0 / ((radius * 10.0 + 1.0) * (radius * 10.0 + 1.0) - 1.0);
        float weight = 1.0;
        float weightSum = weight;
        if (first) {
            for (int i = 1; float(i) < scaledRadius; i++) {
                weight -= 1.0 / scaledRadius;
                weightSum += weight * 2.0;
                result += texture(screen, TexCoords.st + vec2(texOffset.x * i, 0.0)).rgb * weight;
                result += texture(screen, TexCoords.st - vec2(texOffset.x * i, 0.0)).rgb * weight;
            }
        }
        else {
            for (int i = 1; float(i) < scaledRadius; i++) {
                weight -= 1.0 / scaledRadius;
                weightSum += weight * 2.0;
                result += texture(screen, TexCoords.st + vec2(0.0, texOffset.y * i)).rgb * weight;
                result += texture(screen, TexCoords.st - vec2(0.0, texOffset.y * i)).rgb * weight;
            }
        }
        result /= weightSum;
    }
    else {
        float firstWeight = 0.84089642 / pow(scaledRadius, 0.96);
        result *= firstWeight;
        float weightSum = firstWeight;
        if (first) {
            for (int i = 1; float(i) <= ceil(scaledRadius); i++) {
                float weight = firstWeight * exp(-i * i / (2.0 * scaledRadius));
                weightSum += weight * 2;
                result += texture(screen, TexCoords.st + vec2(texOffset.x * i, 0.0)).rgb * weight;
                result += texture(screen, TexCoords.st - vec2(texOffset.x * i, 0.0)).rgb * weight;
            }
        }
        else {
            for (int i = 1; float(i) <= ceil(scaledRadius); i++) {
                float weight = firstWeight * exp(-i * i / (2.0 * scaledRadius));
                weightSum += weight * 2;
                result += texture(screen, TexCoords.st + vec2(0.0, texOffset.y * i)).rgb * weight;
                result += texture(screen, TexCoords.st - vec2(0.0, texOffset.y * i)).rgb * weight;
            }
        }
        result /= weightSum;
    }

    FragColor = vec4(result, 1.0);
}
