
//we loose one fragment in the fragment buffer by using zero as the list end marker
//however, this makes the method far more robust since unbound our out-of-bounds texture accesses
//generate zeros, effectively terminating the list instead of running into an endless loop, 
//leading to driver crashes / freezes
#define FRAGMENT_LIST_END (0)

struct FragmentNode {
	float4 color;
	float depth;
	uint nextNode;
};

cbuffer cbTransparency {
	uint g_fragmentBufferSize;
};

RWTexture2D<uint> g_listHeadRW;
RWStructuredBuffer<FragmentNode> g_fragmentBufferRW;

void AddTransparentFragment(float3 pos, float4 fragmentColor)
{
	//we ignore fragments that are too transparent
	if(fragmentColor.w < 0.001) 
		return;

	uint2 screenPos = uint2(pos.xy);

	uint nNewFragmentAddress = g_fragmentBufferRW.IncrementCounter();
	// check if our fragment buffer is full
	if( nNewFragmentAddress >= g_fragmentBufferSize) { 
		return; 
	}

	uint nOldFragmentAddress;
	InterlockedExchange(g_listHeadRW[screenPos], nNewFragmentAddress, nOldFragmentAddress);

	FragmentNode n;
	n.color = fragmentColor;
	n.depth = pos.z;
	n.nextNode = nOldFragmentAddress;
	g_fragmentBufferRW[nNewFragmentAddress] = n;
	return;
}
