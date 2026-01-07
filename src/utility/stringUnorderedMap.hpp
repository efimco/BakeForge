#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

// Defines how to hash and compare std::string and std::string_view.
// Necessary so that std::string and std::string_view
// could be used interchangeably as a lookup key (heterogeneous lookup).
// Example:
//
// std::unordered_map<std::string, int, TransparentStringHash, ...> map;
// std::string_view key = "key";
// x.emplace(key, 1); // this function would not accept std::string_view otherwise
//

struct TransparentStringHash
{
	using is_transparent = void;

	size_t operator()(const std::string_view value) const noexcept
	{
		return std::hash<std::string_view>{}(value);
	}

	size_t operator()(const std::string& value) const noexcept
	{
		return std::hash<std::string>{}(value);
	}
};

struct TransparentStringEqual
{
	using is_transparent = void;

	bool operator()(const std::string_view first, const std::string_view second) const noexcept
	{
		return first == second;
	}
};

template <typename T>
using StringUnorderedMap = std::unordered_map<std::string, T, TransparentStringHash, TransparentStringEqual>;
