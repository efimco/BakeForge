#include "DebugBVHPass.hpp"

#include <iostream>
#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

#include "scene.hpp"
#include "shaderManager.hpp"
#include "primitive.hpp"


struct alignas(16) DebugBVHConstantBuffer
{
	glm::mat4 viewProjection;
};

DebugBVHPass::DebugBVHPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	m_device = device;
	m_context = context;
	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_shaderManager->LoadVertexShader("debugBVH", L"..\\..\\src\\shaders\\debugBVH.hlsl", "VS");
	m_shaderManager->LoadPixelShader("debugBVH", L"..\\..\\src\\shaders\\debugBVH.hlsl", "PS");

	// Create dynamic vertex buffer
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth = static_cast<UINT>(m_maxVertices * sizeof(DebugLineVertex));
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT hr = m_device->CreateBuffer(&vbDesc, nullptr, &m_vertexBuffer);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create debug BVH vertex buffer" << std::endl;
	}

	// Create constant buffer
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.ByteWidth = sizeof(DebugBVHConstantBuffer);
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create debug BVH constant buffer" << std::endl;
	}

	// Create input layout
	constexpr D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
		, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
		,};
	if (const auto vsBlob = m_shaderManager->getVertexShaderBlob("debugBVH"))
	{
		hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()
		                                 , &m_inputLayout);
		if (FAILED(hr))
		{
			std::cerr << "Failed to create debug BVH input layout" << std::endl;
		}
	}

	// Wireframe rasterizer - no culling for debug viz
	D3D11_RASTERIZER_DESC rsDesc = {};
	rsDesc.FillMode = D3D11_FILL_WIREFRAME;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.FrontCounterClockwise = FALSE;
	rsDesc.ScissorEnable = FALSE;
	rsDesc.MultisampleEnable = FALSE;
	rsDesc.AntialiasedLineEnable = TRUE;
	m_device->CreateRasterizerState(&rsDesc, &m_rasterizerState);

	// Depth stencil - read depth but don't write
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dsDesc.StencilEnable = FALSE;
	m_device->CreateDepthStencilState(&dsDesc, &m_depthStencilState);

}

void DebugBVHPass::addBoxLines(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
{
	// 8 corners of the box
	glm::vec3 corners[8] = {{min.x, min.y, min.z}
	                        , {max.x, min.y, min.z}
	                        , {min.x, max.y, min.z}
	                        , {max.x, max.y, min.z}
	                        , {min.x, min.y, max.z}
	                        , {max.x, min.y, max.z}
	                        , {min.x, max.y, max.z}
	                        , {max.x, max.y, max.z}
	                        ,};

	// 12 edges (each edge = 2 vertices for line list)
	const int edges[12][2] = {{0, 1}
	                          , {2, 3}
	                          , {4, 5}
	                          , {6, 7}
	                          ,
	                          // X edges
	                          {0, 2}
	                          , {1, 3}
	                          , {4, 6}
	                          , {5, 7}
	                          ,
	                          // Y edges
	                          {0, 4}
	                          , {1, 5}
	                          , {2, 6}
	                          , {3, 7}
	                          ,
	                          // Z edges
	};

	for (int i = 0; i < 12; ++i)
	{
		m_vertices.push_back({corners[edges[i][0]], color});
		m_vertices.push_back({corners[edges[i][1]], color});
	}
}

void DebugBVHPass::addPrimitiveBVHBoxes(Primitive* primitive, int maxDepth)
{
	const Bvh* bvh = primitive->getBVH();
	if (!bvh || bvh->nodes.empty())
		return;

	glm::mat4 worldMatrix = primitive->getWorldMatrix();

	std::vector<std::pair<size_t, int>> stack;
	stack.push_back({0, 0});

	while (!stack.empty())
	{
		auto [nodeIdx, depth] = stack.back();
		stack.pop_back();

		if (nodeIdx >= bvh->nodes.size())
			continue;
		if (maxDepth >= 0 && depth > maxDepth)
			continue;

		const Node& node = bvh->nodes[nodeIdx];

		// Color: cyan/magenta gradient for primitive BVH (different from scene BVH)
		float t = std::min(depth / 10.0f, 1.0f);
		glm::vec4 color;
		if (node.is_leaf())
		{
			color = glm::vec4(0.0f, 1.0f, 1.0f, 0.5f); // Cyan for leaves
		}
		else
		{
			color = glm::vec4(1.0f, t, 1.0f - t, 0.4f); // Magenta to blue gradient
		}

		// Get local bounds
		glm::vec3 localMin(node.bounds[0], node.bounds[2], node.bounds[4]);
		glm::vec3 localMax(node.bounds[1], node.bounds[3], node.bounds[5]);

		// Transform all 8 corners to world space and compute new AABB
		glm::vec3 corners[8] = {{localMin.x, localMin.y, localMin.z}
		                        , {localMax.x, localMin.y, localMin.z}
		                        , {localMin.x, localMax.y, localMin.z}
		                        , {localMax.x, localMax.y, localMin.z}
		                        , {localMin.x, localMin.y, localMax.z}
		                        , {localMax.x, localMin.y, localMax.z}
		                        , {localMin.x, localMax.y, localMax.z}
		                        , {localMax.x, localMax.y, localMax.z}
		                        ,};

		glm::vec3 worldMin(FLT_MAX);
		glm::vec3 worldMax(-FLT_MAX);

		for (int i = 0; i < 8; ++i)
		{
			glm::vec4 worldCorner = worldMatrix * glm::vec4(corners[i], 1.0f);
			worldMin = glm::min(worldMin, glm::vec3(worldCorner));
			worldMax = glm::max(worldMax, glm::vec3(worldCorner));
		}

		addBoxLines(worldMin, worldMax, color);

		if (!node.is_leaf())
		{
			size_t firstChild = node.index.first_id();
			stack.push_back({firstChild, depth + 1});
			stack.push_back({firstChild + 1, depth + 1});
		}

		if (m_vertices.size() > m_maxVertices - 24)
		{
			break;
		}
	}
}

void DebugBVHPass::updateVertexBuffer(Scene* scene, const int maxDepth)
{
	m_vertices.clear();

	const Bvh* sceneBvh = scene->getSceneBVH();
	if (!sceneBvh || sceneBvh->nodes.empty())
	{
		return;
	}

	// Traverse scene BVH using a stack and add boxes
	// Color by depth: root = red, deeper = green/blue
	std::vector<std::pair<size_t, int>> stack; // node index, depth
	stack.push_back({0, 0});

	int maxFoundDepth = 0;

	while (!stack.empty())
	{
		auto [nodeIdx, depth] = stack.back();
		stack.pop_back();

		if (nodeIdx >= sceneBvh->nodes.size())
			continue;

		maxFoundDepth = std::max(maxFoundDepth, depth);

		// Skip if beyond max depth (unless maxDepth is -1, meaning show all)
		if (maxDepth >= 0 && depth > maxDepth)
			continue;

		const Node& node = sceneBvh->nodes[nodeIdx];

		// Color based on depth - lerp from red (root) to cyan (deep)
		const float t = std::min(depth / 8.0f, 1.0f);
		glm::vec4 color;
		if (depth == 0)
		{
			color = glm::vec4(1.0f, 0.0f, 0.0f, 0.8f); // Red for root
		}
		else if (node.is_leaf())
		{
			color = glm::vec4(0.0f, 1.0f, 0.0f, 0.6f); // Green for leaves
		}
		else
		{
			// Interpolate yellow -> cyan for intermediate nodes
			color = glm::vec4(1.0f - t, 0.8f, t, 0.5f);
		}

		// Node bounds are stored as [minX, maxX, minY, maxY, minZ, maxZ]
		glm::vec3 bmin(node.bounds[0], node.bounds[2], node.bounds[4]);
		glm::vec3 bmax(node.bounds[1], node.bounds[3], node.bounds[5]);

		addBoxLines(bmin, bmax, color);

		// If not leaf, add children to stack
		if (!node.is_leaf())
		{
			size_t firstChild = node.index.first_id();
			stack.push_back({firstChild, depth + 1});
			stack.push_back({firstChild + 1, depth + 1});
		}

		// Safety check to avoid infinite loops or excessive memory
		if (m_vertices.size() > m_maxVertices - 24)
		{
			std::cerr << "Debug BVH: Maximum vertex count reached, truncating" << std::endl;
			break;
		}
	}

	// Also draw per-primitive BVHs if enabled
	if (m_showPrimitiveBVH)
	{
		for (auto& [handle, prim] : scene->getPrimitives())
		{
			addPrimitiveBVHBoxes(prim, maxDepth);
			if (m_vertices.size() > m_maxVertices - 24)
				break;
		}
	}
}

void DebugBVHPass::draw(const glm::mat4& view
                        , const glm::mat4& projection
                        , Scene* scene
                        , const ComPtr<ID3D11RenderTargetView>& rtv
                        , const ComPtr<ID3D11DepthStencilView>& dsv
                        , const int maxDepth)
{
	if (!m_enabled)
		return;


	// Use passed maxDepth or member variable
	const int depth = (maxDepth >= 0) ? maxDepth : m_maxDepth;
	updateVertexBuffer(scene, depth);

	if (m_vertices.empty())
	{

		return;
	}

	// Upload vertices to GPU
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, m_vertices.data(), m_vertices.size() * sizeof(DebugLineVertex));
		m_context->Unmap(m_vertexBuffer.Get(), 0);
	}

	// Update constant buffer
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		const auto cb = static_cast<DebugBVHConstantBuffer*>(mapped.pData);
		cb->viewProjection = projection * view;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	// Set pipeline state
	m_context->RSSetState(m_rasterizerState.Get());
	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	// Disable blending - we're drawing opaque wireframe lines
	m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	m_context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());

	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	constexpr UINT stride = sizeof(DebugLineVertex);
	constexpr UINT offset = 0;
	m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

	m_context->VSSetShader(m_shaderManager->getVertexShader("debugBVH"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("debugBVH"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	m_context->Draw(static_cast<UINT>(m_vertices.size()), 0);

	// Unbind render targets to avoid resource hazards
	ID3D11RenderTargetView* nullRTVs = nullptr;
	m_context->OMSetRenderTargets(1, &nullRTVs, nullptr);


}
