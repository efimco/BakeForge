#include "material.hpp"
#include "texture.hpp"

ID3D11ShaderResourceView* const* Material::getSRVs()
{
	m_srvCache[0] = albedo ? albedo->srv.Get() : nullptr;
	m_srvCache[1] = metallicRoughness ? metallicRoughness->srv.Get() : nullptr;
	m_srvCache[2] = normal ? normal->srv.Get() : nullptr;
	return m_srvCache;
}
