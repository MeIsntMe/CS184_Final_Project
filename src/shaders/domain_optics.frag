#version 330 core

in vec3 vWorldPos;
in vec3 vCamWorldPos;

uniform vec3 domainCenter;
uniform vec3 domainSize;
uniform sampler3D densityTexture;
uniform vec3 scatteringCoefficients;
uniform float densityMultiplier;
uniform vec3 lightPos;
uniform vec3 sphereCenter;
uniform float sphereRadius;

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


// Returns distance to sphere entry, or -1 if miss
float sphereIntersect(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0) return -1.0;
    float t = -b - sqrt(disc);
    return (t > 0.0) ? t : -1.0;
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

    float surfaceThreshold = 1.0; // Density at which the fluid becomes "solid"
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
    // Sphere rendering (glass ball)
    float tSphere = (sphereRadius > 0.0) ? sphereIntersect(vCamWorldPos, rayDir, sphereCenter, sphereRadius) : -1.0;
    float tFluid  = hitSurface ? length(hitWorldPos - vCamWorldPos) : 1e9;
    bool hitSphereFirst = (tSphere > 0.0 && tSphere < tFluid);

    vec3 normal = vec3(0.0);
    if (hitSphereFirst) {
        vec3 sHitPos = vCamWorldPos + rayDir * tSphere;
        vec3 sNormal = normalize(sHitPos - sphereCenter);

        float fresnel = pow(1.0 - max(dot(sNormal, -rayDir), 0.0), 5.0);
        fresnel = mix(0.04, 1.0, fresnel);

        vec3 reflDir = reflect(rayDir, sNormal);
        vec3 refrDir = refract(rayDir, sNormal, 1.0 / 1.5); // glass IOR

        // Reflection: check if reflected ray hits fluid
        vec3 reflColor = backgroundColor;
        vec3 reflPos = sHitPos + reflDir * (stepSize * 2.0);
        for (int i = 0; i < 60; i++) {
            vec3 tp = (reflPos - boxMin) / domainSize;
            if (any(lessThan(tp, vec3(0.0))) || any(greaterThan(tp, vec3(1.0)))) break;
            if (texture(densityTexture, tp).r > surfaceThreshold) {
                reflColor = vec3(scatteringCoefficients) * 0.5;
                break;
            }
            reflPos += reflDir * stepSize;
        }

        // Refraction: march through glass sphere to exit, then into scene
        vec3 refrColor = backgroundColor;
        vec3 refrPos = sHitPos + refrDir * (stepSize * 2.0);
        // find sphere exit
        vec3 ocExit = refrPos - sphereCenter;
        float bE = dot(ocExit, refrDir);
        float cE = dot(ocExit, ocExit) - sphereRadius * sphereRadius;
        float discE = bE * bE - cE;
        if (discE >= 0.0) {
            float tExit = -bE + sqrt(discE);
            vec3 exitPos = refrPos + refrDir * tExit;
            vec3 exitNormal = normalize(sphereCenter - exitPos); // inward → outward at exit
            vec3 refrDir2 = refract(refrDir, -exitNormal, 1.5); // glass→air
            if (length(refrDir2) < 0.001) refrDir2 = refrDir; // total internal reflection fallback
            vec3 marchPos = exitPos + refrDir2 * (stepSize * 2.0);
            for (int i = 0; i < 80; i++) {
                vec3 tp = (marchPos - boxMin) / domainSize;
                if (any(lessThan(tp, vec3(0.0))) || any(greaterThan(tp, vec3(1.0)))) break;
                if (texture(densityTexture, tp).r > surfaceThreshold) {
                    refrColor = vec3(scatteringCoefficients) * 0.6;
                    break;
                }
                marchPos += refrDir2 * stepSize;
            }
        }

        vec3 finalColor = mix(refrColor, reflColor, fresnel);
        FragColor = vec4(finalColor, 1.0);

    } else if(hitSurface) {
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
        vec3 refractionColor = backgroundColor * exp(-opticalDepth * scatteringCoefficients * densityMultiplier);

        vec3 finalColor = mix(refractionColor, reflectionColor, fresnel);
        //FragColor = vec4(finalColor, 1.0);

        // ACES filmic tone mapping
        vec3 tonemapped = clamp((finalColor * (2.51 * finalColor + 0.03)) /
                                (finalColor * (2.43 * finalColor + 0.59) + 0.14), 0.0, 1.0);
        FragColor = vec4(finalColor, 1.0);
        
    } else {
        // The primary camera ray missed the fluid entirely
        FragColor = vec4(1.0-backgroundColor, 0.0);
    }
}