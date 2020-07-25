cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld; 
    float4x4 gViewProj;
    float3 gEyePosW;
};

struct HullOut
{
    float3 PosL : POSITION;
};


float4 BernSteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(
		invT * invT * invT,
		3.0f * t * invT * invT,
		3.0f * t * t * invT, 
		t * t * t
    );
}

float4 dBernSteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(
		-3 * invT * invT,
		3 * invT * invT - 6 * t * invT,
		6 * t * invT - 3 * t * t,
		3 * t * t
	);
}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0, 0, 0);
    sum  = basisV.x * (basisU.x*bezpatch[0].PosL  + basisU.y*bezpatch[1].PosL  + basisU.z*bezpatch[2].PosL  + basisU.w*bezpatch[3].PosL );
    sum += basisV.y * (basisU.x*bezpatch[4].PosL  + basisU.y*bezpatch[5].PosL  + basisU.z*bezpatch[6].PosL  + basisU.w*bezpatch[7].PosL );
    sum += basisV.z * (basisU.x*bezpatch[8].PosL  + basisU.y*bezpatch[9].PosL  + basisU.z*bezpatch[10].PosL + basisU.w*bezpatch[11].PosL);
    sum += basisV.w * (basisU.x*bezpatch[12].PosL + basisU.y*bezpatch[13].PosL + basisU.z*bezpatch[14].PosL + basisU.w*bezpatch[15].PosL);

    return sum;
}

// The above functions can be utilized like so to evaluate p(u, v) and compute the partial derivataves:


struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    return vout;
}

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 16> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    float3 centerL = 0.25f * (patch[0].PosL + patch[3].PosL + patch[15].PosL + patch[12].PosL);
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;

    float d = distance(centerW, gEyePosW);

    // Tessellate the patch based on distance from eyes.
	// the tessellation is 0 if d >= d1 and 64 if d <= d0.  The interval
	// [d0, d1] defines the range we tessellate in.

    const float d0 = 20.0f;
    const float d1 = 100.0f;
    float tess = 64.0f * saturate((d1 - d) / (d1 - d0));

    // Uniformly tessellate the patch.

    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.EdgeTess[3] = tess;

    pt.InsideTess[0] = tess;
    pt.InsideTess[1] = tess;

    return pt;
}


[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 16> p, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    
    hout.PosL = p[i].PosL;

    return hout;
}

struct DomainOut
{
    float4 PosH : SV_Position;
};

[domain("quad")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 16> quad)
{
    DomainOut dout;

    float4 basisU = BernSteinBasis(uv.x);
    float4 basisV = BernSteinBasis(uv.y);

    float3 p = CubicBezierSum(quad, basisU, basisV);

    float4 posW = mul(float4(p, 1.0f), gWorld);

    dout.PosH = mul(posW, gViewProj);

    return dout;
}

float4 PS(DomainOut din) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
 