cbuffer cbPickLocation : register(b0)
{
	uint mousePosX;
	uint mousePosY;
	uint2 _pad; // align to 16 bytes
}


Texture2D<uint> gBufferObjectID : register(t0);

RWStructuredBuffer<uint> pickedColor : register(u0);

[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	uint idColor = gBufferObjectID.Load(int3(mousePosX, mousePosY, 0));
	pickedColor[0] = idColor;
}