#pragma once

#include <filesystem>
#include <string>

#include <d3d11_4.h>
#include <wrl.h>

#include "shaderInfo.hpp"

using namespace Microsoft::WRL;

// Default shader path if not defined by CMake (relative from build output directory)
#ifndef SHADER_PATH
#define SHADER_PATH L"../../src/shaders/"
#endif

enum ShaderType
{
	COMPUTE,
	VERTEX,
	PIXEL
};

class ShaderManager
{
public:
	explicit ShaderManager(ComPtr<ID3D11Device> device);

	// Helper to build full shader path from just the shader filename
	static std::wstring GetShaderPath(const std::wstring& shaderFilename);

	bool LoadVertexShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "VS");
	bool LoadPixelShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "PS");
	bool LoadComputeShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "CS");

	static ID3D11VertexShader* getVertexShader(const std::string& name);
	static ID3D11PixelShader* getPixelShader(const std::string& name);
	static ID3D11ComputeShader* getComputeShader(const std::string& name);
	static ID3DBlob* getVertexShaderBlob(const std::string& name);

	void checkForChanges();

	void recompileAll();

	bool compileShader(ShaderInfo& info, ShaderType shaderType) const;
	static std::filesystem::file_time_type getFileModifiedTime(const std::wstring& filename);

private:
	ComPtr<ID3D11Device> m_device;
};
