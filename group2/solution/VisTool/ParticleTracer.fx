//--------------------------------------------------------------------------------------
// File: ParticleTracer.fx
//
// Collection of Shaders to handle particle advection and rendering as well as
// computation and rendering of characteristic lines
//
// Note: All fx-files in the project are automatically compiled by Visual Studio
//       to fxo-files in the output directories "x64\Debug\" and "x64\Release\"
//       (see "HLSL Compiler" in the project properties).
//--------------------------------------------------------------------------------------
#include "common.hlsli"
#include "TransparencyModuleInterface.hlsli"

//#define PARTICLE_SHADER_DEBUG 1

cbuffer cbParticleTracer
{
	float4x4	g_modelWorldViewProj;
	float4x4	g_modelWorld;
	float4x4	g_modelWorldView;
	float4x4	g_modelWorldInvTransp;
	float4x4	g_viewProj;

	float3		g_camRight = float3(1,0,0);	//x axis in view space projected to texture space
	float3		g_camUp = float3(0,1,0);	//y axis in view space -"-
	float3		g_camForward = float3(0,0,1);//z axis in view space -"-

	float		g_maxParticleLifetime;
	float		g_timestepT;
	float		g_timeDelta;
	float3		g_velocityScaling;
	float3		g_rnd;
	uint		g_numParticles;

	float3		g_spawnRegionMin;
	float3		g_spawnRegionMax;

	float		g_particleSize = .01;
	float4		g_particleColor;

	bool		g_colorParticles;
};

struct Particle {
	float3 pos;
	float age;
	float3 seedPos;
	float ageSeed;
};

Texture3D<float3> g_flowFieldTex0;
Texture3D<float3> g_flowFieldTex1;

Texture3D<float> g_scalarVolume;
Texture1D<float4> g_transferFunction;

StructuredBuffer<Particle> g_particleBuffer;
RWStructuredBuffer<Particle> g_particleBufferRW;

// samples the volue dataset 
// either directly if we are not time dependent, or
// interpolating between the two textures
float3 SampleFlowField(float3 p) {
	if(g_timestepT == 0)
		return g_flowFieldTex0.SampleLevel(samLinear, p, 0);
	else
		return g_timestepT * g_flowFieldTex1.SampleLevel(samLinear, p, 0) + (1. - g_timestepT) * g_flowFieldTex0.SampleLevel(samLinear, p, 0);
}

#include "GeometricPixelShaders.hlsli"


// the particles are advected in texture space, so we transform through the whole matrix
GSIn_CamOrientedSprite vsParticle(uint vertexID : SV_VertexID)
{
	GSIn_CamOrientedSprite p;
	float3 v = g_particleBuffer[vertexID].pos;
	
	p.pos = v;
	p.col = g_particleColor.xyz;
	p.size = g_particleSize;
	
	return p;
}

GSIn_CamOrientedSprite vsParticleColored(uint vertexID : SV_VertexID)
{
	GSIn_CamOrientedSprite p;
	float3 v = g_particleBuffer[vertexID].pos;
	
	p.pos = v;
	p.col = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, v, 0), 0).xyz;
	p.size = g_particleSize;
	
	return p;
}


void reseedParticle(uint pId) {
	uint rndID = pId;//(pId + g_rnd[0] * g_numParticles)%g_numParticles;
	g_particleBufferRW[pId].pos = g_particleBufferRW[rndID].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
}

[numthreads(256,1,1)]	
void CSAdvect(uint3 threadID: SV_DispatchThreadID)
{
	uint pId = threadID.x;
	if(pId >= g_numParticles)
		return;

	if(!all(max(g_particleBufferRW[pId].pos, float3(0,0,0))) 
		|| any(max(g_particleBufferRW[pId].pos, float3(1,1,1)) - float3(1,1,1))) 
	{
		reseedParticle(pId);
	}

	if(g_particleBufferRW[pId].age > g_maxParticleLifetime) 
	{
		reseedParticle(pId);
		g_particleBufferRW[pId].age = fmod(g_particleBufferRW[pId].age, g_maxParticleLifetime) + g_particleBufferRW[pId].ageSeed * g_maxParticleLifetime;
	}

	g_particleBufferRW[pId].pos = g_particleBufferRW[pId].pos + g_timeDelta * SampleFlowField(g_particleBufferRW[pId].pos) * g_velocityScaling;
	g_particleBufferRW[pId].age += g_timeDelta;
}

//	****************************************************
//					Characteristic Lines / Surfaces
//	****************************************************
cbuffer cbCharacteristicLine {
	float		g_clWidth;
	float		g_clStepsize;
	uint		g_clLength;
	float3		g_clRibbonBaseOrientation;
	float		g_clReseedInterval;
	bool		g_clEnableAlphaDensity;
	float		g_alphaDensityCoefficient;
	bool		g_clEnableAlphaFade;
	bool		g_clEnableAlphaShape;
	float		g_alphaShapeCoefficient = 0.75;
	bool		g_clEnableAlphaCurvature;
	float		g_alphaCurvatureCoefficient;
	float3		g_timeSurfaceLineOffset;
	bool		g_enableSurfaceLighting;
	bool		g_enableTimeSurfaces;
	uint		g_numTimeSurfaces;
};

struct CLVertex {
	float3 pos;
	float3 dir; //for ribbons, this is the tangent; for surfaces, this is the normal
	float4 color;
	float age;
	float size;
};

StructuredBuffer<CLVertex> g_characteristicLineBuffer;
RWStructuredBuffer<CLVertex> g_characteristicLineBufferRW;

struct PSIn_CharacteristicLineVertex {
	float4 pos : SV_POSITION;
	float3 col : COLOR;
	bool sameStrip : SAME_STRIP;
};

struct GSIn_CharacteristicRibbon {
	uint vertexID : VERTEX_ID;
};

struct PSIn_CharacteristicRibbon {
	float4 pos : SV_POSITION;
	float4 worldPos : WORLD_POSITION;
	float3 volPos : VOLUME_POSITION;
	float4 col : COLOR;
	float3 nor : NORMAL;
};

PSIn_CharacteristicLineVertex vsCharacteristicLine(uint vertexID : SV_VertexID)
{
	uint lineId = vertexID / (g_clLength + 1);
	uint vIdx = vertexID % (g_clLength + 1);
	uint clVertexIndex = lineId * g_clLength + (vIdx % g_clLength);

	PSIn_CharacteristicLineVertex clv;
	float3 v = g_characteristicLineBuffer[clVertexIndex].pos;
	
	clv.pos = mul(float4(v, 1), g_modelWorldViewProj);
	clv.col = g_characteristicLineBuffer[clVertexIndex].color.rgb;
	clv.sameStrip = vIdx != g_clLength && (g_characteristicLineBuffer[clVertexIndex].age > g_characteristicLineBuffer[clVertexIndex+1].age);
	
	return clv;
}

float4 psCharacteristicLine(PSIn_CharacteristicLineVertex clv) : SV_Target
{
	if(!clv.sameStrip)
		discard;

	return float4(clv.col, 1);
}

GSIn_CharacteristicRibbon vsCharacteristicRibbon(uint vertexID : SV_VertexID)
{
	GSIn_CharacteristicRibbon gsIn;
	gsIn.vertexID = vertexID;
	return gsIn;
}

[maxvertexcount(4)] 
void gsExtrudeRibbon(line GSIn_CharacteristicRibbon gsIn[2], inout TriangleStream<PSIn_CharacteristicRibbon> stream) {
	uint lineId1 = gsIn[0].vertexID / (g_clLength + 1);
	uint lineId2 = gsIn[1].vertexID / (g_clLength + 1);
	uint vIdx1 = gsIn[0].vertexID % (g_clLength + 1);
	uint clVertexIndex0 = lineId1 * g_clLength + ((vIdx1 - 1 + g_clLength) % g_clLength);
	uint clVertexIndex1 = lineId1 * g_clLength +  (vIdx1 % g_clLength);
	uint clVertexIndex2 = lineId1 * g_clLength + ((vIdx1 + 1) % g_clLength);
	uint clVertexIndex3 = lineId1 * g_clLength + ((vIdx1 + 2) % g_clLength);

	CLVertex v0 = g_characteristicLineBuffer[clVertexIndex0];
	CLVertex v1 = g_characteristicLineBuffer[clVertexIndex1];
	CLVertex v2 = g_characteristicLineBuffer[clVertexIndex2];
	CLVertex v3 = g_characteristicLineBuffer[clVertexIndex3];

	if(lineId1 != lineId2 || v1.age <= v2.age)
		return;

	PSIn_CharacteristicRibbon cr;

	
	float3 c0, c1, c2, c3; //four positions on the line, needed to provide nice normals

	// we know vertexID + 1 exist and belong to the same ribbon, but -1 and +2 can be out of bounds
	
	c1 = v1.pos;
	c2 = v2.pos;

	if(v0.age > v1.age)	c0 = v0.pos;
	else		c0 = c1;
	if(v2.age > v3.age) c3 = v3.pos;
	else		c3 = c2;


	float3 t1 = g_clWidth * v1.dir;
	float3 t2 = g_clWidth * v2.dir;

	float3 n1 = normalize(cross((c2 - c0), t1));
	float3 n2 = normalize(cross((c3 - c1), t2));
	
	cr.col = v1.color;
	cr.nor = n1;

	float4 v = float4(c1 - t1, 1);
	cr.volPos = v.xyz;
	cr.pos = mul(v, g_modelWorldViewProj);
	cr.worldPos = mul(v, g_modelWorld);cr.worldPos /= cr.worldPos.w;
	stream.Append(cr);

	v = float4(c1 + t1, 1);
	cr.volPos = v.xyz;
	cr.pos = mul(v, g_modelWorldViewProj);
	cr.worldPos = mul(v, g_modelWorld);cr.worldPos /= cr.worldPos.w;
	stream.Append(cr);

	cr.col = v2.color;
	cr.nor = n2;

	v = float4(c2 - t2, 1);
	cr.volPos = v.xyz;
	cr.pos = mul(v, g_modelWorldViewProj);
	cr.worldPos = mul(v, g_modelWorld);cr.worldPos /= cr.worldPos.w;
	stream.Append(cr);

	v = float4(c2 + t2, 1);
	cr.volPos = v.xyz;
	cr.pos = mul(v, g_modelWorldViewProj);
	cr.worldPos = mul(v, g_modelWorld);cr.worldPos /= cr.worldPos.w;
	stream.Append(cr);
}

float4 psCharacteristicRibbon(PSIn_CharacteristicRibbon cr) : SV_Target
{
	//PS-based ribbon clipping
	if(any(cr.volPos - saturate(cr.volPos)))
		discard;

	float3 n = normalize(cr.nor);

	return WorldSpaceLighting(cr.worldPos.xyz, n, cr.col);
}


/////////////////////////////////////////////////
//				 Tube rendering
/////////////////////////////////////////////////
GSIn_CamOrientedSprite vsTubeEndCap(uint vertexID : SV_VertexID)
{
	GSIn_CamOrientedSprite p;
	
	//p.pos = g_characteristicLineBuffer[vertexID].pos;
	p.pos = g_particleBuffer[vertexID].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;

	if(g_colorParticles)
		p.col = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, p.pos, 0), 0).xyz;
	else
		p.col = g_particleColor.xyz;
	
	p.size = g_clWidth;

	return p;
}


[maxvertexcount(36)]
void gsExtrudeBBox(line GSIn_CharacteristicRibbon gsIn[2], inout TriangleStream<PSIn_RTPill> stream)
{
	uint lineId0 = gsIn[0].vertexID / (g_clLength + 1);
	uint lineId1 = gsIn[1].vertexID / (g_clLength + 1);
	uint vIdx0 = gsIn[0].vertexID % (g_clLength + 1);
	uint clVertexIndex0 = lineId0 * g_clLength + (vIdx0 % g_clLength);
	uint clVertexIndex1 = lineId0 * g_clLength + ((vIdx0 + 1) % g_clLength);

	CLVertex v0 = g_characteristicLineBuffer[clVertexIndex0];
	CLVertex v1 = g_characteristicLineBuffer[clVertexIndex1];

	//we don't want to connect different lines
	if(lineId0 != lineId1)
		return;
	
	//we should leave out this segment because they should not be connected
	if(v0.age <= v1.age)
	{
		return;
	//	v1 = v0;//g_characteristicLineBuffer[lineId0 * g_clLength + ((vIdx0 + g_clLength - 1) % g_clLength)];
	}
	
	float4 pos0World = mul(float4(v0.pos, 1), g_modelWorld);	pos0World /= pos0World.w;
	float4 pos1World = mul(float4(v1.pos, 1), g_modelWorld);	pos1World /= pos1World.w;

	float3 pos0Clamp = saturate(v0.pos);
	if(any(pos0Clamp - v0.pos))
	{
		// if both are out of the volume, we don't render anything
		float3 pos1Clamp = saturate(v1.pos);

		if(any(pos1Clamp - v1.pos))
			return;

		//otherwise, clamp the ending position
		pos0World =  mul(float4(pos0Clamp, 1), g_modelWorld);	pos0World /= pos1World.w;
	}

	ExtrudePillContainerBox(pos0World.xyz, pos1World.xyz, v0.color.rgb, v1.color.rgb, g_clWidth, stream);
}



/////////////////////////////////////////////////////////////////
//					Surface Rendering
/////////////////////////////////////////////////////////////////

#define ALPHA_DENSITY_PER_PIXEL 0
#define SURFACE_CLIPPING_PER_PIXEL 1
#define SURFACE_CLIPPING_PER_VERTEX 0
#define SURFACE_CLIPPING_PER_TRIANGLE 0
#define RENDER_SURFACE_NORMALS 0
#define ENABLE_SURFACE_SHADING 1
#define FLAT_SURFACE_NORMALS 0
#define SURFACE_DEPTH_TEST DepthNoWrite
#define FORCE_SURFACE_WIREFRAME 0

struct GSIn_CharacteristicSurface {
	uint CLVertexID : CL_VERTEX_ID;
};

struct PSIn_CharacteristicSurface {
	float4 pos : SV_POSITION;
	float4 worldPos : WORLD_POSITION;
#if SURFACE_CLIPPING_PER_PIXEL
	float3 volPos : VOLUME_POSITION;
#endif
	float4 col : COLOR;
#if FLAT_SURFACE_NORMALS
	nointerpolation float3 nor : WORLD_NORMAL;
#else
	 float3 nor : WORLD_NORMAL;
#endif
#if ALPHA_DENSITY_PER_PIXEL
	float invMaxArea : INV_MAX_AREA;
#endif
};

//buffer containing normal and alpha value for each triangle of a streak/stream surface
//dimensions shoud be: (2 * g_clLength) x (g_numParticles - 1)
struct TriangleProperties {
	float3 normal;
	float3 center;
	float area;
	float alpha;
};
StructuredBuffer<TriangleProperties> g_triangleProperties;
RWStructuredBuffer<TriangleProperties> g_trianglePropertiesRW;

GSIn_CharacteristicSurface vsSurface(uint vertexID : SV_VertexID)
{
	GSIn_CharacteristicSurface gsIn;
	gsIn.CLVertexID = vertexID;
	return gsIn;
}

[maxvertexcount(6)] 
void gsExtrudeSurfaceQuad(point GSIn_CharacteristicSurface gsIn[1], inout TriangleStream<PSIn_CharacteristicSurface> stream) {
	uint lineId = gsIn[0].CLVertexID / g_clLength;
	uint vIdx1 = gsIn[0].CLVertexID % g_clLength;
	uint clVertexIndex0 = lineId * g_clLength +  (vIdx1 % g_clLength);
	uint clVertexIndex1 = lineId * g_clLength + ((vIdx1+1) % g_clLength);
	uint clVertexIndex2 = (lineId+1) * g_clLength + (vIdx1 % g_clLength);
	uint clVertexIndex3 = (lineId+1) * g_clLength + ((vIdx1 + 1) % g_clLength);

	CLVertex v0 = g_characteristicLineBuffer[clVertexIndex0];
	CLVertex v1 = g_characteristicLineBuffer[clVertexIndex1];
	
	if(g_enableTimeSurfaces) {
		if(!((vIdx1+1) % (g_clLength/g_numTimeSurfaces)))
			return;
	}

	if(v0.age < v1.age)
		return;
	
	CLVertex v2 = g_characteristicLineBuffer[clVertexIndex2];
	CLVertex v3 = g_characteristicLineBuffer[clVertexIndex3];


	PSIn_CharacteristicSurface cr0, cr1, cr2, cr3;

	cr0.col = v0.color;
	cr0.pos = mul(float4(v0.pos, 1), g_modelWorldViewProj);
	cr0.worldPos = mul(float4(v0.pos, 1), g_modelWorld);
	cr0.worldPos /= cr0.worldPos.w;
	cr0.nor = v0.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr0.invMaxArea = v0.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr0.volPos = v0.pos;
#endif
#if SURFACE_CLIPPING_PER_TRIANGLE
	if(!any(v0.pos - saturate(v0.pos)))
#endif
	stream.Append(cr0);

	cr1.col = v1.color;
	cr1.pos = mul(float4(v1.pos, 1), g_modelWorldViewProj);
	cr1.worldPos = mul(float4(v1.pos, 1), g_modelWorld);
	cr1.worldPos /= cr1.worldPos.w;
	cr1.nor = v1.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr1.invMaxArea = v1.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr1.volPos = v1.pos;
#endif
#if SURFACE_CLIPPING_PER_TRIANGLE
	if(!any(v1.pos - saturate(v1.pos)))
#endif
	stream.Append(cr1);

	cr2.col = v2.color;
	cr2.pos = mul(float4(v2.pos, 1), g_modelWorldViewProj);
	cr2.worldPos = mul(float4(v2.pos, 1), g_modelWorld);
	cr2.worldPos /= cr2.worldPos.w;
	cr2.nor = v2.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr2.invMaxArea = v2.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr2.volPos = v2.pos;
#endif
#if SURFACE_CLIPPING_PER_TRIANGLE
	if(!any(v2.pos - saturate(v2.pos)))
#endif
	stream.Append(cr2);	

	cr3.col = v3.color;
	cr3.pos = mul(float4(v3.pos, 1), g_modelWorldViewProj);
	cr3.worldPos = mul(float4(v3.pos, 1), g_modelWorld);
	cr3.worldPos /= cr3.worldPos.w;
	cr3.nor = v3.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr3.invMaxArea = v3.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr3.volPos = v3.pos;
#endif
#if SURFACE_CLIPPING_PER_TRIANGLE
	if(!any(v3.pos - saturate(v3.pos)))
#endif
	stream.Append(cr3);
}

[maxvertexcount(7)] 
void gsExtrudeSurfaceWireframe(point GSIn_CharacteristicSurface gsIn[1], inout LineStream<PSIn_CharacteristicSurface> stream) {
	uint lineId = gsIn[0].CLVertexID / g_clLength;
	uint vIdx1 = gsIn[0].CLVertexID % g_clLength;
	uint clVertexIndex0 = lineId * g_clLength +  (vIdx1 % g_clLength);
	uint clVertexIndex1 = lineId * g_clLength + ((vIdx1+1) % g_clLength);
	uint clVertexIndex2 = (lineId+1) * g_clLength + (vIdx1 % g_clLength);
	uint clVertexIndex3 = (lineId+1) * g_clLength + ((vIdx1 + 1) % g_clLength);

	CLVertex v0 = g_characteristicLineBuffer[clVertexIndex0];
	CLVertex v1 = g_characteristicLineBuffer[clVertexIndex1];
	
	if(g_enableTimeSurfaces) {
		if(!((vIdx1+1) % (g_clLength/g_numTimeSurfaces))) 
			return;
	}

	if(v0.age < v1.age)
		return;
	
	CLVertex v2 = g_characteristicLineBuffer[clVertexIndex2];
	CLVertex v3 = g_characteristicLineBuffer[clVertexIndex3];


	PSIn_CharacteristicSurface cr0, cr1, cr2, cr3;

	cr0.col = float4(v0.color.rgb, 1);
	cr0.pos = mul(float4(v0.pos, 1), g_modelWorldViewProj);
	cr0.worldPos = mul(float4(v0.pos, 1), g_modelWorld);
	cr0.worldPos /= cr0.worldPos.w;
	cr0.nor = v0.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr0.invMaxArea = v0.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr0.volPos = v0.pos;
#endif

	cr1.col = float4(v1.color.rgb, 1);
	cr1.pos = mul(float4(v1.pos, 1), g_modelWorldViewProj);
	cr1.worldPos = mul(float4(v1.pos, 1), g_modelWorld);
	cr1.worldPos /= cr1.worldPos.w;
	cr1.nor = v1.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr1.invMaxArea = v1.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr1.volPos = v1.pos;
#endif

	cr2.col = float4(v2.color.rgb, 1);
	cr2.pos = mul(float4(v2.pos, 1), g_modelWorldViewProj);
	cr2.worldPos = mul(float4(v2.pos, 1), g_modelWorld);
	cr2.worldPos /= cr2.worldPos.w;
	cr2.nor = v2.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr2.invMaxArea = v2.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr2.volPos = v2.pos;
#endif

	cr3.col = float4(v3.color.rgb, 1);
	cr3.pos = mul(float4(v3.pos, 1), g_modelWorldViewProj);
	cr3.worldPos = mul(float4(v3.pos, 1), g_modelWorld);
	cr3.worldPos /= cr3.worldPos.w;
	cr3.nor = v3.dir;
#if ALPHA_DENSITY_PER_PIXEL
	cr3.invMaxArea = v3.size;
#endif
#if SURFACE_CLIPPING_PER_PIXEL
	cr3.volPos = v3.pos;
#endif


	stream.Append(cr0);
	stream.Append(cr1);
	stream.Append(cr2);
	stream.Append(cr3);
	stream.Append(cr2);

	stream.RestartStrip();
	stream.Append(cr0);
	stream.Append(cr2);
}

[earlydepthstencil]
void psSurface(PSIn_CharacteristicSurface v) 
{
#if SURFACE_CLIPPING_PER_PIXEL
	if(any(v.volPos - saturate(v.volPos)))
		discard;
#endif

	v.nor = normalize(v.nor);

#if ALPHA_DENSITY_PER_PIXEL
	if(g_clEnableAlphaDensity) {
		float3 dr = normalize(v.worldPos.xyz - g_lightPos);
		v.col.w *= saturate(g_alphaDensityCoefficient * v.invMaxArea / abs(dot(v.nor, dr)));
	}
#endif

#if RENDER_SURFACE_NORMALS
	v.col.xyz = float4(0.5*v.nor + (float3)0.5, 1);
#endif

#if ENABLE_SURFACE_SHADING
	if(g_enableSurfaceLighting) {
		v.col = WorldSpaceLighting(v.worldPos.xyz, v.nor, v.col);
	}
#endif

	AddTransparentFragment(v.pos.xyz, v.col);
//	return (float4)0;
}

[earlydepthstencil]
float4 psSurfaceSolid(PSIn_CharacteristicSurface v) : SV_Target
{
#if SURFACE_CLIPPING_PER_PIXEL
	if(any(v.volPos - saturate(v.volPos)))
		discard;
#endif

#if ALPHA_DENSITY_PER_PIXEL
	v.nor = normalize(v.nor);
	float3 dr = normalize(v.worldPos.xyz - g_lightPos);
	v.col.w *= saturate(g_alphaDensityCoefficient * v.invMaxArea / abs(dot(v.nor, dr)));
#endif

#if RENDER_SURFACE_NORMALS
	v.col.xyz = float4(0.5*v.nor + (float3)0.5, 1);
#endif

#if ENABLE_SURFACE_SHADING
	if(g_enableSurfaceLighting) {
		v.col = WorldSpaceLighting(v.worldPos.xyz, v.nor, v.col);
	}
#endif
	v.col.w = 1.0;

	return v.col;
}

/////////////////////////////////////////////////////////
//		Characteristic Line Compute Shaders
/////////////////////////////////////////////////////////
float3 SampleVectorDeriv(float3 pos, float3 step)
{
	return SampleFlowField(pos + step) - SampleFlowField(pos - step);
}

float3 SampleVorticity(float3 pos)
{
	float3 ux = SampleVectorDeriv(pos, float3(1, 0, 0));
	float3 uy = SampleVectorDeriv(pos, float3(0, 1, 0));
	float3 uz = SampleVectorDeriv(pos, float3(0, 0, 1));
	
	return float3(uz.y - uy.z, ux.z - uz.x, uy.x - ux.y);
}

[numthreads(256,1,1)]	
void CSInitCharacteristicLine(uint3 threadID: SV_DispatchThreadID)
{
	uint pId = threadID.x * g_clLength + threadID.y;
	if(threadID.x >= g_numParticles || threadID.y >= g_clLength)
		return;

	if(g_enableTimeSurfaces) {
		uint numTimeSurfaceLength = g_clLength / g_numTimeSurfaces;
		g_characteristicLineBufferRW[pId].age = -(float)threadID.y * 1e-6 - (threadID.y/numTimeSurfaceLength) * g_clReseedInterval;
		g_characteristicLineBufferRW[pId].pos = ((threadID.y % numTimeSurfaceLength) * g_timeSurfaceLineOffset + g_particleBuffer[threadID.x].seedPos)*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
	}
	else {
		g_characteristicLineBufferRW[pId].age = - (float)threadID.y * g_clReseedInterval;
		g_characteristicLineBufferRW[pId].pos = g_particleBuffer[threadID.x].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
	}
	g_characteristicLineBufferRW[pId].color = g_particleColor;
}

[numthreads(256,1,1)]	
void CSComputeStreamline(uint3 threadID: SV_DispatchThreadID)
{
	uint pId = threadID.x;
	if(pId >= g_numParticles)
		return;

	uint pOffset = (pId+1) * g_clLength - 1;
	float3 pos = g_particleBuffer[pId].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
	g_characteristicLineBufferRW[pOffset].pos = pos;
	g_characteristicLineBufferRW[pOffset].age = 0;
	if(g_colorParticles)
		g_characteristicLineBufferRW[pOffset].color = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, pos, 0), 0);
	else
		g_characteristicLineBufferRW[pOffset].color = g_particleColor;

	for(unsigned int i=1; i < g_clLength; i++) {
		if(all(max(pos, float3(0,0,0))) 
			&& !any(max(pos, float3(1,1,1)) - float3(1,1,1)))
				pos = pos + g_clStepsize * SampleFlowField(pos) * g_velocityScaling;	

		g_characteristicLineBufferRW[pOffset - i].pos = pos;		
		g_characteristicLineBufferRW[pOffset - i].age = g_maxParticleLifetime * (float)i/(float)g_clLength;

		if(g_colorParticles)
			g_characteristicLineBufferRW[pOffset - i].color = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, pos, 0), 0);
		else
			g_characteristicLineBufferRW[pOffset - i].color = g_particleColor;
	}

}

[numthreads(256,1,1)]	
void CSComputeStreamRibbon(uint3 threadID: SV_DispatchThreadID)
{
	uint pId = threadID.x;
	if(pId >= g_numParticles)
		return;

	uint pOffset = pId * g_clLength;
	float3 pos = g_particleBuffer[pId].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
	float3 tangent;
	if(any(g_clRibbonBaseOrientation))
		tangent = g_clRibbonBaseOrientation;
	else {
		//use vorticity
		//tangent = normalize(cross(SampleFlowField(pos), SampleVorticity(pos)));
		tangent = normalize(SampleVorticity(pos));
	}
	g_characteristicLineBufferRW[pOffset].pos = pos;
	g_characteristicLineBufferRW[pOffset].dir = tangent;
	g_characteristicLineBufferRW[pOffset].age = g_clLength;
	if(g_colorParticles)
		g_characteristicLineBufferRW[pOffset].color = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, pos, 0), 0);
	else
		g_characteristicLineBufferRW[pOffset].color = g_particleColor;

	for(unsigned int i=1; i < g_clLength; i++) {
		if(all(max(pos, float3(0,0,0))) 
			&& !any(max(pos, float3(1,1,1)) - float3(1,1,1))) {
				float3 posleft = pos - g_clWidth * tangent;
				float3 posright = pos + g_clWidth * tangent;
				float3 dirleft = g_clStepsize * SampleFlowField(posleft) * g_velocityScaling;
				float3 dirright = g_clStepsize * SampleFlowField(posright) * g_velocityScaling;
				posleft += dirleft;
				posright += dirright;
				float3 dir = 0.5*(posleft + posright) - pos;
				
				pos = 0.5*(posleft + posright);
				tangent = normalize(cross(dir, cross(posright - posleft, dir)));
		}

		g_characteristicLineBufferRW[pOffset + i].pos = pos;		
		g_characteristicLineBufferRW[pOffset + i].dir =  tangent;
		g_characteristicLineBufferRW[pOffset + i].age = g_clLength - i;

		if(g_colorParticles)
			g_characteristicLineBufferRW[pId].color = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, pos, 0), 0);
		else
			g_characteristicLineBufferRW[pId].color = g_particleColor;
	}

}


[numthreads(256,1,1)]	
void CSComputeStreakLine(uint3 threadID: SV_DispatchThreadID)
{
	if(threadID.x >= g_numParticles || threadID.y >= g_clLength)
		return;

	uint pId = threadID.x * g_clLength + threadID.y;

	g_characteristicLineBufferRW[pId].age += g_timeDelta;

	if(g_characteristicLineBufferRW[pId].age > g_maxParticleLifetime) 
	{
		if(g_enableTimeSurfaces) {
			uint numTimeSurfaceLength = g_clLength / g_numTimeSurfaces;
			if(numTimeSurfaceLength > 0)
				g_characteristicLineBufferRW[pId].pos = ((threadID.y%numTimeSurfaceLength) * g_timeSurfaceLineOffset + g_particleBuffer[threadID.x].seedPos)*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
			g_characteristicLineBufferRW[pId].age = fmod(g_characteristicLineBufferRW[pId].age, g_maxParticleLifetime) - g_clReseedInterval;
		}
		else {
			g_characteristicLineBufferRW[pId].pos = g_particleBuffer[threadID.x].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
			g_characteristicLineBufferRW[pId].age = fmod(g_characteristicLineBufferRW[pId].age, g_maxParticleLifetime);
		}
	}


	float advectionDelta = min(g_characteristicLineBufferRW[pId].age, g_timeDelta);
	//only advect if it has positive age
	if(advectionDelta > 0) {
	
		float3 pos = g_characteristicLineBufferRW[pId].pos + advectionDelta * SampleFlowField(g_characteristicLineBufferRW[pId].pos) * g_velocityScaling;
		
		g_characteristicLineBufferRW[pId].pos = pos;
	}

	if(g_colorParticles)
		g_characteristicLineBufferRW[pId].color = g_transferFunction.SampleLevel(
														samLinear, g_scalarVolume.SampleLevel(samLinear, g_characteristicLineBufferRW[pId].pos, 0), 0);
	else
		g_characteristicLineBufferRW[pId].color = g_particleColor;

	
}

[numthreads(256,1,1)]	
void CSComputeStreakRibbon(uint3 threadID: SV_DispatchThreadID)
{
	uint pId = threadID.x * g_clLength + threadID.y;

	if(threadID.x >= g_numParticles || threadID.y >= g_clLength)
		return;

	g_characteristicLineBufferRW[pId].age += g_timeDelta;

	if(g_characteristicLineBufferRW[pId].age > g_maxParticleLifetime) 
	{
		g_characteristicLineBufferRW[pId].pos = g_particleBuffer[threadID.x].seedPos*(g_spawnRegionMax - g_spawnRegionMin) + g_spawnRegionMin;
		g_characteristicLineBufferRW[pId].age = fmod(g_characteristicLineBufferRW[pId].age, g_maxParticleLifetime) - g_clReseedInterval;
		
		//reset tangent
		if(any(g_clRibbonBaseOrientation))			
			g_characteristicLineBufferRW[pId].dir = g_clRibbonBaseOrientation;
		else {
			//use vorticity
			//tangent = normalize(cross(SampleFlowField(pos), SampleVorticity(pos)));
			g_characteristicLineBufferRW[pId].dir = normalize(SampleVorticity(g_characteristicLineBufferRW[pId].pos));
		}
	}


	float advectionDelta = min(g_characteristicLineBufferRW[pId].age, g_timeDelta);
	//only advect if it has positive age
	if(advectionDelta > 0) {

		float3 pos = g_characteristicLineBufferRW[pId].pos;
		float3 tangent = g_characteristicLineBufferRW[pId].dir;

		float3 posleft = pos - g_clWidth * tangent;
		float3 posright = pos + g_clWidth * tangent;
		float3 dirleft = advectionDelta * SampleFlowField(posleft) * g_velocityScaling;
		float3 dirright = advectionDelta * SampleFlowField(posright) * g_velocityScaling;
		posleft += dirleft;
		posright += dirright;
		float3 dir = 0.5*(posleft + posright) - pos;
				
		//average
		pos = 0.5*(posleft + posright);

		g_characteristicLineBufferRW[pId].pos = pos;
		g_characteristicLineBufferRW[pId].dir = normalize(cross(dir, cross(posright - posleft, dir)));

		if(g_colorParticles)
			g_characteristicLineBufferRW[pId].color = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, pos, 0), 0);
		else
			g_characteristicLineBufferRW[pId].color = g_particleColor;
	}
}


//////////////////////////////////////////////////////////
//		Characteristic Surface Compute Shaders
//////////////////////////////////////////////////////////

//Particle Position in View space
float3 ppos(uint id) {
	float4 p = mul(float4(g_characteristicLineBuffer[id].pos, 1), g_modelWorld);
	return p.xyz/p.w;
};

[numthreads(32,32,1)]	
void CSComputeTriangleProperties(uint3 threadID: SV_DispatchThreadID)
{
	uint numVertices = g_numParticles * g_clLength;
	//x is vertex index, y is line index
	if(threadID.y >= g_numParticles || threadID.x >= g_clLength*2)
		return;

	uint triID = threadID.y * 2 * g_clLength + threadID.x;

	//the three particles defining the triangle
	uint pID0, pID1, pID2;
	if(threadID.x % 2) {
		pID0 = (threadID.y + 1) * g_clLength + threadID.x/2;
		pID1 = (threadID.y) * g_clLength + (threadID.x/2 +1) % g_clLength;
		pID2 = (threadID.y + 1) * g_clLength + (threadID.x/2 +1) % g_clLength;
	}
	else {
		pID0 = (threadID.y) * g_clLength + threadID.x/2;
		pID1 = (threadID.y) * g_clLength + (threadID.x/2 +1) % g_clLength;
		pID2 = (threadID.y + 1) * g_clLength + threadID.x/2;
	}

	// these are in view space
	float3 p0 = ppos(pID0);
	float3 p1 = ppos(pID1);
	float3 p2 = ppos(pID2);

	//the three sides of the triangle opposed to the corresp. points
	float3 s0 = p2 - p1;
	float3 s1 = p2 - p0;
	float3 s2 = p1 - p0;

	float l0 = length(s0);
	float l1 = length(s1);
	float l2 = length(s2);

	//normal is now in world space
	float3 normal = (l1 > 0 && l2 > 0) ? normalize(cross(s2, s1)) : (float3)0;

	//area computed after Heron's formula
	float s = (l0 + l1 + l2)/2.; //semiperimeter
	float h = s * (s - l0) * (s - l1) * (s - l2);
	float area = sqrt(h);

	float3 center = (p0+p1+p2)/3.;

	float alpha = 1.;

#if ! ALPHA_DENSITY_PER_PIXEL
	//alpha density
	if(g_clEnableAlphaDensity) {
		float3 dr = normalize(center - g_lightPos);
		float alphaDensity = (area>0) ? saturate( g_alphaDensityCoefficient /(area * abs(dot(normal, dr)))) : 1;
		alpha *= alphaDensity;
	}
#endif

	static const float four_div_sqrt_three = 2.309401076758503058;
	if(g_clEnableAlphaShape) {
		float maxEdgeProd = max(max(l0*l1, l1*l2), l2*l0);
		float alphaShape = pow(four_div_sqrt_three * area / maxEdgeProd, g_alphaShapeCoefficient);
		alpha *= alphaShape;
	}

	g_trianglePropertiesRW[triID].center = center;
	g_trianglePropertiesRW[triID].area = area;
	g_trianglePropertiesRW[triID].normal = normal;
	g_trianglePropertiesRW[triID].alpha = alpha;

}

float HelperCurvatureDotProd(uint otherVertexIndex, float3 worldPos, float3 normal)
{
	float4 otherWorldPos = mul(float4(g_characteristicLineBufferRW[otherVertexIndex].pos, 1), g_modelWorld);
	return abs(dot(normal, normalize(otherWorldPos.xyz/otherWorldPos.w - worldPos)));
}

[numthreads(32,32,1)]	
void CSComputeSurfaceVertexMetrics(uint3 threadID: SV_DispatchThreadID)
{
	//x is vertex index, y is line index
	if(threadID.x >= g_clLength || threadID.y >= g_numParticles)
		return;

	uint pID = threadID.y * g_clLength + threadID.x;

#if SURFACE_CLIPPING_PER_VERTEX
	//if out of volume, just make it transparent
	if(any(g_characteristicLineBufferRW[pID].pos - saturate(g_characteristicLineBufferRW[pID].pos))) {	
		g_characteristicLineBufferRW[pID].color.w = 0.0;
		return;
	}
#endif

	uint clVertexIndexForward = threadID.y * g_clLength + ((threadID.x+1) % g_clLength);
	uint clVertexIndexBackward = threadID.y * g_clLength + ((threadID.x-1 + g_clLength) % g_clLength);
	bool forwardConnected = 	(g_characteristicLineBufferRW[pID].age >= g_characteristicLineBufferRW[clVertexIndexForward].age);
	bool backwardConnected = 	(g_characteristicLineBufferRW[clVertexIndexBackward].age >= g_characteristicLineBufferRW[pID].age);

	float3 normal = float3(0,0,0);
	float k = .0000001;
	float triangleAlpha = 1.;
	int numAdjacentTriangles = 0;

#if ALPHA_DENSITY_PER_PIXEL
	float maxArea = 0;
#endif

	// we work clockwise around the (max.) 6 adjacent triangles, starting with the "northern" triangle
	if(forwardConnected && threadID.y > 0) 
	{
		int triID0 = (threadID.y - 1) * 2*g_clLength + 2*threadID.x;
		normal += g_triangleProperties[triID0].area*g_triangleProperties[triID0].normal;
		triangleAlpha = min(triangleAlpha, g_triangleProperties[triID0].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID0].area);
#endif

		int triID1 = (threadID.y - 1) * 2*g_clLength + (2*threadID.x + 1)%(2*g_clLength);
		normal += g_triangleProperties[triID1].area*g_triangleProperties[triID1].normal;
		triangleAlpha = min(triangleAlpha, g_triangleProperties[triID1].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID1].area);
#endif

		numAdjacentTriangles += 2;
	}

	if(forwardConnected && (threadID.y < (g_numParticles-1)) )
	{
		int triID2 = (threadID.y) * 2*g_clLength + 2*threadID.x;
		normal += g_triangleProperties[triID2].area * g_triangleProperties[triID2].normal;
		triangleAlpha = min(triangleAlpha, g_triangleProperties[triID2].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID2].area);
#endif
		numAdjacentTriangles += 1;
	}

	if(backwardConnected && (threadID.y < (g_numParticles-1))) 
	{
		int triID3 = (threadID.y) * 2*g_clLength + (2*threadID.x - 1 + 2*g_clLength)%(2*g_clLength);
		normal += g_triangleProperties[triID3].area*g_triangleProperties[triID3].normal;
		triangleAlpha = min(triangleAlpha, g_triangleProperties[triID3].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID3].area);
#endif

		int triID4 = (threadID.y) * 2*g_clLength + (2*threadID.x - 2 + 2*g_clLength)%(2*g_clLength);
		normal += g_triangleProperties[triID4].area * g_triangleProperties[triID4].normal;
		triangleAlpha = min(triangleAlpha,g_triangleProperties[triID4].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID4].area);
#endif

		numAdjacentTriangles += 2;
	}

	if(backwardConnected && threadID.y > 0) 
	{
		int triID5 = (threadID.y - 1) * 2*g_clLength + (2*threadID.x - 1 + 2*g_clLength)%(2*g_clLength);
		normal += g_triangleProperties[triID5].area * g_triangleProperties[triID5].normal;
		//triangleAlpha = min(triangleAlpha, g_triangleProperties[triID5].alpha);
#if ALPHA_DENSITY_PER_PIXEL
		maxArea = max(maxArea, g_triangleProperties[triID5].area);
#endif

		numAdjacentTriangles += 1;
	}

	normal = normalize(normal);
	
	//if(!forwardConnected)		g_characteristicLineBufferRW[pID].color = float4(1,0,0, 1);
	//else	if(!backwardConnected)		g_characteristicLineBufferRW[pID].color = float4(0,1,0, 1);
#if PARTICLE_SHADER_DEBUG
	switch(numAdjacentTriangles) {
	case 0:g_characteristicLineBufferRW[pID].color = float4(0,0,0,1);break;
	case 1:g_characteristicLineBufferRW[pID].color = float4(1,0,0,1);break;
	case 2:g_characteristicLineBufferRW[pID].color = float4(1,1,0,1);break;
	case 3:g_characteristicLineBufferRW[pID].color = float4(0,1,0,1);break;
	case 4:g_characteristicLineBufferRW[pID].color = float4(0,1,1,1);break;
	case 5:g_characteristicLineBufferRW[pID].color = float4(0,0,1,1);break;
	case 6:g_characteristicLineBufferRW[pID].color = float4(1,0,1,1);break;
	}
#endif

	float vertexAlpha = 1.0;
	//Vertex Alpha Modifiers
	if(g_clEnableAlphaFade) {
		vertexAlpha *= 1. - g_characteristicLineBufferRW[pID].age / g_maxParticleLifetime;
	}
	if(g_clEnableAlphaCurvature) {
		float4 worldPos_w = mul(float4(g_characteristicLineBufferRW[pID].pos, 1), g_modelWorld);
		float3 worldPos = worldPos_w.xyz/worldPos_w.w;
		float maxDotProd = 0;
		if(forwardConnected)			maxDotProd = max(maxDotProd, HelperCurvatureDotProd(clVertexIndexForward, worldPos, normal));
		if(backwardConnected)			maxDotProd = max(maxDotProd, HelperCurvatureDotProd(clVertexIndexBackward, worldPos, normal));
		if(threadID.y > 0)				maxDotProd = max(maxDotProd, HelperCurvatureDotProd(pID - g_clLength, worldPos, normal));
		if(threadID.y < g_numParticles - 1) maxDotProd = max(maxDotProd, HelperCurvatureDotProd(pID + g_clLength, worldPos, normal));
		vertexAlpha *= saturate(1. - g_alphaCurvatureCoefficient * maxDotProd);
		//	g_characteristicLineBufferRW[pID].color = float4(saturate(1. - g_alphaCurvatureCoefficient * maxDotProd), 0, 0, 1);
	}

	//g_characteristicLineBufferRW[pID].color.xyz = float3((float)threadID.y/(float)g_numParticles, (float)threadID.x/(float)g_clLength, 0);

#if ALPHA_DENSITY_PER_PIXEL
	g_characteristicLineBufferRW[pID].size = 1./maxArea;
#endif

	g_characteristicLineBufferRW[pID].dir = normal;
	g_characteristicLineBufferRW[pID].color.w = triangleAlpha*vertexAlpha;
}

technique11 ParticleTracer
{
	pass PASS_RENDER
	{
		SetVertexShader(CompileShader(vs_5_0, vsParticle()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSprite()));
		SetPixelShader(CompileShader(ps_5_0, psRTSpherePerspective()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_RENDER_COLORED
	{
		SetVertexShader(CompileShader(vs_5_0, vsParticleColored()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSprite()));
		SetPixelShader(CompileShader(ps_5_0, psRTSpherePerspective()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_RENDER_CL_LINE
	{
		SetVertexShader(CompileShader(vs_5_0, vsCharacteristicLine()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psCharacteristicLine()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_RENDER_CL_RIBBON
	{
		SetVertexShader(CompileShader(vs_5_0, vsCharacteristicRibbon()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeRibbon()) );
		SetPixelShader(CompileShader(ps_5_0, psCharacteristicRibbon()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}


	pass PASS_RENDER_TUBE_CORNERS
	{
		SetVertexShader(CompileShader(vs_5_0, vsTubeEndCap()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSprite()));
		SetPixelShader(CompileShader(ps_5_0, psRTSpherePerspective()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_RENDER_TUBE_CYLINDERS
	{
		SetVertexShader(CompileShader(vs_5_0, vsCharacteristicRibbon()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeBBox()));
		SetPixelShader(CompileShader(ps_5_0, psRTPill()));
		SetRasterizerState(CullBack);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_RENDER_SURFACE
	{
		SetVertexShader(CompileShader(vs_5_0, vsSurface()));
#if FORCE_SURFACE_WIREFRAME
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSurfaceWireframe()));
#else
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSurfaceQuad()));
#endif
		SetPixelShader(CompileShader(ps_5_0, psSurface()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(SURFACE_DEPTH_TEST, 0);
		SetBlendState(BlendDisableAll, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_RENDER_SURFACE_WIREFRAME
	{
		SetVertexShader(CompileShader(vs_5_0, vsSurface()));
		SetGeometryShader(CompileShader(gs_5_0, gsExtrudeSurfaceWireframe()));
		SetPixelShader(CompileShader(ps_5_0, psSurfaceSolid()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_ADVECT
	{
		SetComputeShader(CompileShader(cs_5_0, CSAdvect()));
	}
	pass PASS_COMPUTE_STREAMLINE
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeStreamline()));
	}
	pass PASS_COMPUTE_STREAMRIBBON
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeStreamRibbon()));
	}
	pass PASS_INIT_CHARACTERISTIC_LINE
	{
		SetComputeShader(CompileShader(cs_5_0, CSInitCharacteristicLine()));
	}
	pass PASS_COMPUTE_STREAKLINE
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeStreakLine()));
	}
	pass PASS_COMPUTE_STREAKRIBBON
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeStreakRibbon()));
	}
	pass PASS_COMPUTE_TRIANGLE_PROPERTIES
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeTriangleProperties()));
	}
	pass PASS_COMPUTE_SURFACE_VERTEX_METRICS
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeSurfaceVertexMetrics()));
	}
}
