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

vec3 calculateNormal(vec3 texPos) {
    vec3 offset = vec3(0.01, 0.0, 0.0); 
    
    // Clamp the coordinates so they can never exceed 1.0 or drop below 0.0
    float dx = texture(densityTexture, clamp(texPos + offset.xyy, 0.0, 1.0)).r - 
               texture(densityTexture, clamp(texPos - offset.xyy, 0.0, 1.0)).r;
               
    float dy = texture(densityTexture, clamp(texPos + offset.yxy, 0.0, 1.0)).r - 
               texture(densityTexture, clamp(texPos - offset.yxy, 0.0, 1.0)).r;
               
    float dz = texture(densityTexture, clamp(texPos + offset.yyx, 0.0, 1.0)).r - 
               texture(densityTexture, clamp(texPos - offset.yyx, 0.0, 1.0)).r;
    
    vec3 n = vec3(-dx, -dy, -dz);
    
    if(length(n) < 0.0001) {
        return vec3(0.0, 1.0, 0.0); 
    }
    
    return normalize(n); 
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

    float surfaceThreshold = 0.5; // Density at which the fluid becomes "solid"
    vec3 backgroundColor = vec3(1.0); // Pure white environment
    
    bool hitSurface = false;
    vec3 hitWorldPos;
    vec3 hitTexPos;

    for(int i = 0; i < 300; i++) {
        // Stop if we exit the back of the box
        if(tNear > tFar) break;

        // Convert the current world position to 0..1 Texture Space to read the density
        vec3 texPos = (currentWorldPos - boxMin) / domainSize;
        float density = texture(densityTexture, texPos).r;

        if (density > surfaceThreshold) {
            hitSurface = true;
            hitWorldPos = currentWorldPos;
            hitTexPos = texPos;
            break;
        }
        currentWorldPos += rayDir * stepSize;
        tNear += stepSize;
    }
    vec3 normal = vec3(0.0);
    if(hitSurface) {
        vec3 normal = calculateNormal(hitTexPos);
        
        // Fresnel: 1.0 at grazing angles (edges), near 0.0 looking straight on
        float fresnel = pow(1.0 - max(dot(normal, -rayDir), 0.0), 5.0);
        fresnel = mix(0.1, 1.0, fresnel); // Ensure it's always at least 10% reflective

        // --- 3. SELF-REFLECTION ---
        vec3 reflDir = reflect(rayDir, normal);
        // BUMP the ray slightly outside the surface so it doesn't self-intersect
        vec3 reflWorldPos = hitWorldPos + normal * (stepSize * 4.0); 
        vec3 reflectionColor = backgroundColor; // Default to white if it hits nothing
        
        // March the reflection ray
        for(int i = 0; i < 100; i++) {
            vec3 texPos = (reflWorldPos - boxMin) / domainSize;
            
            // If the reflection ray leaves the box, it hits the white background
            if(any(lessThan(texPos, vec3(0.0))) || any(greaterThan(texPos, vec3(1.0)))) {
                break; 
            }
            
            if(texture(densityTexture, texPos).r > surfaceThreshold) {
                // The reflection ray hit ANOTHER part of the fluid! 
                // You can replace this with a lighting calculation if you want shading in the reflections
                reflectionColor = vec3(scatteringCoefficients); // Base fluid color
                break;
            }
            reflWorldPos += reflDir * stepSize;
        }

        // --- 4. VOLUMETRIC REFRACTION ---
        float iorRatio = 1.0 / 1.33; // Air to Water
        vec3 refrDir = refract(rayDir, normal, iorRatio);
        // BUMP the ray slightly inside the surface
        vec3 refrWorldPos = hitWorldPos - normal * (stepSize * 4.0); 
        
        // We will accumulate absorption as the bent ray travels through the fluid
        float opticalDepth = 0.0;
        
        for(int i = 0; i < 150; i++) {
            vec3 texPos = (refrWorldPos - boxMin) / domainSize;
            
            // Stop when the refracted ray exits the back of the fluid tank
            if(any(lessThan(texPos, vec3(0.0))) || any(greaterThan(texPos, vec3(1.0)))) {
                break; 
            }
            
            float d = texture(densityTexture, texPos).r;
            if(d > 0.01) {
                opticalDepth += d * stepSize;
            }
            refrWorldPos += refrDir * stepSize;
        }
        
        // Tint the white background based on how much fluid the refracted ray traveled through
        // (This acts exactly like colored glass)
        vec3 refractionColor = backgroundColor * exp(-opticalDepth * scatteringCoefficients * densityMultiplier);

        // --- 5. FINAL BLEND ---
        vec3 finalColor = mix(refractionColor, reflectionColor, fresnel);
        FragColor = vec4(finalColor, 1.0);
        
    } else {
        // The primary camera ray missed the fluid entirely
        FragColor = vec4(1.0-backgroundColor, 0.0);
    }
}