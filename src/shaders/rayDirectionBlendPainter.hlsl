cbuffer RayDirectionBlendConstants : register(b0)
{
	float2 coords;
	float brushSize;
	float blendValue;
	float2 dimensions;
	float2 _pad;
};

RWTexture2D<float> oRayDirectionBlend : register(u0);

[numthreads(16, 16, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float sizeOfBrush = brushSize / (dimensions.x / 5.0f); // Assuming square texture, so using x dimension for normalization
	uint2 pixelCoord = dispatchThreadID.xy;
	float2 uv = pixelCoord / dimensions;
	float distanceToBrushCenter = distance(uv, coords);
	if (distanceToBrushCenter < sizeOfBrush)
	{
		float blendFactor = 1;
		blendFactor *= blendValue; // Apply user-defined blend value
		oRayDirectionBlend[pixelCoord] = blendFactor;
	}
}
