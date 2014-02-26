/**
	Collection of compute shaders to calculate various metrics on vector volume data
*/

// for the normal calculation
RWTexture3D<float3> g_normalTexture;

// for time interpolation
Texture3D<float> g_scalarTextureT0;
Texture3D<float> g_scalarTextureT1;
RWTexture3D<float> g_scalarTextureInterpolated;

RWTexture2D<float2> g_minMaxTexture;

cbuffer cbScalarVolume {
	float g_timestepT;
	uint3	g_volumeRes;
}

[numthreads(32,2,2)]
void CSComputeInterpolation(uint3 threadID: SV_DispatchThreadID)
{
	g_scalarTextureInterpolated[threadID] = (1. - g_timestepT) * g_scalarTextureT0[threadID] + g_timestepT * g_scalarTextureT1[threadID];
}

[numthreads(32,2,2)]
void CSComputeNormal(uint3 threadID: SV_DispatchThreadID)
{
	float3 n;
	n.x = g_scalarTextureT0[threadID - uint3(1, 0, 0)] - g_scalarTextureT0[threadID + uint3(1, 0, 0)];
	n.y = g_scalarTextureT0[threadID - uint3(0, 1, 0)] - g_scalarTextureT0[threadID + uint3(0, 1, 0)];
	n.z = g_scalarTextureT0[threadID - uint3(0, 0, 1)] - g_scalarTextureT0[threadID + uint3(0, 0, 1)];
	g_normalTexture[threadID] = normalize(n);
}

[numthreads(1,1,1)]
//calculates the min and max along a line in x direction
void CSGetMinMax(uint3 threadID: SV_DispatchThreadID)
{
	uint3 p = threadID;
	float threadMin = g_scalarTextureT0[p];
	float threadMax = g_scalarTextureT0[p];
	for(uint i=1; i < g_volumeRes.x; i++) {
		p.x++;
		float m = g_scalarTextureT0[p];
		threadMin = min(threadMin, m);
		threadMax = max(threadMax, m);
	}
	g_minMaxTexture[threadID.yz] = float2(threadMin, threadMax);
}

technique11 ScalarVolumeComputeShaders
{
	pass PASS_INTERPOLATE
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeInterpolation()));
	}
	pass PASS_COMPUTE_NORMALS
	{
		SetComputeShader(CompileShader(cs_5_0, CSComputeNormal()));
	}
	pass PASS_MINMAX {
		SetComputeShader(CompileShader(cs_5_0, CSGetMinMax()));
	}
}