#pragma once

#include <d3d11_4.h>
#include <bvh/v2/vec.h>

using Vec3 = bvh::v2::Vec<float, 3>;

struct Position
{
	float x, y, z;
	operator Vec3() const { return Vec3{ x, y, z }; }
	Position() = default;
	Position(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {};
};

struct TexCoords
{
	float u, v;

	TexCoords() = default;
	TexCoords(float _u, float _v) : u(_u), v(_v) {};
};

struct Normals
{
	float nx, ny, nz;
	Normals() = default;
	Normals(float _nx, float _ny, float _nz) : nx(_nx), ny(_ny), nz(_nz) {};
};

struct InterleavedData
{
	Position position;
	TexCoords texCoords;
	Normals normals;
	InterleavedData() = default;
	InterleavedData(Position _position,
		TexCoords _texCoords,
		Normals _normals)
		: position(_position),
		texCoords(_texCoords),
		normals(_normals) {
	};

};