// Pixel Shaders that simulate geometry by raytracing geometric primitives
#include "common.hlsli"

//**********************************************
//			Raytracing Sphere
//**********************************************
struct GSIn_CamOrientedSprite {
	float3 pos : POSITION;
	float3 col : COLOR;
	float size : SPRITE_SIZE;
};

struct PSIn_RTSphere {
	float4 pos : SV_POSITION;
	float4 worldPos : WORLD_POSITION;
	float2 tex : SPRITE_TEXTURE;
	nointerpolation float3 col : COLOR;
	nointerpolation float3 centerVol : PARTICLE_CENTER_VOLUME;
	nointerpolation float3 centerWorld : PARTICLE_CENTER_WORLD;
	nointerpolation float radius : SPHERE_RADIUS;
};


[maxvertexcount(4)] 
void gsExtrudeSprite(point GSIn_CamOrientedSprite particle[1], inout TriangleStream<PSIn_RTSphere>stream) {

	PSIn_RTSphere o;

	float4 centerWorld = mul(float4(particle[0].pos, 1), g_modelWorld);
	centerWorld /= centerWorld.w;

	//NOTE: "right" and "up" are not really right and up in view space, but simply form a orthonormal coordinate system with "forward"
	float3 camForward = normalize(centerWorld.xyz - g_lightPos); //light pos equals camera pos
	float3 camRight = normalize(cross(camForward.xyz, camForward.yzx));
	float3 camUp = normalize(cross(camForward, camRight));

	o.centerVol = particle[0].pos;
	o.centerWorld = centerWorld.xyz;
	o.col = particle[0].col;
	o.radius = particle[0].size;

	o.tex = float2(0,1);
	o.worldPos = float4(centerWorld.xyz + particle[0].size*camUp - particle[0].size*camRight - particle[0].size*camForward, 1);
	o.pos = mul(o.worldPos, g_viewProj);
	stream.Append(o);

	o.tex = float2(1,1);
	o.worldPos = float4(centerWorld.xyz + particle[0].size*camUp + particle[0].size*camRight - particle[0].size*camForward, 1);
	o.pos = mul(o.worldPos, g_viewProj);
	stream.Append(o);

	o.tex = float2(0,0);
	o.worldPos = float4(centerWorld.xyz - particle[0].size*camUp - particle[0].size*camRight - particle[0].size*camForward, 1);
	o.pos = mul(o.worldPos, g_viewProj);
	stream.Append(o);

	o.tex = float2(1,0);
	o.worldPos = float4(centerWorld.xyz - particle[0].size*camUp + particle[0].size*camRight - particle[0].size*camForward, 1);
	o.pos = mul(o.worldPos, g_viewProj);
	stream.Append(o);
}

// sphere raytracing pixel shader with ortographic optimizations
float4 psRTSphereOrto(PSIn_RTSphere p, out float d : SV_DEPTH) : SV_Target
{
	//the center of the sphere is at (0,0,0) with radius 1
	//we start our rays on the front face of the bounding box, i.e. with tex coordinates -1 in z direction 
	//float3 dir = float3(0,0,1);
	//float3 org = float3(p.tex, -1);

	//float3 L = org; // - center, but center is at (0,0,0)
	//float a = 1;// = dot(dir, dir); //since dir is normalized
	//float b = -2; // = 2 * L.z;// = 2 * dot(dir, L); //since dir=float(0,0,1)
	//p.tex is the 2d texture coordinate of the sprite
	float c = dot(p.tex, p.tex);// = dot(L, L) - 1; //since radius is 1
	

	
	//determine the minimal solution for the quadratic equation
	float tMin = -1;
	float discr = 4 - 4 * c; 
	if (discr < 0) 
		discard; // no intersection - discard fragment
	else { 
		if (discr == 0) 
			tMin = 1;//- 0.5 * b; //one intersection - unique
		else { 
			float q = 1 + sqrt(discr);// = -0.5 * (b - sqrt(discr)); 
			tMin = min(q, c / q); //take the minimum of the two intersections
		} 
	}


	// the intersection position in particle texture space
	float3 texPos = float3(p.tex, tMin -1);
	
	// the intersection / surface position in volume texture space
	float3 surfaceVolPos = p.centerVol + p.radius*texPos.y*g_camUp + p.radius*texPos.x*g_camRight + p.radius*texPos.z*g_camForward;
	float3 surfaceVolNormal = normalize(p.centerVol - surfaceVolPos);

	float4 surfaceWorldPos = mul(float4(surfaceVolPos, 1), g_modelWorld);
	surfaceWorldPos /= surfaceWorldPos.w;
	float3 n = normalize(mul(float4(surfaceVolNormal, 0), g_modelWorldInvTransp).xyz);

	//lighting
	float3 l = normalize(surfaceWorldPos.xyz - g_lightPos);
	float3 v = l;						//light direction and viewing direction are the same for our "headlight"
	float3 rl = reflect(-l, n);

	float3 specular = g_lightColor * pow(saturate(dot(rl, v)), g_specularExp);
	float3 diffuse = g_lightColor * p.col * saturate(dot(n, l));
	float3 ambient = p.col;

	float3 col = (k_s * specular) + (k_d * diffuse) + k_a * ambient;

	// compute clip position to write an updated depth value
	float4 clipPos = mul(float4(surfaceVolPos, 1), g_modelWorldViewProj);
	d = clipPos.z / clipPos.w;
	return float4(col, 1);
}

/*
	determine intersection between a ray and a sphere, returning position and normal
	pr: ray starting point
	dr: ray direction
	sc: sphere center
	r2: squared sphere radius
*/
bool IntersectRaySphere(float3 pr, float3 dr, float3 sc, float r2, inout float3 intersectionPos, inout float3 intersectionNormal)
{
	// ray sphere intersection adapted from http://www.scratchapixel.com/lessons/3d-basic-lessons/lesson-7-intersecting-simple-shapes/ray-sphere-intersection/
	float3 L = pr - sc;
	float a = dot(dr, dr); 
	float b = 2 * dot(dr, L); 
	float c = dot(L, L) - r2;

	//determine the minimal solution for the quadratic equation
	float tMin = -1;

	float discr = b * b - 4 * a * c; 
	if (discr < 0) 
		return false; 
	else if (discr == 0) 
		tMin = - 0.5 * b / a; 
	else { 
		float q = (b > 0) ? -0.5 * (b + sqrt(discr)) 
						: -0.5 * (b - sqrt(discr)); 
		tMin = min(q / a, c / q);
	} 
	intersectionPos = pr + tMin * dr;
	intersectionNormal = normalize(intersectionPos - sc);
	return true;
}

float4 psRTSpherePerspective(PSIn_RTSphere p, out float d : SV_DepthGreaterEqual) : SV_Target
{
	p.worldPos /= p.worldPos.w;
	//the center of the sphere is at (0,0,0) with radius 1
	//we start our rays on the front face of the bounding box, i.e. with tex coordinates -1 in z direction 
	float3 org = p.worldPos.xyz;
	float3 dir = org - g_lightPos;

	
	float3 surfaceWorldPos, n;
	if(!IntersectRaySphere(org, dir, p.centerWorld, p.radius*p.radius, surfaceWorldPos, n)) 
		discard;
		/*{
		float4 clipPos = mul(float4(surfaceWorldPos, 1), g_viewProj);

		d = clipPos.z / clipPos.w;
		return float4(1,0, 0, 1);
	}*/
	
	
	//lighting
	float3 l = normalize(g_lightPos - surfaceWorldPos);
	float3 v = l;						//light direction and viewing direction are the same for our "headlight"
	float3 rl = reflect(-l, n);

	float3 specular = g_lightColor * pow(saturate(dot(rl, v)), g_specularExp);
	float3 diffuse = g_lightColor * p.col * saturate(dot(n, l));
	float3 ambient = p.col;

	float3 col = (k_s * specular) + (k_d * diffuse) + k_a * ambient;

	// compute clip position to write an updated depth value
	float4 clipPos = mul(float4(surfaceWorldPos, 1), g_viewProj);

	d = clipPos.z / clipPos.w;
	return float4(col, 1);
}


//**************************************************
//					Raytracing Pill
//**************************************************
// Note: Pill is a cylinder with spheres as end caps

struct PSIn_RTPill {
	float4 pos : SV_POSITION;
	float4 worldPos : WORLD_POSITION;
	float3 col : COLOR;
	nointerpolation float3 bottomWorld : BOTTOM_WORLD;
	nointerpolation float3 topWorld : TOP_WORLD;
	nointerpolation float size : SIZE;
};

//pushes 36 vertices to the pipeline
//expects start and end in world coordinates
void ExtrudePillContainerBox(float3 start, float3 end, float3 colorStart, float3 colorEnd, float size, inout TriangleStream<PSIn_RTPill> stream) 
{
	PSIn_RTPill t;

	t.size = size;

	// three orthogonal directions in world space
	float3 z = end.xyz - start.xyz;
	float3 y = 1.42 * t.size * normalize(cross(z, z.yzx)); //scale with sqrt(2) + epsilon because those are the corners
	float3 x = 1.42 * t.size * normalize(cross(z, y));

	t.bottomWorld = start.xyz;
	t.topWorld = end.xyz;
	
	//we extrude above the cylinder to have space for the circular end cap
	end.xyz += t.size * normalize(z);
	start.xyz -= t.size * normalize(z);

	// quad 1 - yellow
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(1, 1, 0); 
#endif
	t.worldPos = float4(start.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(start.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();

	// quad 2 - teal
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(0, 1, 1);
#endif
	t.worldPos = float4(start.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(start.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();

	// quad 3 - pink
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(1, 0, 1);
#endif
	t.worldPos = float4(start.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(start.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();

	// quad 4 - green
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(0, 1, 0);
#endif
	t.worldPos = float4(start.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(start.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();

	// quad 5 - red
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(1, 0, 0);
#endif
	t.worldPos = float4(end.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(end.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	t.worldPos = float4(end.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorEnd.rgb; stream.Append(t);
	stream.RestartStrip();

	// quad 6 - blue
#ifdef PARTICLE_SHADER_DEBUG
	colorStart = colorEnd = float3(0, 0, 1);
#endif
	t.worldPos = float4(start.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz - y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	stream.RestartStrip();
	t.worldPos = float4(start.xyz - x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz + x, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	t.worldPos = float4(start.xyz + y, 1); t.pos = mul(t.worldPos, g_viewProj); t.col = colorStart.rgb; stream.Append(t);
	stream.RestartStrip();
}


float4 psRTPill(PSIn_RTPill v, out float d : SV_DepthGreaterEqual) : SV_Target
{
	//ray description
	float3 pr = v.worldPos.xyz / v.worldPos.w;
	float3 dr = normalize(pr - g_lightPos);

	//cylinder description
	float3 pc = v.bottomWorld;
	float3 dc = normalize(v.topWorld - v.bottomWorld);
	float r2 = v.size*v.size;

	//setup parameters for quadratic equation
	float3 deltap = pr - pc;
	float3 t1 = dot(dr, dc) * dc;
	float3 t2 = (dr - t1);
	float A = dot(t2, t2);
	float3 t3 = dot(deltap, dc) * dc;
	float B = 2 * dot(t2, deltap - t3);
	float3 t4 = deltap - t3;
	float C = dot(t4, t4) - r2;

	//solve quadratic equation
	float discr = B*B - 4*A*C;
	if(discr < 0) {
		discard;//return float4(1, 0, 0, 1); //no intersection at all
	}

	float s = sqrt(discr);
	float tMin = min(-B+s, -B-s)  / (2 * A);

	//intersection point in world space
	float3 surfaceWorldPos = pr + tMin * dr;
	float3 n; //surface normal

	//check if intersection is below the cylinder caps
	//if they are above or below the cylinder, we do an intersection test for the
	//end caps of the cylinder
	if(dot(dc, surfaceWorldPos - v.bottomWorld) < 0.0) {
		if(!IntersectRaySphere(pr, dr, v.bottomWorld, r2, surfaceWorldPos, n))
			discard;
	}
	else if(dot(dc, surfaceWorldPos - v.topWorld) > 0.0) {
		if(!IntersectRaySphere(pr, dr, v.topWorld, r2, surfaceWorldPos, n))
			discard;		
	}
	else {
		float3 d = surfaceWorldPos - v.bottomWorld;
		n = (d - dot(d, dc)*dc)/v.size;
	}

	// compute clip position to write an updated depth value
	float4 clipPos = mul(float4(surfaceWorldPos, 1), g_viewProj);
	d = clipPos.z / clipPos.w;

	//lighting
	return WorldSpaceLighting(surfaceWorldPos, n, float4(v.col, 1));
}