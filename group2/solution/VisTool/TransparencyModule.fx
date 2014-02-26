// This shader and accompanying module manages Order-Independent Transparency
// It is designed as a module that can be used by all classes, provided that 

#include "common.hlsli"
#include "TransparencyModuleInterface.hlsli"

cbuffer cbTransparencyModule
{
	uint2 g_viewportResolution;
};

struct PSIn_FullscreenQuad {
	float4 pos : SV_Position;
};

struct FragmentSortedListElement {
	float4 color;
	float depth;
};

Texture2D<uint> g_listHead;
StructuredBuffer<FragmentNode> g_fragmentBuffer;

uint vsFullscreenQuad(uint vertexID : SV_VertexID) : NOTHING
{
	return 0;
}

[maxvertexcount(4)]
void gsFullscreenQuad(point uint n[1] : NOTHING, inout TriangleStream<PSIn_FullscreenQuad> stream) 
{
	PSIn_FullscreenQuad q;
	q.pos = float4(-1, -1, 0, 1); stream.Append(q);
	q.pos = float4(1, -1, 0, 1); stream.Append(q);
	q.pos = float4(-1, 1, 0, 1); stream.Append(q);
	q.pos = float4(1, 1, 0, 1); stream.Append(q);
}

#define NUM_MAX_LAYERS 32

float4 psFullscreenTransparency(float4 pos : SV_Position) : SV_Target
{
	FragmentSortedListElement fragments[NUM_MAX_LAYERS+1];
	uint listIndex;
	uint numFragments = 0;

	listIndex = g_listHead[uint2(pos.xy)];

	/*[unroll]
	for(uint i=0; i < NUM_MAX_LAYERS+1; i++) {
		fragments[i].depth = 1;
		fragments[i].color = (float4)0;
	}*/

	//return float4((float3)g_fragmentBuffer[listIndex].depth, 1);
	
	fragments[0].depth = 1;
	fragments[0].color = (float4)0;

	//traverse the list
	while(listIndex != FRAGMENT_LIST_END) {
		FragmentNode f = g_fragmentBuffer[listIndex];

		
		//insert into the sorted list
		//if(f.color.w >= 0.0005) 
		{
			FragmentSortedListElement e;
			e.color = f.color;
			e.depth = f.depth;

			//numFragments = min(numFragments + 1, NUM_MAX_LAYERS);
			//init the new fragment
			if(numFragments < NUM_MAX_LAYERS) {
				numFragments++;
				fragments[numFragments].depth = 1;
				//fragments[numFragments].color = (float4)0;
			}
			
			/*for(int j=0; j < numFragments+1; j++) {
				if(fragments[j].depth > e.depth) {
					FragmentSortedListElement t = fragments[j];
					fragments[j] = e;
					e = t;
				}
			}*/
			for(int j = numFragments; j >= 0; j--) {
				if(j == 0 || fragments[j-1].depth < e.depth)  {
					fragments[j] = e;
					break;
				}
				else {
					fragments[j] = fragments[j-1];
				}
			}

		}

		listIndex = f.nextNode;
	}

	
	// to analyze the transparency layer complexity of the scene
	//if(numFragments == NUM_MAX_LAYERS)
	//	return float4(1, 0, 0, 1);
	//return float4((float3)((float)numFragments/(float)NUM_MAX_LAYERS), 0.6);
	

	//blend front to back, premultiplied
	float4 blendedColor = float4(fragments[0].color.rgb*fragments[0].color.a, fragments[0].color.a);

	//return blendedColor;

	for(uint k=1; k < numFragments; k++) {
		float4 C_a = fragments[k].color;
		blendedColor.rgb =  blendedColor.rgb + (1. - blendedColor.w) * C_a.w * C_a.xyz;
		blendedColor.w = blendedColor.w + (1. - blendedColor.w) * C_a.w;
	}
	return blendedColor;
}


float4 psFragmentCount(float4 pos : SV_Position) : SV_Target
{
	FragmentSortedListElement fragments[NUM_MAX_LAYERS+1];
	uint listIndex;
	uint numFragments = 0;

	listIndex = g_listHead[uint2(pos.xy)];


	if(listIndex == FRAGMENT_LIST_END)
		discard;

	//traverse the list
	while(listIndex != FRAGMENT_LIST_END) {
		FragmentNode f = g_fragmentBuffer[listIndex];
		numFragments ++;

		if(f.depth < 0)
			return float4(0, 1, 0, 1);
		if(f.depth > 1)
			return float4(0, 0, 1, 1);

		listIndex = f.nextNode;
	}

	
	// to analyze the transparency layer complexity of the scene
	if(numFragments == NUM_MAX_LAYERS)
		return float4(1, 0, 0, 1);
	float t = ((float)numFragments/(float)NUM_MAX_LAYERS);
	return float4((float3)t, 1);
}

float4 psBlendDepth(float4 pos : SV_Position) : SV_Target
{
	FragmentSortedListElement fragments[NUM_MAX_LAYERS+1];
	uint listIndex;
	uint numFragments = 0;

	listIndex = g_listHead[uint2(pos.xy)];


	fragments[0].depth = 1;
	fragments[0].color = (float4)0;

	//traverse the list
	while(listIndex != FRAGMENT_LIST_END) {
		FragmentNode f = g_fragmentBuffer[listIndex];

		{
			FragmentSortedListElement e;
			e.color = float4((float3)(1. - (f.depth)), 0.4);
			e.depth = f.depth;


			if(numFragments < NUM_MAX_LAYERS) {
				numFragments++;
				fragments[numFragments].depth = 1;
			}

			for(int j = numFragments; j >= 0; j--) {
				if(j == 0 || fragments[j-1].depth < e.depth)  {
					fragments[j] = e;
					break;
				}
				else {
					fragments[j] = fragments[j-1];
				}
			}

		}

		listIndex = f.nextNode;
	}

	
	//blend front to back, premultiplied
	float4 blendedColor = float4(fragments[0].color.rgb*fragments[0].color.a, fragments[0].color.a);

	//return blendedColor;

	for(uint k=1; k < numFragments; k++) {
		float4 C_a = fragments[k].color;
		blendedColor.rgb =  blendedColor.rgb + (1. - blendedColor.w) * C_a.w * C_a.xyz;
		blendedColor.w = blendedColor.w + (1. - blendedColor.w) * C_a.w;
	}
	return blendedColor;
}

technique11 TransparencyModule
{
	pass RENDER_TRANSPARENCY_TO_SCREEN
	{
		SetVertexShader(CompileShader(vs_5_0, vsFullscreenQuad()));
		SetGeometryShader(CompileShader(gs_5_0, gsFullscreenQuad()));
		SetPixelShader(CompileShader(ps_5_0, psFullscreenTransparency()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass RENDER_FRAGMENT_COUNT
	{
		SetVertexShader(CompileShader(vs_5_0, vsFullscreenQuad()));
		SetGeometryShader(CompileShader(gs_5_0, gsFullscreenQuad()));
		SetPixelShader(CompileShader(ps_5_0, psFragmentCount()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
	pass RENDER_BLENDED_DEPTH
	{
		SetVertexShader(CompileShader(vs_5_0, vsFullscreenQuad()));
		SetGeometryShader(CompileShader(gs_5_0, gsFullscreenQuad()));
		SetPixelShader(CompileShader(ps_5_0, psBlendDepth()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendPremultAlpha, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
}
