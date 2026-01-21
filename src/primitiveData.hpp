#pragma once

#include <d3d11_4.h>
#include <glm/glm.hpp>

using Position = glm::vec3;
using Normal = glm::vec3;
using TexCoords = glm::vec2;

struct Vertex
{
	Position position;
	Normal normal;
	TexCoords texCoords;
	Vertex() = default;

	Vertex(const Position position_, const Normal normal_, const TexCoords texCoords_)
		: position(position_)
		, normal(normal_)
		, texCoords(texCoords_) {};
};


struct Triangle
{
	Position v0;
	float pad0;
	Position v1;
	float pad1;
	Position v2;
	float pad2;
	Normal n0;  // vertex normal at v0
	float pad3;
	Normal n1;  // vertex normal at v1
	float pad4;
	Normal n2;  // vertex normal at v2
	float pad5;
	Position Center() const
	{
		return (v0 + v1 + v2) / 3.0f;
	}
};
