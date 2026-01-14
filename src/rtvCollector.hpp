#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class RTVCollector
{
public:
	explicit RTVCollector() = default;
	~RTVCollector() = default;
	void addRTV(const std::string& passName, ID3D11ShaderResourceView* rtv);
	std::unordered_map<std::string, ID3D11ShaderResourceView*>& getRTVMap();

private:
	static std::unordered_map<std::string, ID3D11ShaderResourceView*> rtvMap; // (passName, RTV)
};
