#pragma once

#include <d3d11_4.h>
#include <wrl.h>

#include "sceneNode.hpp"

using namespace Microsoft::WRL;

class BakerPass;

class BakerNode : public SceneNode
{
public:
	explicit BakerNode() = default;
	~BakerNode() override = default;
};

class LowPolyNode : public BakerNode
{
public:
	explicit LowPolyNode(const std::string_view nodeName);
	float cageOffset;
	~LowPolyNode() override = default;
};

class HighPolyNode : public BakerNode
{
public:
	explicit HighPolyNode(const std::string_view nodeName);
	~HighPolyNode() override = default;
};

class Baker : public BakerNode
{
public:
	explicit Baker(std::string_view nodeName, ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~Baker() override = default;

	void bake();

	std::unique_ptr<LowPolyNode> lowPoly;
	std::unique_ptr<HighPolyNode> highPoly;

	uint32_t textureWidth;

private:
	std::unique_ptr<BakerPass> m_bakerPass;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
};
