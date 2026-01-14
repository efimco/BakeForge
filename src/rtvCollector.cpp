#include "rtvCollector.hpp"

std::unordered_map<std::string, ID3D11ShaderResourceView*> RTVCollector::rtvMap = {};

void RTVCollector::addRTV(const std::string& passName, ID3D11ShaderResourceView* rtv)
{
	rtvMap[passName] = rtv;
}

std::unordered_map<std::string, ID3D11ShaderResourceView*>& RTVCollector::getRTVMap()
{
	return rtvMap;
}
