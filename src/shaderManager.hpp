#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include "shaderInfo.hpp"

using namespace Microsoft::WRL;
enum ShaderType
{
	COMPUTE,
	VERTEX,
	PIXEL
};

class ShaderManager
{
public:
	ShaderManager(const ComPtr<ID3D11Device>& device);

	bool LoadVertexShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "VS");
	bool LoadPixelShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "PS");
	bool LoadComputeShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "CS");

	ID3D11VertexShader* getVertexShader(const std::string& name);
	ID3D11PixelShader* getPixelShader(const std::string& name);
	ID3D11ComputeShader* getComputeShader(const std::string& name);
	ID3DBlob* getVertexShaderBlob(const std::string& name);

	void checkForChanges();

	void recompileAll();

	bool compileShader(ShaderInfo& info, ShaderType shaderType);
	std::filesystem::file_time_type getFileModifiedTime(const std::wstring& filename);

private:
	const ComPtr<ID3D11Device>& m_device;
};