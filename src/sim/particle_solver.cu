__global__


p.vy -= gravity * dt;

p.x += p.vx * dt;
p.y += p.vy * dt;

if (p.x < -1.f) { p.x = -1.f; p.vx *= -0.5f; }
if (p.x > 1.f) { p.x = 1.f; p.vx *= -0.5f; }
if (p.y < -1.f) { p.y = -1.f; p.vy *= -0.5f; }
if (p.y > 1.f) { p.y = 1.f; p.vy *= -0.5f; }