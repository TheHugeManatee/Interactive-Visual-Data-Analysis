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

cbuffer cbSimpleMesh
{
	float4x4	g_worldViewProj;
	float4		g_meshColor;
};

struct SimpleVertex
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSIn
{
	float4 pos   : SV_Position;
	float3 worldPos : WPOS;
	float4 color : COLOR;
	float3 normal : NORM;
};



void vsSimpleMesh(SimpleVertex v, out PSIn output)
{
	output.color = g_meshColor;

	// transform vertex position into clip space
	output.pos = mul(float4(v.pos, 1), g_worldViewProj);
	output.worldPos = v.pos;
	output.normal = v.normal;//mul(float4(v.normal, 0), g_worldViewProjInvTransp).xyz;
	//output.color = (float4)0.5 + float4(0.5 * output.normal, 0.5);
}


float4 psSimpleMesh(PSIn input) : SV_Target
{
	//this is actually done in object space, but we ignore possible normal distortions, assuming the mesh is only uniformly scaled
	return WorldSpaceLighting(input.worldPos, normalize(input.normal), input.color);
}

//A simple vertex shader that has harcoded vertex positions 
void vsBBox(uint vertexID : SV_VertexID, out PSIn output)
{
	//assignment 1.3
	switch(vertexID)
	{
		case 0:			output.pos = float4(  .0,  1.0,   .0, 1.0);			break;
		case 1:			output.pos = float4( 1.0,  1.0,   .0, 1.0);			break;
		case 2:			output.pos = float4(  .0,   .0,   .0, 1.0);			break;
		case 3:			output.pos = float4( 1.0,   .0,   .0, 1.0);			break;
		case 4:			output.pos = float4(  .0,  1.0,  1.0, 1.0);			break;
		case 5:			output.pos = float4( 1.0,  1.0,  1.0, 1.0);			break;
		case 6:			output.pos = float4(  .0,   .0,  1.0, 1.0);			break;
		case 7:			output.pos = float4( 1.0,   .0,  1.0, 1.0);			break;
		default:		output.pos = float4( 0.0,  0.0,  0.0, 1.0);			break;
	}

	output.color = g_meshColor;
	output.worldPos = output.pos.xyz;

	// transform vertex position into clip space
	output.pos = mul(output.pos, g_worldViewProj);
	output.normal = float3(0, 0, 0);
}

//A simple pixel shader that returns the input vertex color (interpolated output of vertex shader)
float4 psSimple(PSIn input) : SV_Target
{
	return input.color;
}



// Simple technique (a technique is a collection of passes)
technique11 SimpleMesh
{
	//triangles
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsSimpleMesh()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimpleMesh()));
		SetRasterizerState(CullBack);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	//bbox
	pass P1
	{
		SetVertexShader(CompileShader(vs_5_0, vsBBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimple()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
}
