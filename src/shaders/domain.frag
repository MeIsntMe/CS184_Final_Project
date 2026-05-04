#version 330 core

in vec3 vWorldPos;
in vec3 vCamWorldPos;

uniform vec3 domainCenter;
uniform vec3 domainSize;
uniform sampler3D densityTexture;

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
    
    // 3. Setup World-Space Marching
    vec3 currentWorldPos = vCamWorldPos + rayDir * tNear;
    
    // Since we are marching in World Space now, your step size depends on your domain size. 
    // If domainSize is 10.0, a step of 0.05 gives you 200 high-quality samples.
    float stepSize = 0.05; 
    
    float transmittance = 1.0;
    vec3 scatteredLight = vec3(0.0);
    
    // 4. The March
    for(int i = 0; i < 300; i++) {
        // Stop if we exit the back of the box
        if(tNear > tFar) break;
        
        // Convert the current world position to 0..1 Texture Space to read the density
        vec3 texPos = (currentWorldPos - boxMin) / domainSize;
        float density = texture(densityTexture, texPos).r;
        
        if(density > 0.01) {
            // ... (Your lighting math, absorption, and accumulation here) ...
            
            transmittance *= exp(-density * stepSize); // Example accumulation
            scatteredLight += density * transmittance * stepSize; 
        }
        
        // Step forward in World Space
        currentWorldPos += rayDir * stepSize;
        tNear += stepSize;
        
        if(transmittance < 0.01) break; // Early exit if opaque
    }
    
    FragColor = vec4(scatteredLight, 1.0 - transmittance);
}