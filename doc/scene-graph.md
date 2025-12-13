# Scene Graph System

Hierarchical scene management with transform propagation.

## Overview

TimeToDX uses a scene graph where all objects inherit from `SceneNode`. Each node has a transform that is relative to its parent, and the world transform is computed by traversing up the hierarchy.

## Class Hierarchy

```
SceneNode (base)
 Primitive   - Renderable mesh with material
 Light       - Light source (point/directional/spot)
 Camera      - Viewpoint for rendering
 Scene       - Root container (special SceneNode)
```

## SceneNode

Base class for all scene objects.

### Properties
```cpp
class SceneNode {
public:
    Transform transform;        // Local transform
    std::string name;           // Display name
    bool visible = true;        // Render visibility
    bool dirty = true;          // Needs update
    bool movable = true;        // Can be transformed

    SceneNode* parent = nullptr;
    std::list<std::unique_ptr<SceneNode>> children;
};
```

### Key Methods
```cpp
// Get world-space transform matrix
glm::mat4 getWorldMatrix();

// Add child (transfers ownership)
void addChild(std::unique_ptr<SceneNode>&& child);

// Remove child (returns ownership)
std::unique_ptr<SceneNode> removeChild(SceneNode* child);
```

### World Matrix Calculation
```cpp
glm::mat4 SceneNode::getWorldMatrix() {
    transform.updateMatrix();
    if (parent) {
        return parent->getWorldMatrix() * transform.matrix;
    }
    return transform.matrix;
}
```

## Transform

Local transform with position, rotation, scale.

```cpp
struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);  // Euler degrees
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 matrix = glm::mat4(1.0f);
    glm::mat4 prevMatrix = glm::mat4(1.0f);

    void updateMatrix();
    void translate(const glm::vec3& translation);
    void rotate(const glm::vec3& rotation);
    void setScale(const glm::vec3& scale);
};
```

### Matrix Composition
```cpp
void Transform::updateMatrix() {
    prevMatrix = matrix;
    
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    
    glm::mat4 R = glm::mat4(1.0f);
    R = glm::rotate(R, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    R = glm::rotate(R, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    R = glm::rotate(R, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    
    matrix = T * R * S;  // TRS order
}
```

## Scene (Root Container)

Special SceneNode that acts as the root and manages resources.

### Resource Management
```cpp
class Scene : public SceneNode {
    // Flat lists for iteration
    std::vector<Primitive*> m_primitives;
    std::vector<Light*> m_lights;
    std::vector<Camera*> m_cameras;
    
    // Resource maps
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materials;
    
    // Selection
    SceneNode* m_activeNode = nullptr;
    std::unordered_map<SceneNode*, bool> m_selectedNodes;
    Camera* m_activeCamera = nullptr;
};
```

### Key Methods
```cpp
// Resource access
std::shared_ptr<Texture> getTexture(std::string name);
std::shared_ptr<Material> getMaterial(std::string name);

// Object access
std::vector<Primitive*>& getPrimitives();
std::vector<Light*>& getLights();
Primitive* getPrimitiveByID(size_t id);

// Selection
void setActiveNode(SceneNode* node, bool addToSelection = false);
void clearSelectedNodes();
bool isNodeSelected(SceneNode* node);
```

## Primitive

Renderable mesh with vertex/index buffers.

```cpp
class Primitive : public SceneNode {
public:
    std::shared_ptr<Material> material;
    
    // GPU resources
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    
    void draw(ID3D11DeviceContext* context);
    
private:
    std::vector<InterleavedData> m_vertexData;
    std::vector<uint32_t> m_indexData;
};
```

## Light

Light source with type-specific properties.

```cpp
enum LightType { POINT_LIGHT, DIRECTIONAL_LIGHT, SPOT_LIGHT };

class Light : public SceneNode {
public:
    LightType type = POINT_LIGHT;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float radius = 1.0f;
    glm::vec3 direction = glm::vec3(0, -1, 0);
    glm::vec3 attenuation = glm::vec3(1, 0, 0);  // Constant, linear, quadratic
    glm::vec2 spotParams = glm::vec2(0);  // Inner/outer angles
};
```

## Camera

Orbit camera with Blender-style controls.

```cpp
class Camera : public SceneNode {
public:
    glm::vec3 front, up, right, worldUp;
    glm::vec3 orbitPivot;
    float fov = 45.0f;
    float distanceToOrbitPivot = 5.0f;
    
    glm::mat4 getViewMatrix();
    void processOrbit();
    void processPanning();
    void processZoom();
    void updateCameraVectors();
};
```

## Reparenting

When a node is reparented, its world transform is preserved:

```cpp
void SceneNode::addChild(std::unique_ptr<SceneNode>&& child) {
    // Save world transform before reparenting
    glm::mat4 worldTransform = child->getWorldMatrix();
    
    // Remove from old parent
    if (child->parent) {
        child->parent->removeChild(child.get());
    }
    
    // Set new parent
    child->parent = this;
    children.push_back(std::move(child));
    
    // Compute new local transform to maintain world position
    glm::mat4 parentWorldInverse = glm::inverse(this->getWorldMatrix());
    glm::mat4 newLocalMatrix = parentWorldInverse * worldTransform;
    
    // Decompose and apply
    glm::vec3 scale, position, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(newLocalMatrix, scale, rotation, position, skew, perspective);
    
    child->transform.position = position;
    child->transform.rotation = glm::degrees(glm::eulerAngles(rotation));
    child->transform.scale = scale;
}
```
