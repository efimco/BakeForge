#pragma once
#include "primitiveData.hpp"
#include <vector>
#include "primitive.hpp"
#include "texture.hpp"
#include <memory>
#include "material.hpp"

namespace SceneManager
{
	std::unique_ptr<Primitive>& addPrimitive(std::unique_ptr<Primitive>&& primitive);
	std::vector<std::unique_ptr<Primitive>>& getPrimitives();
	size_t getPrimitiveCount();
	std::shared_ptr<Texture> getTexture(std::string name);
	void addTexture(std::shared_ptr<Texture>&& texture);
	std::shared_ptr<Material> getMaterial(std::string name);
	void addMaterial(std::shared_ptr<Material>&& material);

	void selectPrimitive(Primitive* primitive);
	void selectPrimitive(uint32_t id);
	void deselectPrimitive(Primitive* primitive);
	void deselectPrimitive(uint32_t id);
	bool isPrimitiveSelected(Primitive* primitive);
	void clearSelectedPrimitives();

};