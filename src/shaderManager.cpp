#include "shaderManager.hpp"

#include <filesystem>
#include <iostream>
#include <unordered_map>

#include <d3dcompiler.h>
#include <ranges>

static std::unordered_map<std::string, ShaderInfo> m_vertexShaders;
static std::unordered_map<std::string, ShaderInfo> m_pixelShaders;
static std::unordered_map<std::string, ShaderInfo> m_computeShaders;

std::wstring ShaderManager::GetShaderPath(const std::wstring& shaderFilename)
{
	return std::wstring(SHADER_PATH) + shaderFilename;
}

ShaderManager::ShaderManager(ComPtr<ID3D11Device> device)
{
	m_device = device;
}

bool ShaderManager::LoadVertexShader(const std::string& name,
	const std::wstring& filename,
	const std::string& entryPoint)
{
	if (m_vertexShaders.contains(name))
	{
		std::wcout << L"Vertex shader already exists: " << name.c_str() << std::endl;
		return true;
	}
	ShaderInfo info;
	info.filename = filename;

	info.entryPoint = entryPoint;
	info.shaderModelVersion = "vs_5_0";
	info.lastModifiedTime = getFileModifiedTime(filename);

	if (compileShader(info, VERTEX))
	{
		m_vertexShaders[name] = std::move(info);
		std::wcout << L"Loaded vertex shader: " << name.c_str() << " " << filename << std::endl;
		return true;
	}

	std::wcout << L"Failed to load vertex shader: " << name.c_str() << " " << filename << std::endl;
	return false;
}

bool ShaderManager::LoadPixelShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint)
{
	if (m_pixelShaders.contains(name))
	{
		std::wcout << L"Pixel shader already exists: " << name.c_str() << std::endl;
		return true;
	}

	ShaderInfo info;
	info.filename = filename;

	info.entryPoint = entryPoint;
	info.shaderModelVersion = "ps_5_0";
	info.lastModifiedTime = getFileModifiedTime(filename);

	if (compileShader(info, PIXEL))
	{
		m_pixelShaders[name] = std::move(info);
		std::wcout << L"Loaded pixel shader: " << name.c_str() << " " << filename << std::endl;
		return true;
	}

	std::wcout << L"Failed to load pixel shader: " << name.c_str() << " " << filename << std::endl;
	return false;
}

bool ShaderManager::LoadComputeShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint)
{
	if (m_computeShaders.contains(name))
	{
		std::wcout << L"Compute shader already exists: " << name.c_str() << std::endl;
		return true;
	}
	ShaderInfo info;
	info.filename = filename;

	info.entryPoint = entryPoint;
	info.shaderModelVersion = "cs_5_0";
	info.lastModifiedTime = getFileModifiedTime(filename);

	if (compileShader(info, COMPUTE))
	{
		m_computeShaders[name] = std::move(info);
		std::wcout << L"Loaded compute shader: " << name.c_str() << " " << filename << std::endl;
		return true;
	}

	std::wcout << L"Failed to load compute shader: " << name.c_str() << " " << filename << std::endl;
	return false;
}

ID3D11VertexShader* ShaderManager::getVertexShader(const std::string& name)
{
	auto it = m_vertexShaders.find(name);
	return (it != m_vertexShaders.end()) ? it->second.vertexShader.Get() : nullptr;
}

ID3D11PixelShader* ShaderManager::getPixelShader(const std::string& name)
{
	auto it = m_pixelShaders.find(name);
	return (it != m_pixelShaders.end()) ? it->second.pixelShader.Get() : nullptr;
}

ID3D11ComputeShader* ShaderManager::getComputeShader(const std::string& name)
{
	auto it = m_computeShaders.find(name);
	return (it != m_computeShaders.end()) ? it->second.computeShader.Get() : nullptr;
}

ID3DBlob* ShaderManager::getVertexShaderBlob(const std::string& name)
{
	auto it = m_vertexShaders.find(name);
	return (it != m_vertexShaders.end()) ? it->second.blob.Get() : nullptr;
}

void ShaderManager::checkForChanges()
{
	// Check vertex shaders
	for (auto& info : m_vertexShaders | std::views::values)
	{
		auto currentTime = getFileModifiedTime(info.filename);
		if (currentTime > info.lastModifiedTime)
		{
			std::wcout << L"Recompiling vertex shader: " << info.filename << std::endl;
			if (compileShader(info, VERTEX))
			{
				info.lastModifiedTime = currentTime;
				std::wcout << L"Successfully recompiled vertex shader: " << info.filename << std::endl;
			}
			else
			{
				std::wcout << L"Failed to recompile vertex shader: " << info.filename << std::endl;
			}
		}
	}

	// Check pixel shaders
	for (auto& info : m_pixelShaders | std::views::values)
	{
		auto currentTime = getFileModifiedTime(info.filename);
		if (currentTime > info.lastModifiedTime)
		{
			std::wcout << L"Recompiling pixel shader: " << info.filename << std::endl;
			if (compileShader(info, PIXEL))
			{
				info.lastModifiedTime = currentTime;
				std::wcout << L"Successfully recompiled pixel shader: " << info.filename << std::endl;
			}
			else
			{
				std::wcout << L"Failed to recompile pixel shader: " << info.filename << std::endl;
			}
		}
	}

	// Check compute shaders
	for (auto& info : m_computeShaders | std::views::values)
	{
		auto currentTime = getFileModifiedTime(info.filename);
		if (currentTime > info.lastModifiedTime)
		{
			std::wcout << L"Recompiling compute shader: " << info.filename << std::endl;
			if (compileShader(info, COMPUTE))
			{
				info.lastModifiedTime = currentTime;
				std::wcout << L"Successfully recompiled compute shader: " << info.filename << std::endl;
			}
			else
			{
				std::wcout << L"Failed to recompile compute shader: " << info.filename << std::endl;
			}
		}
	}
}

void ShaderManager::recompileAll()
{
	std::cout << "Recompiling all shaders..." << std::endl;

	for (auto& info : m_vertexShaders | std::views::values)
	{
		compileShader(info, VERTEX);
		info.lastModifiedTime = getFileModifiedTime(info.filename);
	}

	for (auto& info : m_pixelShaders | std::views::values)
	{
		compileShader(info, PIXEL);
		info.lastModifiedTime = getFileModifiedTime(info.filename);
	}

	for (auto& info : m_computeShaders | std::views::values)
	{
		compileShader(info, COMPUTE);
		info.lastModifiedTime = getFileModifiedTime(info.filename);
	}
}


bool ShaderManager::compileShader(ShaderInfo& info, const ShaderType shaderType) const
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	// Debug: Check if file exists
	if (!std::filesystem::exists(info.filename))
	{
		std::wcerr << L"Shader file not found: " << info.filename << std::endl;
		std::wcerr << L"Current working directory: " << std::filesystem::current_path().wstring() << std::endl;
		return false;
	}

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompileFromFile(
		info.filename.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		info.entryPoint.c_str(),
		info.shaderModelVersion.c_str(),
		flags,
		0,
		&shaderBlob,
		&errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			std::cout << "Shader compilation error:\n"
				<< static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
		}
		return false;
	}

	if (shaderType == VERTEX)
	{
		hr = m_device->CreateVertexShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&info.vertexShader);
	}
	else if (shaderType == PIXEL)
	{
		hr = m_device->CreatePixelShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&info.pixelShader);
	}
	else if (shaderType == COMPUTE)
	{
		hr = m_device->CreateComputeShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&info.computeShader);
	}

	if (SUCCEEDED(hr))
	{
		info.blob = shaderBlob;
		return true;
	}

	return false;
}


std::filesystem::file_time_type ShaderManager::getFileModifiedTime(const std::wstring& filename)
{
	try
	{
		return std::filesystem::last_write_time(filename);
	}
	catch (const std::filesystem::filesystem_error&)
	{
		return (std::filesystem::file_time_type::min)();
	}
}
