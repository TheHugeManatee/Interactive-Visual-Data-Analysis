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

cbuffer cbGlyphVisualizer
{
	float4x4	g_worldViewProj;
	float		g_timestepT;
	float3		g_camPos; //in world space
	uint3		g_numGlyphs;	// number of glyphs in all directions (equal to volume resolution in normal case, )
	float3		g_sliceCenter;	// the center of the slice in tex coordinates
	float3		g_glyphSpacing;	// glyph spacing in tex coordinates for each direction (normally 1/res in all directions)
	float		g_glyphSize;
};

Texture3D<float3> g_flowFieldTex0;
Texture3D<float3> g_flowFieldTex1;

struct glyphSpec {
	float4 pos: GLYPH_POS;
	float3 dir: DIR;
	float4 col: COLOR;
};

struct PSVertex {
	float4 pos: SV_POSITION;
	float4 col: COLOR;
	float3 nor: NORMAL;
	float3 oPos: OBJECT_POS;
};

// samples the volue dataset 
// either directly if we are not time dependent, or
// interpolating between the two textures
float3 SampleFlowField(float3 p) {
	if(g_timestepT == 0)
		return g_flowFieldTex0.SampleLevel(samLinear, p, 0);
	else
		return g_timestepT * g_flowFieldTex1.SampleLevel(samLinear, p, 0) + (1. - g_timestepT) * g_flowFieldTex0.SampleLevel(samLinear, p, 0);
}

glyphSpec vsGlyph(uint vertexID : SV_VertexID)
{
	int3 index;
	glyphSpec g;

	index.z = vertexID / (g_numGlyphs.x * g_numGlyphs.y);
	index.x = vertexID % g_numGlyphs.x;
	index.y = ((vertexID % (g_numGlyphs.y*g_numGlyphs.x)) / g_numGlyphs.x);
	
	g.col = float4(1, 1, 1, 1);//float4((float3)index/(float3)g_numGlyphs, 1);

	index -= g_numGlyphs / 2;

	float4 v = float4(g_sliceCenter + index * g_glyphSpacing, 1);


	g.pos = v;
	g.dir = SampleFlowField(v.xyz);

	return g;
}



[maxvertexcount(9)] 
void gsArrow(point glyphSpec glyph[1], inout TriangleStream<PSVertex>stream) {
	
	float3 l = normalize(g_camPos.xyz - glyph[0].pos.xyz);
	float3 x = normalize(glyph[0].dir);
	float3 y = normalize(cross(x, l));
	float3 z = cross(x, y);

	float s = 0.002 * g_glyphSize;
	
	float4x4 arrowTransform;
	arrowTransform[0] = s*float4(x,  0);
	arrowTransform[1] = s*float4(y,  0);
	arrowTransform[2] = s*float4(z,  0);
	arrowTransform[3] = float4( glyph[0].pos.xyz, 1);

	PSVertex v;
	v.col = glyph[0].col;
	v.oPos = glyph[0].pos.xyz;

	float4 v1 = mul(float4(-3,  .5, 0, 1), arrowTransform);
	float4 v2 = mul(float4( 1,  .5, 0, 1), arrowTransform);
	float4 v3 = mul(float4(-3, -.5, 0, 1), arrowTransform);
	float4 v4 = mul(float4( 1, -.5, 0, 1), arrowTransform);
	float4 v5 = mul(float4( 1, 1.5, 0, 1), arrowTransform);
	float4 v6 = mul(float4( 3,   0, 0, 1), arrowTransform);
	float4 v7 = mul(float4( 1,-1.5, 0, 1), arrowTransform);

	float3 n1 = normalize(mul(float4(0, 1, 1, 0), arrowTransform)).xyz;
	float3 n2 = normalize(mul(float4(0, -1, 1, 0),arrowTransform)).xyz;
	float3 n3 = normalize(mul(float4(1, 0, 1, 0), arrowTransform)).xyz;

	v.oPos = v1.xyz/v1.w;v.nor = n1; v.pos = mul(v1, g_worldViewProj);	stream.Append(v);
	v.oPos = v2.xyz/v2.w;v.nor = n1; v.pos = mul(v2, g_worldViewProj);	stream.Append(v);
	v.oPos = v3.xyz/v3.w;v.nor = n2; v.pos = mul(v3, g_worldViewProj);	stream.Append(v);
	stream.RestartStrip();
	v.oPos = v2.xyz/v2.w;v.nor = n1; v.pos = mul(v2, g_worldViewProj);	stream.Append(v);
	v.oPos = v4.xyz/v4.w;v.nor = n2; v.pos = mul(v4, g_worldViewProj);	stream.Append(v);
	v.oPos = v3.xyz/v3.w;v.nor = n2; v.pos = mul(v3, g_worldViewProj);	stream.Append(v);
	stream.RestartStrip();
	v.oPos = v5.xyz/v5.w;v.nor = n1; v.pos = mul(v5, g_worldViewProj);	stream.Append(v);
	v.oPos = v6.xyz/v6.w;v.nor = n3; v.pos = mul(v6, g_worldViewProj);	stream.Append(v);
	v.oPos = v7.xyz/v7.w;v.nor = n2; v.pos = mul(v7, g_worldViewProj);	stream.Append(v);
}

[maxvertexcount(16)] 
void gs3DArrow(point glyphSpec glyph[1], inout TriangleStream<PSVertex>stream) {
	
	float3 l = normalize(g_camPos.xyz - glyph[0].pos.xyz);
	float3 x = normalize(glyph[0].dir);
	float3 y = normalize(cross(x, l));
	if(!any(y))
		y = normalize(cross(x, float3(0, 1, 0)));
	float3 z = cross(x, y);
	
	float s = 0.002 * g_glyphSize;

	float4x4 arrowTransform;
	arrowTransform[0] = s*float4(x,  0);
	arrowTransform[1] = s*float4(y,  0);
	arrowTransform[2] = s*float4(z,  0);
	arrowTransform[3] = float4( glyph[0].pos.xyz, 1);

	PSVertex v;
	v.col = glyph[0].col;

	float4 v1 = mul(float4( 3,   0,   0, 1), arrowTransform);

	float4 v2 = mul(float4(-3,  .5, 0.5, 1), arrowTransform);
	float4 v3 = mul(float4(-3, -.5, 0.5, 1), arrowTransform);
	float4 v4 = mul(float4(-3, -.5, -.5, 1), arrowTransform);
	float4 v5 = mul(float4(-3,  .5, -.5, 1), arrowTransform);

	float4 v6 = mul(float4( 1, 1.5,  1.5, 1), arrowTransform);
	float4 v7 = mul(float4( 1,-1.5,  1.5, 1), arrowTransform);
	float4 v8 = mul(float4( 1,-1.5, -1.5, 1), arrowTransform);
	float4 v9 = mul(float4( 1, 1.5, -1.5, 1), arrowTransform);

	v.pos = mul(v1, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v2, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v3, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v4, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v1, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v5, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v2, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v4, g_worldViewProj);	stream.Append(v);
	stream.RestartStrip();
	v.pos = mul(v1, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v6, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v7, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v8, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v1, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v9, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v6, g_worldViewProj);	stream.Append(v);
	v.pos = mul(v8, g_worldViewProj);	stream.Append(v);
}

float4 psGlyph(PSVertex ps) : SV_Target
{
	float3 n = normalize(ps.nor);
	float3 l = normalize(ps.oPos - g_camPos.xyz);

	float3 v = l;						//light direction and viewing direction are the same for our "headlight"
	float3 rl = reflect(-l, n);


	//TODO: use global lighting parameters here

	float4 specular = float4(1,1,1,1) * pow(saturate(dot(rl, v)), 30);

	//return 0.5*float4(ps.nor, 1) + (float4)0.5;
	return 0.6*ps.col * saturate(dot(n, l)) + specular;
}


// Simple technique (a technique is a collection of passes)
technique11 GlyphVisualizer
{
	pass PASS_RENDER
	{
		SetVertexShader(CompileShader(vs_5_0, vsGlyph()));
		SetGeometryShader(CompileShader(gs_5_0, gsArrow()));
		SetPixelShader(CompileShader(ps_5_0, psGlyph()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
}
