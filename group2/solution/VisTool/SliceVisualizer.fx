//--------------------------------------------------------------------------------------
// File: SliceVisualizer.fx
//
// Shader Effect for calculating and rendering slices from the volume data
//
//--------------------------------------------------------------------------------------
#include "common.hlsli"
#include "TransparencyModuleInterface.hlsli"

cbuffer cbSliceVisualizer
{
	float4x4	g_modelWorldViewProj;
	float		g_timestepT;
	float3		g_velocityScaling;
	float3		g_sliceCorner; // slice corner where (u,v) = (0,0) in texture space
	float3		g_sliceDirectionU;	// slice direction u in texture space
	float3		g_sliceDirectionV;	// slice direction v in texture space
	uint2		g_sliceResolution;
	float3		g_planeNormal;

	uint		g_LICLength;
	float		g_LICStepThreshold;
	float		g_LICKernelSigma;
	float		g_LICStepsize = 1.;
};


Texture3D<float3> g_flowFieldTex0;
Texture3D<float3> g_flowFieldTex1;

Texture3D<float> g_scalarVolume;
Texture1D<float4> g_transferFunction;

RWTexture2D<float4> g_sliceTextureRW;
Texture2D<float4> g_sliceTexture;
Texture2D<float> g_noiseTexture;

// samples the volue dataset 
// either directly if we are not time dependent, or
// interpolating between the two textures
float3 SampleFlowField(float3 p) {
	if(g_timestepT == 0)
		return g_flowFieldTex0.SampleLevel(samLinear, p, 0);
	else
		return g_timestepT * g_flowFieldTex1.SampleLevel(samLinear, p, 0) + (1. - g_timestepT) * g_flowFieldTex0.SampleLevel(samLinear, p, 0);
}

float3 SampleVectorDeriv(float3 pos, float3 step)
{
	return SampleFlowField(pos + step) - SampleFlowField(pos - step);
}

float GetNoise(float2 pos)
{
	return g_noiseTexture.SampleLevel(samLinear, pos, 0);
}

struct GSIn_Slice {
	uint id : VERTEX_ID;
};

struct PSIn_Slice {
	float4 pos : SV_POSITION;
	float2 tex : VOL_TEXTURE;
};

GSIn_Slice vsSlice(uint vertexID : SV_VertexID)
{
	GSIn_Slice output;
	output.id = vertexID;
	return output;
}

[maxvertexcount(4)]
void gsSlice(point GSIn_Slice input[1], inout TriangleStream<PSIn_Slice> stream)
{
	float3 v0 = g_sliceCorner;
	float3 v1 = g_sliceCorner + g_sliceResolution.x * g_sliceDirectionU;
	float3 v2 = g_sliceCorner + g_sliceResolution.y * g_sliceDirectionV;
	float3 v3 = g_sliceCorner + g_sliceResolution.x * g_sliceDirectionU + g_sliceResolution.y * g_sliceDirectionV;

	PSIn_Slice output;
	output.pos = mul(float4(v0, 1), g_modelWorldViewProj);
	output.tex = float2(0, 0);
	stream.Append(output);
	output.pos = mul(float4(v1, 1), g_modelWorldViewProj);
	output.tex = float2(1, 0);
	stream.Append(output);
	output.pos = mul(float4(v2, 1), g_modelWorldViewProj);
	output.tex = float2(0, 1);
	stream.Append(output);
	output.pos = mul(float4(v3, 1), g_modelWorldViewProj);
	output.tex = float2(1, 1);
	stream.Append(output);
}

float4 psSlice(PSIn_Slice input) : SV_Target
{
	return float4(g_sliceTexture.SampleLevel(samLinear, input.tex, 0).rgb, 1);
}

[earlydepthstencil]
void psSliceTransparent(PSIn_Slice input)
{
	AddTransparentFragment(input.pos.xyz, g_sliceTexture.SampleLevel(samLinear, input.tex, 0));
}

// Compute Shader helpers and shaders

float3 SliceTexToVolumeTex(float2 tex) 
{
	return g_sliceCorner	+ g_sliceResolution.x * g_sliceDirectionU * tex.x 
							+ g_sliceResolution.y * g_sliceDirectionV * tex.y;
}

//should have support [-1, 1], but does not need to be normalized
float IntegrationKernel(float t)
{


	//if(t > 0)
	//	return exp((- 1. / (1. - t*t))*20);
	//else
		return exp(- 1. / (1. - t*t));
	//return exp(-t*t);
	//return 1;
	//return 0.2 * exp(- t*t*2.25);
}

// samples the flow field at the 2d texture position and
// projects the flow field vector back onto the 2d plane
float2 stepAt(float2 pos) {
	float3 field = SampleFlowField(SliceTexToVolumeTex(pos));
	return float2(dot(field, g_sliceDirectionU), dot(field, g_sliceDirectionV));
}

float2 IntegrateStepEuler(float2 pos, float h)
{
	float2 step = stepAt(pos);
	return pos + 0.001 * h * normalize(step);
}

float2 IntegrateStepHeun(float2 pos, float h)
{
	float2 step = stepAt(pos);
	float2 step2 = stepAt(pos + h * step);

	return pos + 0.001 * h * normalize(step + step2);
}

float2 IntegrateStepRK4(float2 pos, float h)
{
	float2 step1 = stepAt(pos);
	float2 step2 = stepAt(pos + 0.5*h * step1);
	float2 step3 = stepAt(pos + 0.5*h * step2);
	float2 step4 = stepAt(pos + h * step3);

	return pos + 0.001 * h * normalize (step1 + 2.*step2 + 2.*step3 + step4);
}

float2 IntegrateStep(float2 pos, float h)
{
	//return IntegrateStepRK4(pos, h);
	return IntegrateStepEuler(pos, h);
	//return IntegrateStepHeun(pos, h);
}

float ConvolveStreamlineNoise(float2 start)
{
	float w = IntegrationKernel(0);
	float intensity = w * GetNoise(start);
	float weightSum = w;
	float2 posForward = start, posBackward = start;

	// make g_LICLength/2 steps both forwards and backwards
	float step = 2. / g_LICLength;
	for(float i=0; i < 1; i += step) {
		float wF = IntegrationKernel(i);
		float wB = IntegrationKernel(-i);
		weightSum += wF + wB;

		posForward = IntegrateStep(posForward, g_LICStepsize);
		intensity += wF * GetNoise(posForward);

		posBackward = IntegrateStep(posBackward, - g_LICStepsize);
		intensity += wB * GetNoise(posBackward);
	}

	return intensity / weightSum;
}

[numthreads(32,32,1)]	
void CSComputeLICColored(uint3 threadID: SV_DispatchThreadID)
{
	float3 tex = g_sliceCorner + threadID.x * g_sliceDirectionU + threadID.y * g_sliceDirectionV;
	
	float2 start = threadID.xy / (float2)g_sliceResolution;

	float intensity = ConvolveStreamlineNoise(start);

	float4 col = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, tex, 0), 0);
	g_sliceTextureRW[threadID.xy] = intensity * col;
}

[numthreads(32,32,1)]	
void CSComputeLIC(uint3 threadID: SV_DispatchThreadID)
{
	float3 tex = g_sliceCorner + threadID.x * g_sliceDirectionU + threadID.y * g_sliceDirectionV;
	
	float2 start = threadID.xy / (float2)g_sliceResolution;

	float intensity = ConvolveStreamlineNoise(start);

	g_sliceTextureRW[threadID.xy] = (float4)intensity;
}

[numthreads(32,32,1)]	
void CSComputeTFSlice(uint3 threadID: SV_DispatchThreadID)
{
	float3 tex = g_sliceCorner + threadID.x * g_sliceDirectionU + threadID.y * g_sliceDirectionV;
	g_sliceTextureRW[threadID.xy] = g_transferFunction.SampleLevel(samLinear, g_scalarVolume.SampleLevel(samLinear, tex, 0), 0);
}

technique11 SliceVisualizer
{
	pass PASS_RENDER
	{
		SetVertexShader(CompileShader(vs_5_0, vsSlice()));
		SetGeometryShader(CompileShader(gs_5_0, gsSlice()));
		SetPixelShader(CompileShader(ps_5_0, psSlice()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_RENDER_TRANSPARENT
	{
		SetVertexShader(CompileShader(vs_5_0, vsSlice()));
		SetGeometryShader(CompileShader(gs_5_0, gsSlice()));
		SetPixelShader(CompileShader(ps_5_0, psSliceTransparent()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthNoWrite, 0);
		SetBlendState(BlendDisableAll, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

	pass PASS_COMPUTE_TF_SLICE
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeTFSlice()));
	}
	pass PASS_COMPUTE_2D_LIC
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeLIC()));
	}
	pass PASS_COMPUTE_2D_LIC_COLORED
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeLICColored()));
	}
}
