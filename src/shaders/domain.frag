#version 330 core

in vec3 vWorldPos;
in vec3 vCamWorldPos;

uniform vec3 domainCenter;
uniform vec3 domainSize;
uniform sampler3D densityTexture;
uniform vec3 scatteringCoefficients;
uniform float densityMultiplier;
uniform vec3 lightPos;

out vec4 FragColor;

// Calculates the exact entry and exit distances of a ray hitting a box
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
}

float densityAlongRay(vec3 pos, vec3 dir, float stepSize, vec3 boxMin) {
    float densityAccum = 0.0;
    vec3 currentPos = pos;
    
    for (int i = 0; i < 100; i++) {
        vec3 texPos = (currentPos - boxMin) / domainSize;
        // Stop sampling if the shadow ray exits the 3D texture
        if(any(lessThan(texPos, vec3(0.0))) || any(greaterThan(texPos, vec3(1.0)))) {
            break; 
        }
        densityAccum += texture(densityTexture, texPos).r * stepSize * densityMultiplier;
        currentPos += dir * stepSize;
    }
    return densityAccum;
}


void main() {
    // 1. Ray direction in perfect, unwarped World Space
    vec3 rayDir = normalize(vWorldPos - vCamWorldPos);
    
    vec3 boxMin = domainCenter - (domainSize * 0.5);
    vec3 boxMax = domainCenter + (domainSize * 0.5);
    
    // 2. Find where the ray hits the domain box
    vec2 hit = intersectAABB(vCamWorldPos, rayDir, boxMin, boxMax);
    

    // If the camera is inside the box, hit.x is negative (behind the camera).
    // This forces the ray to start exactly at the camera lens instead.
    float tNear = max(0.0, hit.x); 
    float tFar = hit.y;
    
    // If the ray misses the box entirely, or is fully behind us, discard
    if(tNear >= tFar) {
        discard; 
    }
  
    vec3 currentWorldPos = vCamWorldPos + rayDir * tNear;
    
    float stepSize = 0.01; 
    
    vec3 transmittance = vec3(1.0);
    vec3 scatteredLight = vec3(0.0);

    for(int i = 0; i < 300; i++) {
        // Stop if we exit the back of the box
        if(tNear > tFar) break;
        
        // Convert the current world position to 0..1 Texture Space to read the density
        vec3 texPos = (currentWorldPos - boxMin) / domainSize;
        float density = texture(densityTexture, texPos).r;

        float densityAlongSunRay = densityAlongRay(currentWorldPos, normalize(lightPos), stepSize*10, boxMin);
        vec3 transmittedSunlight = exp(-densityAlongSunRay*scatteringCoefficients);
        transmittance *= exp(-density * stepSize * densityMultiplier * scatteringCoefficients); // Example accumulation
        scatteredLight += transmittedSunlight * density * stepSize * transmittance * scatteringCoefficients; 

        // Step forward in World Space
        currentWorldPos += rayDir * stepSize;
        tNear += stepSize;
        
        if(length(transmittance)< 0.01) break; // Early exit if opaque
    }
    float alphaTransmittance = (transmittance.r + transmittance.g + transmittance.b) / 3.0;
    // ACES filmic tone mapping
    vec3 tonemapped = clamp((scatteredLight * (2.51 * scatteredLight + 0.03)) /
                            (scatteredLight * (2.43 * scatteredLight + 0.59) + 0.14), 0.0, 1.0);
    FragColor = vec4(tonemapped, 1.0 - alphaTransmittance);
}