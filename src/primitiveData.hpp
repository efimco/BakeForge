#pragma once

#include <d3d11_4.h>
#include <bvh/v2/vec.h>

using Vec3 = bvh::v2::Vec<float, 3>;

struct Position
{
	float x, y, z;

	explicit operator Vec3() const
	{
		return Vec3{x, y, z};
	}

	Position() = default;

	Position(const float x_, const float y_, const float z_)
		: x(x_)
		, y(y_)
		, z(z_)
	{
	};
};

struct TexCoords
{
	float u, v;

	TexCoords() = default;

	TexCoords(const float u_, const float v_)
		: u(u_)
		, v(v_)
	{
	};
};

struct Normals
{
	float nx, ny, nz;
	Normals() = default;

	Normals(const float nx_, const float ny_, float nz_);;
};

inline Normals::Normals(const float nx_, const float ny_, const float nz_)
	: nx(nx_)
	, ny(ny_)
	, nz(nz_)
{
}

struct InterleavedData
{
	Position position;
	TexCoords texCoords;
	Normals normals;
	InterleavedData() = default;

	InterleavedData(const Position position_, const TexCoords texCoords_, const Normals normals_)
		: position(position_)
		, texCoords(texCoords_)
		, normals(normals_)
	{
	};

};
