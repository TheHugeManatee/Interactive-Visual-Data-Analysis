/**
	Collection of compute shaders to calculate various metrics on vector volume data
*/

RWTexture3D<float> g_metricTexture;
Texture3D<float4> g_vectorTexture0;
Texture3D<float4> g_vectorTexture1;

cbuffer cbVectorVolume {
	float  g_timestepT = 0;
	float3 g_metricMinMax;	//x ^= min, y ^= max, z = (max - min)
	uint3	g_volumeRes;
	bool g_boundaryMetricFixed;
	float g_boundaryMetricValue;
}

float constrainMetric(float m)
{
	return (m - g_metricMinMax.x)/g_metricMinMax.z;
}

float4 SampleVectorVolume(uint3 pos)
{
	if(g_timestepT == 0)
		return g_vectorTexture0[pos];
	else
		return g_timestepT * g_vectorTexture1[pos] + (1. - g_timestepT) * g_vectorTexture0[pos];
}

uint3 clampPos(uint3 pos)
{
	return min(g_volumeRes, max(uint3(0,0,0), pos));
}

float4 SampleVectorDeriv(uint3 pos, uint3 step)
{
	return SampleVectorVolume(clampPos(pos + step)) - SampleVectorVolume(clampPos(pos - step));
}

float3x3 SampleJacobian(uint3 pos) 
{
	float3 ux = SampleVectorDeriv(pos, uint3(1, 0, 0)).xyz;
	float3 uy = SampleVectorDeriv(pos, uint3(0, 1, 0)).xyz;
	float3 uz = SampleVectorDeriv(pos, uint3(0, 0, 1)).xyz;
	
	float3x3 jacobian = {
		ux.x, uy.x, uz.x,
		ux.y, uy.y, uz.y,
		ux.z, uy.z, uz.z
	};
	/*
	float3x3 jacobian = {
		ux.x, ux.y, ux.z,
		uy.x, uy.y, uy.z,
		uz.x, uz.y, uz.z
	};*/
	return jacobian;
}

[numthreads(32,2,2)]	
void CSFourthComponent(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}
	float u = SampleVectorVolume(threadID).w;
	g_metricTexture[threadID] = constrainMetric(u);
}

[numthreads(32,2,2)]	
void CSVelocityX(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}
	float3 u = SampleVectorVolume(threadID).xyz;
	g_metricTexture[threadID] = constrainMetric(u.x);
}

[numthreads(32,2,2)]
void CSVelocityMagnitude(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}
	float3 u = SampleVectorVolume(threadID).xyz;
	g_metricTexture[threadID] = constrainMetric(length(u));
}

[numthreads(32,2,2)]
void CSDivergence(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	float div = J._m00 + J._m11 + J._m22;//ux.x + uy.y + uz.z;
	g_metricTexture[threadID] = constrainMetric(div);
	return;
}

[numthreads(32,2,2)]
void CSVorticityMagnitude(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	float3 u = float3(J._m21 - J._m12, J._m02 - J._m20, J._m10 - J._m01);//float3(uz.y - uy.z, ux.z - uz.x, uy.x - ux.y);

	g_metricTexture[threadID] = constrainMetric(length(u));
}

[numthreads(32,2,2)]
void CSEnstrophyProduction(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	float3x3 S = 0.5 * (J + transpose(J));
	float3 curl = float3(J._m21 - J._m12, J._m02 - J._m20, J._m10 - J._m01);

	float enstrophyProd = dot(curl, mul(curl, S));

	g_metricTexture[threadID] = constrainMetric(enstrophyProd);
}

[numthreads(32,2,2)]
void CSVSquared(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	float3x3 S = 0.5 * (J + transpose(J));
	float3 curl = float3(J._m21 - J._m12, J._m02 - J._m20, J._m10 - J._m01);

	float V2 = 0;
	// no loop unrolling - who cares about performance anyway...
	for(int i=0; i < 2; i++)
		for(int j=0; j < 2; j++)
			V2 += S[i][j]*curl[j]*S[i][j]*curl[j];

	g_metricTexture[threadID] = constrainMetric(V2);
}

[numthreads(32,2,2)]
void CSQParameter(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	
	float3x3 S = 0.5 * (J + transpose(J));
	float3x3 Omega = 0.5 * (J - transpose(J));

	float3x3 OOT = mul(transpose(Omega), Omega);
	float3x3 SST = mul(transpose(S), S);

	float Q = 0.5 * sqrt(OOT[0][0] + OOT[1][1] + OOT[2][2]) - 0.5 * sqrt(SST[0][0] + SST[1][1] + SST[2][2]);
	
	g_metricTexture[threadID] = constrainMetric(Q);
}

[numthreads(32,2,2)]
void CSQSParameter(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	
	float3x3 S = 0.5 * (J + transpose(J));
	float3x3 SSquared = mul(S, S);
	float Q_s = -0.5 * (SSquared._m00 + SSquared._m11 + SSquared._m22); //square rate of strain

	g_metricTexture[threadID] = constrainMetric(Q_s);
}

[numthreads(32,2,2)]
void CSQOmegaParameter(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = g_boundaryMetricValue;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	float3x3 Omega = 0.5 * (J - transpose(J));
	float3x3 OmegaSquared = mul(Omega, Omega);
	float Q_omega = 0.5 * (OmegaSquared._m00 + OmegaSquared._m11 + OmegaSquared._m22);// square rotation

	g_metricTexture[threadID] = constrainMetric(Q_omega);;
}

[numthreads(32,2,2)]
void CSLambda2(uint3 threadID: SV_DispatchThreadID)
{
	if(g_boundaryMetricFixed && (!all(threadID) || !all(threadID - g_volumeRes))) {
		g_metricTexture[threadID] = 0;
		return;
	}

	float3x3 J = SampleJacobian(threadID);
	
	float3x3 S = 0.5 * (J + transpose(J));
	float3x3 Omega = 0.5 * (J - transpose(J));

	float3x3 OmegaSquared = mul(Omega, Omega);
	float3x3 SSquared = mul(S, S);

	float3x3 A = OmegaSquared + SSquared;
	
/*
// Analytical Eigenvalues of 3x3 matrices:
// http://www.wolframalpha.com/input/?i=det+[[a_0+-+l%2Ca_1%2C+a_2]%2C[b_0%2Cb_1-l%2Cb_2]%2C[c_0%2Cc_1%2Cc_2-l]]
// and http://www.wolframalpha.com/input/?i=l^3+%2B+c_2+*+l^2+%2B+c_1+*+l+%2B+c_0+%3D+0+solve+for+l
	float c2 = - A._m00 - A._m11 - A._m22;
	float c1 = - ( -A._m11*A._m22 + A._m12*A._m21 - A._m00*A._m22 + A._m02*A._m20 - A._m00*A._m11 + A._m01*A._m10);
	float c0 = - ( -A._m02*A._m11*A._m20 + A._m01*A._m12*A._m20  + A._m02*A._m10*A._m21 - A._m00*A._m12*A._m21 - A._m01*A._m10*A._m22 + A._m00*A._m11*A._m22);

	float k1 = -2.*c2*c2 + 9.*c1*c2 - 27.*c0;
	float k2 = 3.*c1 - c2*c2;
	float s = sqrt(4.*k2*k2*k2 + k1*k1);

	float s2 = pow(k1 + s, 1./3.);

	float twopowonethird = 1.2599210498948731647672106072782283505;
	float lambda2 = -c2/3. + s2 / (3.*twopowonethird) - twopowonethird*k2/(3.*s2);
*/	

	// power iteration for approximation of first eigenvector
	float3 w, x1 = float3(1,1,1);

	[unroll]
	for(int i=0; i < 15; i++) {
		w = mul(x1, A);
		x1 = normalize(w);
	}
	float lambda1 = dot(x1, mul(x1, A));

	// compute new matrix where second eigenvalue of A is now largest eigenvalue
	// N = x1 * x1^T
	float3x3 N = {x1.x*x1.x, x1.x*x1.y, x1.x*x1.z, 
				  x1.y*x1.x, x1.y*x1.y, x1.y*x1.z,
				  x1.z*x1.x, x1.z*x1.y, x1.z*x1.z};
	float3x3 A2 = A - lambda1 * N;
	
	// repeated power iteration yields (approximation of) second eigenvector
	float3 x2 = float3(1,1,1);

	[unroll]
	for(int j=0; j < 15; j++) {
		w = mul(x2, A2);
		x2 = normalize(w);
	}

	float lambda2 = dot(x2, mul(x2, A)); // rayleigh quotient yields eigenvalue

	g_metricTexture[threadID] = constrainMetric(lambda2);;
}

technique11 Metrics
{
	pass PASS_FOURTH_COMPONENT	{
		SetComputeShader(CompileShader(cs_5_0, CSFourthComponent()));
	}
	pass PASS_VELOCITY_MAGNITUDE	{
		SetComputeShader(CompileShader(cs_5_0, CSVelocityMagnitude()));
	}
	pass PASS_DIVERGENCE {
		SetComputeShader(CompileShader(cs_5_0, CSDivergence()));
	}
	pass PASS_VORTICITY_MAGNITUDE	{
		SetComputeShader(CompileShader(cs_5_0, CSVorticityMagnitude()));
	}
	pass PASS_Q_S_PARAMETER	{
		SetComputeShader(CompileShader(cs_5_0, CSQSParameter()));
	}
	pass PASS_Q_OMEGA_PARAMETER	{
		SetComputeShader(CompileShader(cs_5_0, CSQOmegaParameter()));
	}
	pass PASS_ENSTROPHY_PRODUCTION	{
		SetComputeShader(CompileShader(cs_5_0, CSEnstrophyProduction()));
	}
	pass PASS_V_SQUARED	{
		SetComputeShader(CompileShader(cs_5_0, CSVSquared()));
	}
	pass PASS_Q_PARAMETER	{
		SetComputeShader(CompileShader(cs_5_0, CSQParameter()));
	}
	pass PASS_LAMBDA2	{
		SetComputeShader(CompileShader(cs_5_0, CSLambda2()));
	}
}