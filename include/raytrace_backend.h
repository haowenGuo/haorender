#pragma once

#include "model.h"
#include <memory>

class RayTracingBackend {
public:
	virtual ~RayTracingBackend() = default;
	virtual bool available() const = 0;
	virtual const char* backendName() const = 0;
	virtual bool build(const Model& model, const Matrix4f& modelMatrix) = 0;
	virtual bool occludedDirectional(const Vector3f& origin, const Vector3f& lightDir, float tnear, float tfar) const = 0;
};

std::shared_ptr<RayTracingBackend> createRayTracingBackend();
