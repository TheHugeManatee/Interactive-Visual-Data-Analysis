//--------------------------------------------------------------------------------------
// File: SimpleMesh.fx
//
// Shader Effect for rendering of the SimpleMesh class 
//
// Note: All fx-files in the project are automatically compiled by Visual Studio
//       to fxo-files in the output directories "x64\Debug\" and "x64\Release\"
//       (see "HLSL Compiler" in the project properties).
//--------------------------------------------------------------------------------------

#include "common.hlsli"
#include "TransparencyModuleInterface.hlsli"

cbuffer cbRayCaster
{
	float4x4	g_worldViewProj;
	float4x4	g_worldViewProjInv;

	float		g_isoValue;
	float		g_isoValue2;	//isovalue for second iso surface
	float		g_raycastStepsize;
	float		g_raycastPixelStepsize;
	int			g_binSearchSteps;
	int			g_maxAOSteps = 5;
	float3		g_camPosInObjectSpace;
	bool		g_DVRLighting = false;
	float4		g_surfaceColor;
	float4		g_surfaceColor2;	//color for second iso surface
	float		g_terminationAlphaThreshold = 1;
	float		g_globalAlphaScale = 1.0;
};

Texture3D<float> g_texVolume;
Texture3D<float3> g_texVolumeNormals;
Texture2D<float> g_depthBuffer;
Texture2D<float3> g_rayEntryPoints;
Texture1D<float4> g_transferFunction;

struct SimpleVertex
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSBoxIn
{
	float4 pos   : SV_Position;
	float4 color : COLOR;
	float4 tex : TEXTURE;
	float4 ndcPos : NDC_POS;
};


// samples the volue dataset 
// either directly if we are not time dependent, or
// interpolating between the two textures
float SampleVolume(float3 p, int3 offset) {
	return g_texVolume.SampleLevel(samLinear, p, 0.0, offset);
}
float SampleVolume(float3 p) {
	return g_texVolume.SampleLevel(samLinear, p, 0.0);
}
float3 SampleVolumeNormals(float3 p) {
	return normalize(g_texVolumeNormals.SampleLevel(samLinear, p, 0.0));
}


void vsBox(float4 v : POSITION, out PSBoxIn output)
{
	output.tex = v;
	output.color = g_surfaceColor;

	// transform vertex position into clip space
	output.pos = mul(v, g_worldViewProj);
	output.ndcPos = output.pos;
}

//A simple pixel shader that returns the input vertex color (interpolated output of vertex shader)
float4 psSimple(PSBoxIn input) : SV_Target
{
	
	return input.tex;
}

float4 psSample(PSBoxIn input) : SV_Target
{
	float val = SampleVolume(input.tex.xyz);
	return (float4)val;
}


struct PSIn_EntryPoint {
	float4 pos : SV_POSITION;
	float3 tex : VOL_COORD;
};
PSIn_EntryPoint vsEntryPoint(float4 v : POSITION)
{
	PSIn_EntryPoint pse;
	pse.tex = v.xyz;
	pse.pos = mul(v, g_worldViewProj);
	return pse;
}
float4 psEntryPoint(PSIn_EntryPoint pse) : SV_Target
{
	return float4(pse.tex, 1);
}

// determines the entry point for a ray in direction exiting a unit box at
// exitPoint with direction dir
// returned value is float2(t_entry, t_exit)
// discards fragments where no intersection can be determined
float2 getEntry(float3 exitPoint, float3 dir)
{
	float3 org = g_camPosInObjectSpace;

	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	if (dir.x >= 0) {
		tmin = ( - org.x) / dir.x;
		tmax = (1. - org.x) / dir.x;
	}
	else {
		tmin = (1. - org.x) / dir.x;
		tmax = ( - org.x) / dir.x;
	}
	if (dir.y >= 0.) {
		tymin = ( - org.y) / dir.y;
		tymax = (1. - org.y) / dir.y;
	}
	else {
		tymin = (1. - org.y) / dir.y;
		tymax = ( - org.y) / dir.y;
	}
	//if ( (tmin > tymax) || (tymin > tmax) )
	//	discard;

	if (tymin > tmin)		tmin = tymin;
	if (tymax < tmax)		tmax = tymax;

	if (dir.z >= 0.) {
		tzmin = ( - org.z) / dir.z;
		tzmax = (1. - org.z) / dir.z;
	}
	else {
		tzmin = (1. - org.z) / dir.z;
		tzmax = ( - org.z) / dir.z;
	}
	//if ( (tmin > tzmax) || (tzmin > tmax) )
	//	discard;

	if (tzmin > tmin)		tmin = tzmin;
	if (tzmax < tmax)		tmax = tzmax;
	
	return float2(tmin, tmax);
}

// marches through the volume and finds the first intersection of the ray with the iso surface
// inside is used to control whether to look for a hit from inside the surface (outside = -1) or outside (outside = 1)
// returns the position in ret.xyz and the ray distance t in ret.w
// returns (0, 0, 0, tMax + 1) if no intersection was found
float4 findFirstHit(float3 org, float3 dir, float tstart, float tmax, float outside)
{
	float t = tstart;
	
	while(t < tmax) {
		float tnew = t + g_raycastStepsize;
		float3 pos = org + tnew * dir;

		float sample = SampleVolume(pos);
		if(outside * sample > outside * g_isoValue)
		{
			// binary search to refine the intersection point
			float t1 = t, t2 = tnew;
			for(int i = 0; i < g_binSearchSteps; i++)
			{
				float3 p = org + (t1 + t2)/2 * dir;
				float s = SampleVolume(p);
				if(outside * s < outside * g_isoValue) {
					t1 = (t1 + t2)/2;
					pos = org + t1 * dir;
				}
				else {
					t2 = (t1 + t2)/2;
					pos = org + t2 * dir;
				}
			}
			return float4(pos, t);
		}
			
		t = tnew;
	}
	return float4(0, 0, 0, tmax + 1);
}

float4 findFirstHitSurface(float3 org, float3 dir, float tstart, float tmax, float outside, float surface) {
	float t = tstart;
	
	while(t < tmax) {
		float tnew = t + g_raycastStepsize;
		float3 pos = org + tnew * dir;
		float sample = SampleVolume(pos);
		if(outside * sample > outside * surface)	{
			// binary search to refine the intersection point
			float t1 = t, t2 = tnew;
			for(int i = 0; i < g_binSearchSteps; i++) 	{
				float3 p = org + (t1 + t2)/2 * dir;
				float s = SampleVolume(p);
				if(outside * s < outside * surface) {
					t1 = (t1 + t2)/2;
					pos = org + t1 * dir;
				}
				else {
					t2 = (t1 + t2)/2;
					pos = org + t2 * dir;
				}
			}
			return float4(pos, t);
		}
			
		t = tnew;
	}
	return float4(0, 0, 0, tmax + 1);
}

// left in here for reference, not really needed anymore...
float4 psBasicRaycastBox(PSBoxIn input) : SV_Target
{
	float3 org = g_camPosInObjectSpace;
	float3 dir = normalize(input.tex.xyz - org);
	float2 entryExit = getEntry(input.tex.xyz, dir);
	return float4(org + entryExit.x * dir, 1);
}

// performs volume lighting in volume texture space
// in this case, g_lightPos is assumed to be in volume texture space
// this actually produces wrong results for non-cubic volume sizes but
// the performance penalty of transforming every voxel to view space would be far worse
float3 volumeLighting(float3 surfaceColor, float3 pos) {
	//compute gradient/normal vector
	float3 n = SampleVolumeNormals(pos);

	//lighting
	float3 l = normalize(g_lightPos - pos);
	float3 v = l;						//light direction and viewing direction are the same for our "headlight"
	float3 rl = reflect(-l, n);

	//return (float4)0.5 + float4(0.5 * n, 0.5);
	//return (float4)0.5 + float4(0.5 * l, 0.5);

	float3 ambient = surfaceColor;
	float dotNL = saturate(dot(n, l));
	float3 diffuse = g_lightColor * surfaceColor * dotNL;
	float3 specular = (float3)0;
	if(dotNL > 0)
		specular = g_lightColor * pow(saturate(dot(rl, v)), g_specularExp);


	float3 c = (k_s * specular) + (k_d * diffuse) + k_a * ambient;
	return c;
}

void psIsoSurface(PSBoxIn input, out float4 targetColor : SV_Target, out float targetDepth : SV_Depth)
{

	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);

	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);

	
	float outside = SampleVolume(org) < g_isoValue ? 1 : -1;
	float4 firstHit = findFirstHit(org, dir, 0, tMax, outside);

	// firstHit returns w > tMax when no intersection is found
	if(firstHit.w > tMax)
		discard;

	targetColor = float4(volumeLighting(input.color.xyz, firstHit.xyz), 1);
	float4 surfaceInClip = mul(float4(firstHit.xyz, 1), g_worldViewProj);
	targetDepth = surfaceInClip.z / surfaceInClip.w;
}

float4 psIsoSurfaceAlpha(PSBoxIn input) : SV_Target
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);

	float3 c_acc_premult = float3(0, 0, 0);
	float alpha_acc = 0;
	
	float outside = SampleVolume(org) < g_isoValue ? 1 : -1; // we assume we start outside
	float currentT = 0;
	float4 surfaceHit = findFirstHit(org, dir, currentT, tMax, outside);
	
	//while we are still inside the volume, find next isosurface intersection
	while(surfaceHit.w < tMax) {

		float4 C_a = float4(volumeLighting(input.color.rgb, surfaceHit.xyz), input.color.a);
		
		// front to back alpha blending
		float oneminusalpha = (1. - alpha_acc);
		c_acc_premult =  c_acc_premult + oneminusalpha * C_a.a * C_a.rgb;
		alpha_acc = alpha_acc + oneminusalpha * C_a.a;

		//early ray termination
		if(alpha_acc >= g_terminationAlphaThreshold) {
			alpha_acc = 1.;
			break;
		}

		//now we're on the other side of the surface
		outside = outside * -1;
		currentT = surfaceHit.w + g_raycastStepsize;
		surfaceHit = findFirstHit(org, dir, currentT, tMax, outside);
	}

	return float4(c_acc_premult, alpha_acc);
}

void psIsoSurfaceAlphaGlobal(PSBoxIn input)
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);
	
	float4 entryPos = mul(float4(org, 1), g_worldViewProj);
	entryPos /= entryPos.w;

	float depthStep = (input.pos.z - entryPos.z)/(rayLength);
		
	float outside = SampleVolume(org) < g_isoValue ? 1 : -1;
	float currentT = 0;
	float4 surfaceHit = findFirstHit(org, dir, currentT, tMax, outside);

	//while we are still inside the volume, find next isosurface intersection
	while(surfaceHit.w < tMax) {

		float4 C_a = float4(volumeLighting(input.color.rgb, surfaceHit.xyz), input.color.a);
		
		float4 hitScreenPos = mul(float4(surfaceHit.xyz, 1), g_worldViewProj);
		float d = hitScreenPos.z/hitScreenPos.w;
		AddTransparentFragment(float3(input.pos.xy, d), C_a);

		//now we're on the other side of the surface
		outside = outside * -1;
		currentT = surfaceHit.w + g_raycastStepsize;
		surfaceHit = findFirstHit(org, dir, currentT, tMax, outside);
	}
}

float4 psDualIsoSurface(PSBoxIn input) : SV_Target
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);
	

	float3 c_acc_premult = float3(0, 0, 0);
	float alpha_acc = 0;
	
	float v = SampleVolume(org);
	float outside1 = v < g_isoValue ? 1 : -1;
	float outside2 = v < g_isoValue2 ? 1 : -1;
	float currentT1 = 0;
	float currentT2 = 0;
	float4 surfaceHit1 = findFirstHitSurface(org, dir, currentT1, tMax, outside1, g_isoValue);
	float4 surfaceHit2 = findFirstHitSurface(org, dir, currentT2, tMax, outside2, g_isoValue2);
	
	//while we are still inside the volume, find next isosurface intersection
	while(surfaceHit1.w <= tMax || surfaceHit2.w <= tMax) {
		if(surfaceHit1.w < surfaceHit2.w) {
			float4 C_a = float4(volumeLighting(g_surfaceColor.rgb, surfaceHit1.xyz), g_surfaceColor.a);
		
			// front to back alpha blending
			float oneminusalpha = (1. - alpha_acc);
			c_acc_premult =  c_acc_premult + oneminusalpha * C_a.a * C_a.rgb;
			alpha_acc = alpha_acc + oneminusalpha * C_a.a;

			//early ray termination
			if(alpha_acc >= g_terminationAlphaThreshold) {
				alpha_acc = 1.;
				break;
			}

			//now we're on the other side of the surface
			outside1 = outside1 * -1;
			currentT1 = surfaceHit1.w + g_raycastStepsize;
			surfaceHit1 = findFirstHitSurface(org, dir, currentT1, tMax, outside1, g_isoValue);
		}
		else {
			float4 C_a = float4(volumeLighting(g_surfaceColor2.rgb, surfaceHit2.xyz), g_surfaceColor2.a);
		
			// front to back alpha blending
			float oneminusalpha = (1. - alpha_acc);
			c_acc_premult =  c_acc_premult + oneminusalpha * C_a.a * C_a.rgb;
			alpha_acc = alpha_acc + oneminusalpha * C_a.a;

			//early ray termination
			if(alpha_acc >= g_terminationAlphaThreshold) {
				alpha_acc = 1.;
				break;
			}

			//now we're on the other side of the surface
			outside2 = outside2 * -1;
			currentT2 = surfaceHit2.w + g_raycastStepsize;
			surfaceHit2 = findFirstHitSurface(org, dir, currentT2, tMax, outside2, g_isoValue2);
		}
	}

	return float4(c_acc_premult, alpha_acc);
}

float4 psDVR(PSBoxIn input) : SV_Target
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction
	//float2 entryExit = float2(0, rayLength);//getEntry(input.tex.xyz, dir);
	

	// Integrate the depth buffer data to cut off the raycasting
	// we first transform the position of the depth buffer into the model coordinates
	// then we project this position onto the ray to get the correspondig ray parameter t for the position
	// this position is used as the maximal ray distance from the origin
	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);
	
	// for box background, this debug return should give the same output as the back face shader
	// except if there is other geometry, it should give the xyz-colors of the texture coordinates of the surface in the cube
	//return float4(org + tMax * dir, 1); 
	
	//trace the ray through the volume
	float3 c_acc_premult = float3(0, 0, 0);
	float alpha_acc = 0;
	for(float t = 0; t < tMax; t += g_raycastStepsize) {
		float3 pos = org + t * dir;
		float s = SampleVolume(pos);
		float4 C_a = g_transferFunction.SampleLevel(samLinear, s, 0.0);
		
		//lighting
		if(g_DVRLighting && C_a.w > 0)
			C_a.xyz = volumeLighting(C_a.xyz, pos);
		

		C_a.w *= g_globalAlphaScale * g_raycastPixelStepsize;
		float oneminusalpha = (1. - alpha_acc);
		c_acc_premult =  c_acc_premult + oneminusalpha * C_a.w * C_a.xyz;
		alpha_acc = alpha_acc + oneminusalpha * C_a.w;


		//early ray termination
		if(alpha_acc >= g_terminationAlphaThreshold) {
			alpha_acc = 1.;
			break;
		}
	}

	return float4(c_acc_premult, alpha_acc);
}


/*
	Maximum intensity projection shader using texture value as intensity criterium
*/
float4 psMIP(PSBoxIn input) : SV_Target
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);
	
	float maximum = 0;
	float3 maxPos = org;
	for(float t = 0; t < tMax; t += g_raycastStepsize) {
		float3 pos = org + t * dir;
		float s = SampleVolume(pos);
		if(maximum < s) {
			maximum = s;
			maxPos = pos;
		}
	}
	
	float4 C_a = g_transferFunction.SampleLevel(samLinear, maximum, 0.0);

	return float4(C_a.xyz * C_a.w, C_a.w);
}

/*
	Maximum intensity projection shader using transparency as intensity projection criterium
*/
float4 psMIP2(PSBoxIn input) : SV_Target
{
	float3 org = g_rayEntryPoints[uint2(input.pos.xy)];//g_camPosInObjectSpace;
	float3 dir = input.tex.xyz - org;
	float rayLength = length(dir);
	dir /= rayLength; //normalize direction

	float z = g_depthBuffer[uint2(input.pos.xy)];
	input.ndcPos /= input.ndcPos.w;
	float4 depthPos = mul(float4(input.ndcPos.x, input.ndcPos.y, z, 1), g_worldViewProjInv);
	//return depthPos;
	depthPos /= depthPos.w;	//homogenization
	float tDepth = dot(depthPos.xyz - org, dir);
	float tMax = min(rayLength, tDepth);
	
	float maximum = 0;
	float4 C_max = float4(0,0,0,0);
	for(float t = 0; t < tMax; t += g_raycastStepsize) {
		float3 pos = org + t * dir;
		float s = SampleVolume(pos);
		float4 C_a = g_transferFunction.SampleLevel(samLinear, s, 0.0);
		if(maximum < C_a.w) {
			maximum = C_a.w;
			C_max = C_a;
		}
	}

	return float4(C_max.xyz * C_max.w, C_max.w);
}

// Simple technique (a technique is a collection of passes)
technique11 RayCaster
{
	pass PASS_BOX
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimple()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_BOX_FRONT
	{
		SetVertexShader(CompileShader(vs_5_0, vsEntryPoint()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psEntryPoint()));
		SetRasterizerState(CullBack);
		SetDepthStencilState(DepthDisable, 0);//Disable depth test and write because we render to secondary texture
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_BOX_BACK
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimple()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_BOX_RAYCAST
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psBasicRaycastBox()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_ISOSURFACE
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psIsoSurface()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_ISOSURFACE_ALPHA
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psIsoSurfaceAlpha()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_ISOSURFACE_ALPHA_GLOBAL
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psIsoSurfaceAlphaGlobal()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisableAll, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_DUAL_ISOSURFACE
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psDualIsoSurface()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_DVR
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psDVR()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_MIP
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psMIP()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass PASS_MIP2
	{
		SetVertexShader(CompileShader(vs_5_0, vsBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psMIP2()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

}
