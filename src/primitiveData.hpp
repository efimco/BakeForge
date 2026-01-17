#pragma once

#include <d3d11_4.h>
#include <glm/glm.hpp>

struct Position
{
	float x, y, z;


	Position() = default;
	Position(const float x_, const float y_, const float z_)
		: x(x_)
		, y(y_)
		, z(z_) {};

	operator glm::vec3() const
	{
		return glm::vec3(x, y, z);
	}
};

struct TexCoords
{
	float u, v;

	TexCoords() = default;

	TexCoords(const float u_, const float v_)
		: u(u_)
		, v(v_) {};
};

struct Normal
{
	float x, y, z;
	Normal() = default;

	Normal(const float nx_, const float ny_, float nz_) : x(nx_)
														, y(ny_)
														, z(nz_) {};
	operator glm::vec3() const
	{
		return glm::vec3(x, y, z);
	}
};


struct Vertex
{
	Position position;
	TexCoords texCoords;
	Normal normal;
	Vertex() = default;

	Vertex(const Position position_, const TexCoords texCoords_, const Normal normals_)
		: position(position_)
		, texCoords(texCoords_)
		, normal(normals_) {};
};

struct Triangle
{
	Triangle(const Position p0, const Position p1, const Position p2)
		: v0(p0), v1(p1), v2(p2) {};
	Position v0, v1, v2;
	glm::vec3 normal;
	glm::vec3 center;
};
