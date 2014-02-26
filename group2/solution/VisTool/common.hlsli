#ifndef COMMON_HLSLI
#define COMMON_HLSLI

//Corresponds to D3D11_RASTERIZER_DESC
RasterizerState CullNone
{
	CullMode = None;
};

RasterizerState CullBack {
    CullMode = Back;
};

RasterizerState CullFront {
    CullMode = Front;
};


//Corresponds to D3D11_DEPTH_STENCIL_DESC
DepthStencilState DepthDefault
{

};

DepthStencilState DepthDisable
{
	DepthEnable = false;
	DepthWriteMask = Zero;
};


DepthStencilState DepthNoWrite
{
	DepthEnable = true;
	DepthWriteMask = Zero;
};

//Corresponds to D3D11_BLEND_DESC
BlendState BlendDisable
{
};


BlendState BlendPremultAlpha
{
	BlendEnable[0] = true;
	SrcBlend = ONE;
	DestBlend = INV_SRC_ALPHA;
};

BlendState BlendBackToFront
{
    BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
};

BlendState BlendDisableAll
{
    BlendEnable[0] = FALSE;
    RenderTargetWriteMask[0] = 0;
};

SamplerState samLinear {
	Filter = MIN_MAG_MIP_LINEAR;
};

cbuffer LightingParams
{
	float3		g_lightPos; // in world space
	float3		g_lightColor = float3(1, 1, 1);
	float		g_specularExp = 100;
	float		k_s = 0.4;
	float		k_d = 0.6;
	float		k_a = 0.1;
}

float4 WorldSpaceLighting(float3 p, float3 n, float4 surfaceColor)
{
	float3 l = normalize(p - g_lightPos);

	float3 specular = g_lightColor * pow(abs(dot(l, n)), g_specularExp);
	float3 diffuse = g_lightColor * surfaceColor.rgb * abs(dot(n, l));
	float3 ambient = surfaceColor.rgb;

	float3 col = (k_s * specular) + (k_d * diffuse) + k_a * ambient;

	return float4(col, surfaceColor.a);
}

float4 ViewSpaceLighting(float3 p, float3 n, float4 surfaceColor)
{
	float3 l = normalize(p);

	float3 specular = g_lightColor * pow(abs(dot(l, n)), g_specularExp);
	float3 diffuse = g_lightColor * surfaceColor.rgb * abs(dot(n, l));
	float3 ambient = surfaceColor.rgb;

	float3 col = (k_s * specular) + (k_d * diffuse) + k_a * ambient;

	return float4(col, surfaceColor.a);
}

#endif