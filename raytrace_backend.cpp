#include "raytrace_backend.h"

#include <limits>
#include <vector>
#include <array>

#ifdef HAO_RENDER_EMBREE
#include <embree4/rtcore.h>

namespace {
struct EmbreeVertex {
	float x;
	float y;
	float z;
};

struct EmbreeTriangle {
	unsigned int v0;
	unsigned int v1;
	unsigned int v2;
};

class EmbreeRayTracingBackend final : public RayTracingBackend {
public:
	EmbreeRayTracingBackend() {
		device = rtcNewDevice(nullptr);
		if (device) {
			scene = rtcNewScene(device);
		}
	}

	~EmbreeRayTracingBackend() override {
		if (scene) {
			rtcReleaseScene(scene);
		}
		if (device) {
			rtcReleaseDevice(device);
		}
	}

	bool available() const override {
		return device != nullptr && scene != nullptr;
	}

	const char* backendName() const override {
		return "Embree";
	}

	bool build(const Model& model, const Matrix4f& modelMatrix) override {
		if (!available()) {
			return false;
		}

		mesh_buffers.clear();
		if (scene) {
			rtcReleaseScene(scene);
		}
		scene = rtcNewScene(device);
		if (!scene) {
			return false;
		}

		for (const Mesh& mesh : model.meshes) {
			if (mesh.vertexCount() == 0 || mesh.indices.size() < 3) {
				continue;
			}

			MeshBuffers buffers;
			buffers.vertices.reserve(mesh.vertexCount());
			for (size_t i = 0; i < mesh.vertexCount(); ++i) {
				Vector4f p = modelMatrix * mesh.vertexPosition(i);
				buffers.vertices.push_back({ p[0], p[1], p[2] });
			}

			buffers.triangles.reserve(mesh.indices.size() / 3);
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
				buffers.triangles.push_back(
					{ static_cast<unsigned int>(mesh.indices[i]),
					  static_cast<unsigned int>(mesh.indices[i + 1]),
					  static_cast<unsigned int>(mesh.indices[i + 2]) });
			}

			RTCGeometry geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
			if (!geometry) {
				continue;
			}

			rtcSetSharedGeometryBuffer(
				geometry,
				RTC_BUFFER_TYPE_VERTEX,
				0,
				RTC_FORMAT_FLOAT3,
				buffers.vertices.data(),
				0,
				sizeof(EmbreeVertex),
				buffers.vertices.size());

			rtcSetSharedGeometryBuffer(
				geometry,
				RTC_BUFFER_TYPE_INDEX,
				0,
				RTC_FORMAT_UINT3,
				buffers.triangles.data(),
				0,
				sizeof(EmbreeTriangle),
				buffers.triangles.size());

			rtcCommitGeometry(geometry);
			rtcAttachGeometry(scene, geometry);
			rtcReleaseGeometry(geometry);
			mesh_buffers.push_back(std::move(buffers));
		}

		rtcCommitScene(scene);
		return !mesh_buffers.empty();
	}

	bool occludedDirectional(const Vector3f& origin, const Vector3f& lightDir, float tnear, float tfar) const override {
		if (!available()) {
			return false;
		}

		RTCRay ray{};
		ray.org_x = origin.x();
		ray.org_y = origin.y();
		ray.org_z = origin.z();
		ray.dir_x = lightDir.x();
		ray.dir_y = lightDir.y();
		ray.dir_z = lightDir.z();
		ray.tnear = tnear;
		ray.tfar = tfar;
		ray.time = 0.0f;
		ray.mask = 0xFFFFFFFFu;
		ray.id = 0u;
		ray.flags = 0u;

		RTCRayQueryContext context;
		rtcInitRayQueryContext(&context);
		RTCOccludedArguments args;
		rtcInitOccludedArguments(&args);
		args.context = &context;
		rtcOccluded1(scene, &ray, &args);
		return ray.tfar < 0.0f;
	}

private:
	struct MeshBuffers {
		std::vector<EmbreeVertex> vertices;
		std::vector<EmbreeTriangle> triangles;
	};

	RTCDevice device = nullptr;
	RTCScene scene = nullptr;
	std::vector<MeshBuffers> mesh_buffers;
};
}

std::shared_ptr<RayTracingBackend> createRayTracingBackend() {
	return std::make_shared<EmbreeRayTracingBackend>();
}

#else

namespace {
class NullRayTracingBackend final : public RayTracingBackend {
public:
	bool available() const override { return false; }
	const char* backendName() const override { return "Disabled"; }
	bool build(const Model&, const Matrix4f&) override { return false; }
	bool occludedDirectional(const Vector3f&, const Vector3f&, float, float) const override { return false; }
};
}

std::shared_ptr<RayTracingBackend> createRayTracingBackend() {
	return std::make_shared<NullRayTracingBackend>();
}

#endif
