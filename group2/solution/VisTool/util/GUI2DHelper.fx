#include "../common.hlsli"

cbuffer cbGUI2DHelper
{
	float2 g_mousePosition;
	float g_viewportX;
	bool g_sideBySide3DEnabled;
};

struct PSIn_FullscreenQuad {
	float4 pos : SV_Position;
	float2 tex : TEXTURE;
};

Texture2D<float4> g_tex2DGUI;

uint vsFullscreenQuad(uint vertexID : SV_VertexID) : NOTHING
{
	return 0;
}

[maxvertexcount(4)]
void gsFullscreenQuad(point uint n[1] : NOTHING, inout TriangleStream<PSIn_FullscreenQuad> stream) 
{
	PSIn_FullscreenQuad q;
	q.pos = float4(-1, -1, 0, 1); q.tex = float2(0, 1); stream.Append(q);
	q.pos = float4(1, -1, 0, 1); q.tex = float2(1, 1); stream.Append(q);
	q.pos = float4(-1, 1, 0, 1); q.tex = float2(0, 0); stream.Append(q);
	q.pos = float4(1, 1, 0, 1); q.tex = float2(1, 0); stream.Append(q);
}

static const float dr = 5.0;

float4 psFullscreenQuad(float4 pos : SV_Position, float2 tex : TEXTURE) : SV_Target
{
	if(g_sideBySide3DEnabled) {
		pos.x = fmod(2.*pos.x, g_viewportX);
		tex.x = fmod(2.*tex.x, 1.);
	}
	float2 d = pos.xy - g_mousePosition;
	float d2 = dot(d, d);
	if(d2 < dr*dr) {
		//blend cursor on top
		float4 cursor = float4(1., 0, 0, 1. - d2/(dr*dr));
		float4 bg = g_tex2DGUI.SampleLevel(samLinear, tex, 0.0);
		return cursor * cursor.a + (1. - cursor.a) * bg;
	}
	else
		return g_tex2DGUI.SampleLevel(samLinear, tex, 0.0);
}


technique11 TransparencyModule
{
	pass RENDER_GUI_ON_TOP
	{
		SetVertexShader(CompileShader(vs_5_0, vsFullscreenQuad()));
		SetGeometryShader(CompileShader(gs_5_0, gsFullscreenQuad()));
		SetPixelShader(CompileShader(ps_5_0, psFullscreenQuad()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
}
