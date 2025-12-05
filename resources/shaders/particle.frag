#version 330 core

in vec4 fragColor;
in vec2 texCoord;

out vec4 finalColor;

uniform int uType; // 0 = Snow, 1 = Rain

void main() {
    vec2 coord = texCoord * 2.0 - 1.0; // Map to [-1, 1]
    float alpha = 0.0;

    if (uType == 0) { // Snow
        // Soft circular shape
        float dist = length(coord);
        if (dist > 1.0) discard;
        alpha = 1.0 - smoothstep(0.5, 1.0, dist); // Softer edge
    } else { // Rain
        // Streak shape
        // Fade out towards edges of X
        // Fade out towards ends of Y
        float distX = abs(coord.x);
        float distY = abs(coord.y);
        
        // Elliptical / Streak falloff
        // Combine horizontal fade with vertical fade
        // Make it look like a teardrop or streak
        if (distX > 1.0 || distY > 1.0) discard;
        
        alpha = (1.0 - distX) * (1.0 - smoothstep(0.0, 1.0, distY * distY)); 
        // distY*distY makes it fade faster at tips? No, let's keep it simple.
        // alpha = (1.0 - distX) * (1.0 - distY);
    }
    
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
