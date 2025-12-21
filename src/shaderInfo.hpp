#pragma once

// windows.h disahle minmax
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif

#include <d3d11.h>
#include <wrl.h>

#include <string>
#include <filesystem>

using namespace Microsoft::WRL;

struct ShaderInfo
{
	std::wstring filename;
	std::string entryPoint;
	std::filesystem::file_time_type lastModifiedTime;
	std::string shaderModelVersion;
	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11ComputeShader> computeShader;
	ComPtr<ID3DBlob> blob;
};