
#include"shader.h"
#include "raytrace_backend.h"
#include <drawer.h>
#include <algorithm>
#include <cmath>
#include <limits>
Shader::~Shader() {};

namespace {
double durationMsLocal(const std::chrono::high_resolution_clock::time_point& start,
	const std::chrono::high_resolution_clock::time_point& end) {
	return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();
}

float wrap01(float value) {
	if (!std::isfinite(value)) {
		return 0.f;
	}
	if (value >= 0.f && value <= 1.f) {
		return value;
	}
	value = std::fmod(value, 1.f);
	return value < 0.f ? value + 1.f : value;
}

float channelValue(const MyColor& color, int channel) {
	switch (channel) {
	case 0: return static_cast<float>(color.r);
	case 1: return static_cast<float>(color.g);
	case 2: return static_cast<float>(color.b);
	case 3: return static_cast<float>(color.a);
	default: return static_cast<float>(color.r);
	}
}

float channelValue01(const MyColor& color, int channel) {
	return std::clamp(channelValue(color, channel) / 255.0f, 0.0f, 1.0f);
}

int textureCoord(float value, int size) {
	if (size <= 1) {
		return 0;
	}
	return std::clamp(static_cast<int>(wrap01(value) * static_cast<float>(size - 1)), 0, size - 1);
}

MyColor sampleTexture(const vector<MyImage>& textures, int textureIndex, float u, float v) {
	if (textureIndex < 0 || textureIndex >= static_cast<int>(textures.size())) {
		return MyColor();
	}
	const MyImage& texture = textures[textureIndex];
	return texture.get(textureCoord(u, texture.get_width()), textureCoord(v, texture.get_height()));
}

unsigned char clampColor(float value) {
	return static_cast<unsigned char>(std::clamp(value, 0.f, 255.f));
}

Vector3f srgbToLinear(const Vector3f& c) {
	Vector3f out;
	out[0] = std::pow(std::clamp(c[0], 0.0f, 1.0f), 2.2f);
	out[1] = std::pow(std::clamp(c[1], 0.0f, 1.0f), 2.2f);
	out[2] = std::pow(std::clamp(c[2], 0.0f, 1.0f), 2.2f);
	return out;
}

Vector3f linearToSrgb(const Vector3f& c) {
	Vector3f out;
	out[0] = std::pow(std::clamp(c[0], 0.0f, 1.0f), 1.0f / 2.2f);
	out[1] = std::pow(std::clamp(c[1], 0.0f, 1.0f), 1.0f / 2.2f);
	out[2] = std::pow(std::clamp(c[2], 0.0f, 1.0f), 1.0f / 2.2f);
	return out;
}

Vector3f tonemapReinhard(const Vector3f& c) {
	Vector3f out;
	out[0] = c[0] / (1.0f + c[0]);
	out[1] = c[1] / (1.0f + c[1]);
	out[2] = c[2] / (1.0f + c[2]);
	return out;
}

float quantizeToonDiffuse(float value) {
	if (value <= 0.0f) {
		return 0.0f;
	}
	if (value < 0.22f) {
		return 0.12f;
	}
	if (value < 0.5f) {
		return 0.38f;
	}
	if (value < 0.78f) {
		return 0.68f;
	}
	return 1.0f;
}

Vector3f colorToRgb01(const MyColor& c) {
	if (c.channels >= 3) {
		return Vector3f(
			static_cast<float>(c.r) / 255.0f,
			static_cast<float>(c.g) / 255.0f,
			static_cast<float>(c.b) / 255.0f);
	}
	float value = channelValue01(c, 0);
	return Vector3f(value, value, value);
}

Vector3f sampleTextureLinearRgb(const MyImage& image, float u, float v) {
	if (!image.data || image.width <= 0 || image.height <= 0) {
		return Vector3f::Zero();
	}
	float fx = wrap01(u) * static_cast<float>(std::max(image.width - 1, 1));
	float fy = wrap01(v) * static_cast<float>(std::max(image.height - 1, 1));
	int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, image.width - 1);
	int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, image.height - 1);
	int x1 = std::min(x0 + 1, image.width - 1);
	int y1 = std::min(y0 + 1, image.height - 1);
	float tx = fx - static_cast<float>(x0);
	float ty = fy - static_cast<float>(y0);
	Vector3f c00 = colorToRgb01(image.get(x0, y0));
	Vector3f c10 = colorToRgb01(image.get(x1, y0));
	Vector3f c01 = colorToRgb01(image.get(x0, y1));
	Vector3f c11 = colorToRgb01(image.get(x1, y1));
	Vector3f c0 = c00 * (1.0f - tx) + c10 * tx;
	Vector3f c1 = c01 * (1.0f - tx) + c11 * tx;
	return c0 * (1.0f - ty) + c1 * ty;
}

Vector2f directionToLatLong(const Vector3f& dir) {
	Vector3f d = dir;
	if (d.squaredNorm() <= 1e-8f) {
		d = Vector3f(0.0f, 1.0f, 0.0f);
	}
	else {
		d.normalize();
	}
	float u = std::atan2(d[2], d[0]) / (2.0f * static_cast<float>(MY_PI)) + 0.5f;
	float v = std::acos(std::clamp(d[1], -1.0f, 1.0f)) / static_cast<float>(MY_PI);
	return Vector2f(wrap01(u), std::clamp(v, 0.0f, 1.0f));
}

Vector3f sampleEnvironmentLatLong(const vector<MyImage>& mips, const Vector3f& dir, float roughness) {
	if (mips.empty()) {
		return Vector3f::Zero();
	}
	float mipLevel = roughness * static_cast<float>(std::max(static_cast<int>(mips.size()) - 1, 0));
	int mip0 = std::clamp(static_cast<int>(std::floor(mipLevel)), 0, static_cast<int>(mips.size()) - 1);
	int mip1 = std::clamp(mip0 + 1, 0, static_cast<int>(mips.size()) - 1);
	float t = mipLevel - static_cast<float>(mip0);
	Vector2f uv = directionToLatLong(dir);
	Vector3f c0 = sampleTextureLinearRgb(mips[mip0], uv[0], uv[1]);
	Vector3f c1 = sampleTextureLinearRgb(mips[mip1], uv[0], uv[1]);
	return srgbToLinear(c0 * (1.0f - t) + c1 * t);
}

Vector3f sampleSkyIrradiance(const Vector3f& dir) {
	float t = std::clamp(dir[1] * 0.5f + 0.5f, 0.0f, 1.0f);
	Vector3f horizon(0.45f, 0.48f, 0.52f);
	Vector3f zenith(0.62f, 0.72f, 0.92f);
	Vector3f ground(0.10f, 0.10f, 0.11f);
	if (dir[1] >= 0.0f) {
		return srgbToLinear(horizon * (1.0f - t) + zenith * t);
	}
	float down = std::clamp(-dir[1], 0.0f, 1.0f);
	return srgbToLinear(horizon * (1.0f - down) + ground * down);
}

float radicalInverseVdC(unsigned int bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

Vector2f hammersley(unsigned int i, unsigned int n) {
	return Vector2f(static_cast<float>(i) / static_cast<float>(n), radicalInverseVdC(i));
}

Vector3f importanceSampleGGX(const Vector2f& xi, const Vector3f& n, float roughness) {
	float a = roughness * roughness;
	float phi = 2.0f * static_cast<float>(MY_PI) * xi[0];
	float cosTheta = std::sqrt((1.0f - xi[1]) / std::max(1.0f + (a * a - 1.0f) * xi[1], 1e-6f));
	float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));
	Vector3f h(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
	Vector3f up = std::abs(n[2]) < 0.999f ? Vector3f(0.0f, 0.0f, 1.0f) : Vector3f(1.0f, 0.0f, 0.0f);
	Vector3f tangent = up.cross(n).normalized();
	Vector3f bitangent = n.cross(tangent);
	Vector3f sampleVec = tangent * h[0] + bitangent * h[1] + n * h[2];
	return sampleVec.normalized();
}

Vector2f integrateBrdfSample(float ndotv, float roughness) {
	Vector3f v(std::sqrt(std::max(1.0f - ndotv * ndotv, 0.0f)), 0.0f, ndotv);
	Vector3f n(0.0f, 0.0f, 1.0f);
	float a = 0.0f;
	float b = 0.0f;
	const unsigned int sampleCount = 64u;
	for (unsigned int i = 0; i < sampleCount; ++i) {
		Vector2f xi = hammersley(i, sampleCount);
		Vector3f h = importanceSampleGGX(xi, n, std::max(roughness, 0.04f));
		Vector3f l = (2.0f * v.dot(h) * h - v).normalized();
		float ndotl = std::max(l[2], 0.0f);
		float ndoth = std::max(h[2], 0.0f);
		float vdoth = std::max(v.dot(h), 0.0f);
		if (ndotl > 1e-5f) {
			float k = std::pow(roughness + 1.0f, 2.0f) / 8.0f;
			float gv = ndotv / std::max(ndotv * (1.0f - k) + k, 1e-6f);
			float gl = ndotl / std::max(ndotl * (1.0f - k) + k, 1e-6f);
			float gVis = (gv * gl * vdoth) / std::max(ndoth * ndotv, 1e-6f);
			float fc = std::pow(1.0f - vdoth, 5.0f);
			a += (1.0f - fc) * gVis;
			b += fc * gVis;
		}
	}
	return Vector2f(a, b) / static_cast<float>(sampleCount);
}

Vector3f perspectiveCorrectWeights(const vector<float>& reciprocalWs, const int indexs[3], float alpha, float beta, float gamma) {
	float w0 = reciprocalWs[indexs[0]];
	float w1 = reciprocalWs[indexs[1]];
	float w2 = reciprocalWs[indexs[2]];
	float denom = alpha * w0 + beta * w1 + gamma * w2;
	if (std::abs(denom) <= 1e-8f) {
		return Vector3f(alpha, beta, gamma);
	}
	return Vector3f(alpha * w0, beta * w1, gamma * w2) / denom;
}

float sampleShadowVisibility(const RenderDepthBuffer& shadowmap, float x, float y, float depth, float bias, int radius) {
	if (shadowmap.rows() == 0 || shadowmap.cols() == 0) {
		return 1.0f;
	}

	int px = static_cast<int>(std::round(x));
	int py = static_cast<int>(std::round(y));
	if (px < 0 || px >= shadowmap.cols() || py < 0 || py >= shadowmap.rows() || depth < 0.0f || depth > 1.0f) {
		return 1.0f;
	}

	float visibility = 0.0f;
	int samples = 0;
	for (int dy = -radius; dy <= radius; ++dy) {
		for (int dx = -radius; dx <= radius; ++dx) {
			int sx = std::clamp(px + dx, 0, static_cast<int>(shadowmap.cols()) - 1);
			int sy = std::clamp(py + dy, 0, static_cast<int>(shadowmap.rows()) - 1);
			float storedDepth = static_cast<float>(shadowmap(sy, sx));
			visibility += (storedDepth + bias < depth) ? 0.0f : 1.0f;
			++samples;
		}
	}
	return samples > 0 ? visibility / static_cast<float>(samples) : 1.0f;
}

struct ClipPlane {
	float a;
	float b;
	float c;
	float d;
};

const ClipPlane kFrustumPlanes[] = {
	{ 1.0f, 0.0f, 0.0f, 1.0f },
	{ -1.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 1.0f, 0.0f, 1.0f },
	{ 0.0f, -1.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 1.0f, 1.0f },
	{ 0.0f, 0.0f, -1.0f, 1.0f }
};

float planeEval(const ClipPlane& plane, const ComplexShader::RasterVertex& vertex) {
	const Vector4f& p = vertex.clip_position;
	return plane.a * p[0] + plane.b * p[1] + plane.c * p[2] + plane.d * p[3];
}

ComplexShader::RasterVertex interpolateVertex(const ComplexShader::RasterVertex& a, const ComplexShader::RasterVertex& b, float t) {
	ComplexShader::RasterVertex result;
	result.clip_position = a.clip_position + t * (b.clip_position - a.clip_position);
	result.shadow_position = a.shadow_position + t * (b.shadow_position - a.shadow_position);
	result.shadow_position_near = a.shadow_position_near + t * (b.shadow_position_near - a.shadow_position_near);
	result.shadow_position_far = a.shadow_position_far + t * (b.shadow_position_far - a.shadow_position_far);
	result.uv = a.uv + t * (b.uv - a.uv);
	result.normal = a.normal + t * (b.normal - a.normal);
	result.tangent = a.tangent + t * (b.tangent - a.tangent);
	result.bitangent = a.bitangent + t * (b.bitangent - a.bitangent);
	result.view_position = a.view_position + t * (b.view_position - a.view_position);
	result.world_position = a.world_position + t * (b.world_position - a.world_position);
	result.world_normal = a.world_normal + t * (b.world_normal - a.world_normal);
	return result;
}

void finalizeRasterVertex(ComplexShader::RasterVertex& vertex, int width, int height) {
	float w = vertex.clip_position[3];
	vertex.reciprocal_w = std::abs(w) > 1e-8f ? 1.0f / w : 1.0f;
	Vector4f ndc = std::abs(w) > 1e-8f ? vertex.clip_position * vertex.reciprocal_w : vertex.clip_position;
	vertex.screen_position = Vector4f(
		(ndc[0] * 0.5f + 0.5f) * static_cast<float>(width),
		(ndc[1] * 0.5f + 0.5f) * static_cast<float>(height),
		ndc[2],
		1.0f);
	if (vertex.normal.head<3>().squaredNorm() > 1e-8f) {
		vertex.normal.normalize();
	}
}

void clipAgainstPlane(const vector<ComplexShader::RasterVertex>& input, const ClipPlane& plane, vector<ComplexShader::RasterVertex>& output) {
	output.clear();
	if (input.empty()) {
		return;
	}
	output.reserve(input.size() + 1);

	for (size_t i = 0; i < input.size(); ++i) {
		const ComplexShader::RasterVertex& current = input[i];
		const ComplexShader::RasterVertex& previous = input[(i + input.size() - 1) % input.size()];
		float fCurrent = planeEval(plane, current);
		float fPrevious = planeEval(plane, previous);
		bool currentInside = fCurrent >= 0.0f && current.clip_position[3] > 1e-6f;
		bool previousInside = fPrevious >= 0.0f && previous.clip_position[3] > 1e-6f;

		if (currentInside != previousInside) {
			float denom = fPrevious - fCurrent;
			float t = std::abs(denom) <= 1e-8f ? 0.0f : std::clamp(fPrevious / denom, 0.0f, 1.0f);
			ComplexShader::RasterVertex clipped = interpolateVertex(previous, current, t);
			output.push_back(clipped);
		}

		if (currentInside) {
			output.push_back(current);
		}
	}

}

bool clipAgainstFrustum(const ComplexShader::RasterVertex triangle[3], int width, int height, vector<ComplexShader::RasterVertex>& poly, vector<ComplexShader::RasterVertex>& scratch) {
	poly.assign(triangle, triangle + 3);
	scratch.clear();

	for (const auto& plane : kFrustumPlanes) {
		clipAgainstPlane(poly, plane, scratch);
		if (scratch.size() < 3) {
			poly.clear();
			return false;
		}
		poly.swap(scratch);
	}

	for (auto& v : poly) {
		finalizeRasterVertex(v, width, height);
	}
	return true;
}

bool triangleOutsideFrustum(const ComplexShader::RasterVertex triangle[3]) {
	for (const auto& plane : kFrustumPlanes) {
		bool outside = true;
		for (int i = 0; i < 3; ++i) {
			if (triangle[i].clip_position[3] > 1e-6f && planeEval(plane, triangle[i]) >= 0.0f) {
				outside = false;
				break;
			}
		}
		if (outside) {
			return true;
		}
	}
	return false;
}

bool triangleFullyInsideFrustum(const ComplexShader::RasterVertex triangle[3]) {
	for (int i = 0; i < 3; ++i) {
		if (triangle[i].clip_position[3] <= 1e-6f) {
			return false;
		}
	}
	for (const auto& plane : kFrustumPlanes) {
		for (int i = 0; i < 3; ++i) {
			if (planeEval(plane, triangle[i]) < 0.0f) {
				return false;
			}
		}
	}
	return true;
}

Vector3f perspectiveCorrectWeights(const ComplexShader::RasterVertex triangle[3], float alpha, float beta, float gamma) {
	float w0 = triangle[0].reciprocal_w;
	float w1 = triangle[1].reciprocal_w;
	float w2 = triangle[2].reciprocal_w;
	float denom = alpha * w0 + beta * w1 + gamma * w2;
	if (std::abs(denom) <= 1e-8f) {
		return Vector3f(alpha, beta, gamma);
	}
	return Vector3f(alpha * w0, beta * w1, gamma * w2) / denom;
}
}

void SimpleShader::addLight(Vec4f light) { light_dirs.push_back(light); }
void SimpleShader::setUV(vector<Vec2f>& uv0) { uvs = uv0; }
void SimpleShader::setNormal(vector<Vec4f>& normal0) { normals = normal0; }
void SimpleShader::setTgatexture(const shared_ptr<TGAImage>& tgatext) { tgatexture =tgatext; }
void SimpleShader::setNMTexture(const shared_ptr<TGAImage>& tgatext) { nmtexture = tgatext; }
void SimpleShader::setSpecTexture(const shared_ptr<TGAImage>&tgatext) { spectexture = tgatext; }
void SimpleShader::setUniform_M(const Mat& m) {
	uniform_M = m; 
	uniform_MIT= uniform_M.inv().t();
};
void SimpleShader::setUniform_MIT(const Mat& m) { uniform_MIT = m; };
void SimpleShader::setUniform_V(const Mat& m) { uniform_V = m; };
//void SimpleShader::setPointions(vector<Vec4f>& position0) { positions = position0; };
vec4 SimpleShader::vertexShader(const vertex& vertexs, const Mat& mvp) {

	//vec4 ves_bymvp = vertexs.v * mvp;
	//return ves_bymvp;
	return vec4();
};
int SimpleShader::fragmentShader(Vec3f& bc, Vec3b& color) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 5.f, spec = 16.f;
	Vec4f n(0.f, 0.f, 0.f, 0.f);
	Vec4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uvs[i][0];
		v += bc[i] * uvs[i][1];
		n[0] += bc[i] * normals[i][0];
		n[1] += bc[i] * normals[i][1];
		n[2] += bc[i] * normals[i][2];
	}

	if(tgatexture){ 
		TGAColor& coltex = tgatexture->get(u * tgatexture->get_width(), v * tgatexture->get_height()); 
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	if (nmtexture) {
		TGAColor& col_nm = nmtexture->get(u * nmtexture->get_width(), v * nmtexture->get_height());
		n = Vec4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture) {
		TGAColor& col_spec = spectexture->get(u * spectexture->get_width(), v * spectexture->get_height());
		spec = col_spec.b / 1.f;
	}

	Mat result = uniform_MIT * Mat(n); 
	n = Vec4f(result.at<float>(0, 0),result.at<float>(1, 0),result.at<float>(2, 0),0.f);
	Vec4f l;
	normalize(n, n);
	for (int i = 0; i < light_dirs.size(); i++) {
		Mat m = uniform_V * light_dirs[i];
		l=Vec4f(m.at<float>(0, 0), m.at<float>(1, 0), m.at<float>(2, 0), 0.f) ;
		normalize(l, l);
		Vec4f r = l - 2.f * (n.dot(l)) * n;
		normalize(r, r);
		intense_diff += max(-n.dot(l), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	color = Vec3b(min(ambient + col[0]* (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec),255.0f),
		min(ambient + col[2] * (intense_diff+ 0.6f * intense_spec),255.0f));

	return 1;
};


void ComplexShader::addLight(Vector4f light) { light_dirs.push_back(light); }
void ComplexShader::setUV(vector<Vector2f>& uv0) { uvs = uv0; }
void ComplexShader::setNormal(vector<Vector4f>& normal0) { normals = normal0; }
void ComplexShader::setUV0(const Vector2f& uv0, int index) { uv[index]=uv0; }
void ComplexShader::setN(const Vector4f& n0, int index) { n[index] = n0; }
void ComplexShader::setP(const Vector4f& p0, int index) { position[index] = p0; }
void ComplexShader::setTexture(const vector<MyImage>& textures0) { textures = textures0; }
void ComplexShader::setEnvironmentMap(const MyImage& image) {
	environment_mips.clear();
	if (!image.data || image.width <= 0 || image.height <= 0) {
		ibl_on = 0;
		buildBrdfLut();
		return;
	}
	environment_mips.push_back(image);
	MyImage current = image;
	while (current.width > 1 || current.height > 1) {
		int nextWidth = std::max(1, current.width / 2);
		int nextHeight = std::max(1, current.height / 2);
		std::vector<unsigned char> nextData(nextWidth * nextHeight * current.channels, 0);
		MyImage next(nextData.data(), nextWidth, nextHeight, current.channels);
		for (int y = 0; y < nextHeight; ++y) {
			for (int x = 0; x < nextWidth; ++x) {
				for (int c = 0; c < current.channels; ++c) {
					int x0 = std::min(x * 2, current.width - 1);
					int y0 = std::min(y * 2, current.height - 1);
					int x1 = std::min(x0 + 1, current.width - 1);
					int y1 = std::min(y0 + 1, current.height - 1);
					int idx00 = (x0 + y0 * current.width) * current.channels + c;
					int idx10 = (x1 + y0 * current.width) * current.channels + c;
					int idx01 = (x0 + y1 * current.width) * current.channels + c;
					int idx11 = (x1 + y1 * current.width) * current.channels + c;
					int avg = (static_cast<int>(current.data[idx00]) + static_cast<int>(current.data[idx10]) +
						static_cast<int>(current.data[idx01]) + static_cast<int>(current.data[idx11])) / 4;
					next.data[(x + y * nextWidth) * current.channels + c] = static_cast<unsigned char>(avg);
				}
			}
		}
		environment_mips.push_back(next);
		current = next;
	}
	ibl_on = 1;
	buildBrdfLut();
	std::cout << "[IBL] environment mip levels=" << environment_mips.size()
		<< " brdfLut=" << brdf_lut_size << "x" << brdf_lut_size << std::endl;
}

void ComplexShader::clearEnvironmentMap() {
	environment_mips.clear();
	ibl_on = 0;
	buildBrdfLut();
}

void ComplexShader::buildBrdfLut(int size) {
	brdf_lut_size = std::max(size, 16);
	brdf_lut.assign(brdf_lut_size * brdf_lut_size, Vector2f::Zero());
	for (int y = 0; y < brdf_lut_size; ++y) {
		float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(brdf_lut_size);
		for (int x = 0; x < brdf_lut_size; ++x) {
			float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(brdf_lut_size);
			brdf_lut[y * brdf_lut_size + x] = integrateBrdfSample(std::clamp(ndotv, 0.0f, 1.0f), std::clamp(roughness, 0.04f, 1.0f));
		}
	}
}

void ComplexShader::setUniform_M(const Matrix4f& m) {
	uniform_M = m;
	uniform_M_worldIT = uniform_M.inverse().transpose();
};
void ComplexShader::setUniform_MIT(const Matrix4f& m) { uniform_MIT = (uniform_V * uniform_M).inverse().transpose(); }
void ComplexShader::setUniform_V(const Matrix4f& m) { uniform_V = m; }
void ComplexShader::setUniform_P(const Matrix4f& p) { uniform_P = p; }
//void ComplexShader::setPointions(vector<Vector4f>& position0) { positions = position0; };
Vector4f ComplexShader::vertexShader(const vertex& vertexs, const Matrix4f& mvp) {

	Vector4f ves_bymvp =  mvp * vertexs.v;
	return ves_bymvp;
};
int ComplexShader::drawTriagle_completed(Mat& im, RenderDepthBuffer& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, int indexs[3], float z1, float z2, float z3) {
	(void)z1;
	(void)z2;
	(void)z3;

	RasterVertex inputTriangle[3];
	for (int i = 0; i < 3; ++i) {
		int index = indexs[i];
		inputTriangle[i].clip_position = clip_positions[index];
		inputTriangle[i].screen_position = positions[index];
		inputTriangle[i].shadow_position = shadowpositions.empty() ? Vector4f::Zero() : shadowpositions[index];
		inputTriangle[i].shadow_position_near = shadowpositions_near.empty() ? Vector4f::Zero() : shadowpositions_near[index];
		inputTriangle[i].shadow_position_far = shadowpositions_far.empty() ? Vector4f::Zero() : shadowpositions_far[index];
		inputTriangle[i].uv = uvs[index];
		inputTriangle[i].normal = normals[index];
		inputTriangle[i].tangent = tangents.empty() ? Vector4f::Zero() : tangents[index];
		inputTriangle[i].bitangent = bitangents.empty() ? Vector4f::Zero() : bitangents[index];
		inputTriangle[i].view_position = view_positions.empty() ? Vector3f::Zero() : view_positions[index];
		inputTriangle[i].world_position = world_positions.empty() ? Vector3f::Zero() : world_positions[index];
		inputTriangle[i].world_normal = world_normals.empty() ? Vector4f::Zero() : world_normals[index];
		inputTriangle[i].reciprocal_w = reciprocal_ws[index];
	}

	vector<RasterVertex> clippedPolygon;
	vector<RasterVertex> clipScratch;
	clippedPolygon.reserve(8);
	clipScratch.reserve(8);
	if (!clipAgainstFrustum(inputTriangle, im.cols, im.rows, clippedPolygon, clipScratch)) {
		return 0;
	}

	RasterVertex clippedTriangle[3];
	for (size_t i = 1; i + 1 < clippedPolygon.size(); ++i) {
		clippedTriangle[0] = clippedPolygon[0];
		clippedTriangle[1] = clippedPolygon[i];
		clippedTriangle[2] = clippedPolygon[i + 1];
		drawTriagle_clipped(im, zbuff, difftexture, nmtexture, spectexture, baseColorTex, metallicTex, roughnessTex, metallicRoughnessTex, aoTex, emissiveTex, clippedTriangle, nullptr);
	}
	return 1;
};
int ComplexShader::drawTriagle_clipped(Mat& im, RenderDepthBuffer& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3], const TileBounds* tile) {
	int width = im.cols;
	int height = im.rows;
	const Vector4f& v0 = triangle[0].screen_position;
	const Vector4f& v1 = triangle[1].screen_position;
	const Vector4f& v2 = triangle[2].screen_position;

	int minx = width - 1, miny = height - 1, maxx = 0, maxy = 0;
	minx = max(0, min(minx, static_cast<int>(floor(v0[0]))));
	minx = max(0, min(minx, static_cast<int>(floor(v1[0]))));
	minx = max(0, min(minx, static_cast<int>(floor(v2[0]))));
	miny = max(0, min(miny, static_cast<int>(floor(v0[1]))));
	miny = max(0, min(miny, static_cast<int>(floor(v1[1]))));
	miny = max(0, min(miny, static_cast<int>(floor(v2[1]))));
	maxx = min(width - 1, max(maxx, static_cast<int>(ceil(v0[0]))));
	maxx = min(width - 1, max(maxx, static_cast<int>(ceil(v1[0]))));
	maxx = min(width - 1, max(maxx, static_cast<int>(ceil(v2[0]))));
	maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v0[1]))));
	maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v1[1]))));
	maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v2[1]))));

	if (tile != nullptr) {
		minx = std::max(minx, tile->minx);
		miny = std::max(miny, tile->miny);
		maxx = std::min(maxx, tile->maxx);
		maxy = std::min(maxy, tile->maxy);
	}

	if (minx > maxx || miny > maxy) {
		return 0;
	}

	for (int x = minx; x <= maxx; ++x) {
		for (int y = miny; y <= maxy; ++y) {
			float alpha = static_cast<float>(x);
			float beta = static_cast<float>(y);
			float gamma = v0[2];
			barycentric5(v0, v1, v2, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, alpha, beta, gamma);
			if (alpha < 0 || beta < 0 || gamma < 0) {
				continue;
			}

			Vector3f correctedBc = perspectiveCorrectWeights(triangle, alpha, beta, gamma);
			float depth = correctedBc[0] * v0[2] + correctedBc[1] * v1[2] + correctedBc[2] * v2[2];
			if (!std::isfinite(depth) || depth >= static_cast<float>(zbuff(y, x))) {
				continue;
			}

			zbuff(y, x) = makeRenderDepth(depth);
			Vec3b color;
			fragmentShaderTriangle(correctedBc[0], correctedBc[1], correctedBc[2], color, difftexture, nmtexture, spectexture, baseColorTex, metallicTex, roughnessTex, metallicRoughnessTex, aoTex, emissiveTex, triangle);
			im.at<Vec3b>(height - y - 1, x) = color;
		}
	}

	return 1;
}
int ComplexShader::buildClippedTriangles(const int indexs[3], int width, int height, std::vector<std::array<RasterVertex, 3>>& out) {
	RasterVertex inputTriangle[3];
	for (int i = 0; i < 3; ++i) {
		int index = indexs[i];
		inputTriangle[i].clip_position = clip_positions[index];
		inputTriangle[i].screen_position = positions[index];
		inputTriangle[i].shadow_position = shadowpositions.empty() ? Vector4f::Zero() : shadowpositions[index];
		inputTriangle[i].shadow_position_near = shadowpositions_near.empty() ? Vector4f::Zero() : shadowpositions_near[index];
		inputTriangle[i].shadow_position_far = shadowpositions_far.empty() ? Vector4f::Zero() : shadowpositions_far[index];
		inputTriangle[i].uv = uvs[index];
		inputTriangle[i].normal = normals[index];
		inputTriangle[i].tangent = tangents.empty() ? Vector4f::Zero() : tangents[index];
		inputTriangle[i].bitangent = bitangents.empty() ? Vector4f::Zero() : bitangents[index];
		inputTriangle[i].view_position = view_positions.empty() ? Vector3f::Zero() : view_positions[index];
		inputTriangle[i].world_position = world_positions.empty() ? Vector3f::Zero() : world_positions[index];
		inputTriangle[i].world_normal = world_normals.empty() ? Vector4f::Zero() : world_normals[index];
		inputTriangle[i].reciprocal_w = reciprocal_ws[index];
	}

	if (triangleOutsideFrustum(inputTriangle)) {
		return 0;
	}

	if (triangleFullyInsideFrustum(inputTriangle)) {
		std::array<RasterVertex, 3> tri = { inputTriangle[0], inputTriangle[1], inputTriangle[2] };
		for (auto& v : tri) {
			finalizeRasterVertex(v, width, height);
		}
		out.push_back(tri);
		return 1;
	}

	vector<RasterVertex> clippedPolygon;
	vector<RasterVertex> clipScratch;
	clippedPolygon.reserve(8);
	clipScratch.reserve(8);
	if (!clipAgainstFrustum(inputTriangle, width, height, clippedPolygon, clipScratch)) {
		return 0;
	}

	for (size_t i = 1; i + 1 < clippedPolygon.size(); ++i) {
		std::array<RasterVertex, 3> tri = { clippedPolygon[0], clippedPolygon[i], clippedPolygon[i + 1] };
		out.push_back(tri);
	}
	return 1;
}
int ComplexShader::vertexShader2(const Vector4f& position0, const Vector2f& uv0, const Vector4f& n0, const Vector4f& t0, const Vector4f& b0, const Matrix4f& screen_mvp, const Matrix4f& clip_mvp, const Matrix4f& view_model, int t) {
	Vector4f clipPosition = clip_mvp * position0;
	float clipW = clipPosition[3];
	reciprocal_ws[t] = std::abs(clipW) > 1e-8f ? 1.0f / clipW : 1.0f;
	clip_positions[t] = clipPosition;
	positions[t].noalias() = screen_mvp * position0;
	Vector4f viewPos = view_model * position0;
	view_positions[t] = viewPos.head<3>();
	Vector4f worldPos = uniform_M * position0;
	world_positions[t] = worldPos.head<3>();
	
	if (shadow_on)
	{
		shadowpositions[t] = uniform_LVP * uniform_LP * uniform_LV * uniform_M * position0;
		float w = shadowpositions[t][3];
		if (w != 0.f) {
			shadowpositions[t].noalias() = shadowpositions[t] / w;
		}
		if (shadow_cascade_on) {
			shadowpositions_near[t] = uniform_LVP_near * uniform_LP_near * uniform_LV_near * uniform_M * position0;
			float wn = shadowpositions_near[t][3];
			if (wn != 0.f) {
				shadowpositions_near[t].noalias() = shadowpositions_near[t] / wn;
			}
			shadowpositions_far[t] = uniform_LVP_far * uniform_LP_far * uniform_LV_far * uniform_M * position0;
			float wf = shadowpositions_far[t][3];
			if (wf != 0.f) {
				shadowpositions_far[t].noalias() = shadowpositions_far[t] / wf;
			}
		}
		//light_pos = vec3(light_po[0], light_po[1], light_po[2]);
	}
	float w = positions[t][3];
	if (std::abs(w) <= 1e-8f) { positions[t].noalias() = positions[t]; }
	else {
		positions[t].noalias() = positions[t] / w;
	}
	uvs[t] = uv0;
	normals[t] = uniform_MIT * n0;
	normals[t][3] = 0.f ;
	world_normals[t] = uniform_M_worldIT * n0;
	world_normals[t][3] = 0.f;
	if (normals[t].head<3>().squaredNorm() > 1e-8f) {
		normals[t].normalize();
	}
	if (world_normals[t].head<3>().squaredNorm() > 1e-8f) {
		world_normals[t].normalize();
	}
	tangents[t] = uniform_MIT * t0;
	bitangents[t] = uniform_MIT * b0;
	tangents[t][3] = 0.f;
	bitangents[t][3] = 0.f;
	if (tangents[t].head<3>().squaredNorm() > 1e-8f) {
		tangents[t].normalize();
	}
	if (bitangents[t].head<3>().squaredNorm() > 1e-8f) {
		bitangents[t].normalize();
	}
	return 1;
};
int ComplexShader::fragmentShader(Vector3f& bc, Vec3b& color,int difftexture, int nmtexture ,int spectexture) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uvs[i][0];
		v += bc[i] * uvs[i][1];
		n[0] += bc[i] * normals[i][0];
		n[1] += bc[i] * normals[i][1];
		n[2] += bc[i] * normals[i][2];
	}

	if (difftexture>=0) {
		MyColor coltex = sampleTexture(textures, difftexture, u, v);
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	if (nmtexture >= 0) {
		MyColor col_nm = sampleTexture(textures, nmtexture, u, v);
		n = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor col_spec = sampleTexture(textures, spectexture, u, v);
		spec = col_spec.b / 1.f;
	}
	
	/*Mat result = uniform_MIT * Mat(n);
	n = Vec4f(result.at<float>(0, 0), result.at<float>(1, 0), result.at<float>(2, 0), 0.f);
	Vec4f l;
	normalize(n, n);
	for (int i = 0; i < light_dirs.size(); i++) {
		Mat m = uniform_V * light_dirs[i];
		l = Vec4f(m.at<float>(0, 0), m.at<float>(1, 0), m.at<float>(2, 0), 0.f);
		normalize(l, l);
		Vec4f r = l - 2.f * (n.dot(l)) * n;
		normalize(r, r);
		intense_diff += max(-n.dot(l), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	*/
	color = Vec3b(min(ambient + col[0] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[2] * (intense_diff + 0.6f * intense_spec), 255.0f));

	return 1;
};

int ComplexShader::fragmentShader2(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uv[i][0];
		v += bc[i] * uv[i][1];
		n0[0] += bc[i] * n[i][0];
		n0[1] += bc[i] * n[i][1];
		n0[2] += bc[i] * n[i][2];
	}
	
	if (difftexture >= 0) {
		
		MyColor coltex = sampleTexture(textures, difftexture, u, v);
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	//Vec4f l;

	for (int i = 0; i < light_dirs.size(); i++) {

		Vector4f r = light_dirs[i] - 2.f * (n0.dot(light_dirs[i])) * n0;
		r.normalize();
		intense_diff += max(-n0.dot(light_dirs[i]), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	
	/*color = Vec3b(min(ambient + col[0] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[2] * (intense_diff + 0.6f * intense_spec), 255.0f));*/
	color = Vec3b(col[0]*100,col[1] * 100,col[2] * 100);
	return 1;
};
int ComplexShader::fragmentShader3(float x, float y, float z, Vec3b& color, int difftexture, int nmtexture, int spectexture,int indexs[3]) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);
	u = x * uvs[indexs[0]][0] + y * uvs[indexs[1]][0] + z * uvs[indexs[2]][0];
	v = x * uvs[indexs[0]][1] + y * uvs[indexs[1]][1] + z * uvs[indexs[2]][1];
	u = wrap01(u);
	v = wrap01(v);
	n0[0] = x*normals[indexs[0]][0] + y * normals[indexs[1]][0] + z * normals[indexs[2]][0];
	n0[1] = x * normals[indexs[0]][1] + y * normals[indexs[1]][1] + z * normals[indexs[2]][1];
	n0[2] = x * normals[indexs[0]][2] + y * normals[indexs[1]][2] + z * normals[indexs[2]][2];
	n0[3] = 0.f;
	if (n0.head<3>().squaredNorm() > 1e-8f) {
		n0.normalize();
	}
	
	float shadowVisibility = 1.0f;
	if (shadow_on == 1 ) {
		float shadow_posx = x * shadowpositions[indexs[0]][0] + y * shadowpositions[indexs[1]][0] + z * shadowpositions[indexs[2]][0];
		float shadow_posy = x * shadowpositions[indexs[0]][1] + y * shadowpositions[indexs[1]][1] + z * shadowpositions[indexs[2]][1];
		float shadow_posz = x * shadowpositions[indexs[0]][2] + y * shadowpositions[indexs[1]][2] + z * shadowpositions[indexs[2]][2];
		float nDotL = light_dirs.empty() ? 1.0f : std::abs(n0.dot(light_dirs[0]));
		float bias = std::max(0.006f * (1.0f - nDotL), 0.0025f);
		float normalOffset = 0.0006f * (1.0f - nDotL);
		int radius = 1;
		if (shadow_posz > 0.65f) {
			radius = 2;
		}
		if (shadow_posz > 0.85f) {
			radius = 3;
		}
		shadowVisibility = sampleShadowVisibility(shadowmap, shadow_posx, shadow_posy, shadow_posz - normalOffset, bias, radius);
	}

	if (difftexture >= 0) {
		MyColor coltex = sampleTexture(textures, difftexture, u, v);
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}

	if (nmtexture >= 0) {

		MyColor col_nm = sampleTexture(textures, nmtexture, u, v);
		n0 = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
		if (n0.head<3>().squaredNorm() > 1e-8f) {
			n0.normalize();
		}
	}
	if (spectexture >= 0) {
		MyColor col_spec = sampleTexture(textures, spectexture, u, v);
		spec = col_spec.b / 1.f;
	}

	for (int i = 0; i < light_dirs.size(); i++) {

		Vector4f r = light_dirs[i] - 2.f * (n0.dot(light_dirs[i])) * n0;
		if (r.head<3>().squaredNorm() > 1e-8f) {
			r.normalize();
		}
		intense_diff += max(-n0.dot(light_dirs[i]), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}

	float shadowFactor = 0.35f + 0.65f * shadowVisibility;
	float light = shadowFactor * (intense_diff + 0.6f * intense_spec);
	color = Vec3b(clampColor(ambient + col[0] * light),
		clampColor(ambient + col[1] * light),
		clampColor(ambient + col[2] * light));
	return 1;
};
int ComplexShader::fragmentShaderTriangle(float x, float y, float z, Vec3b& color, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3]) {
	float u = x * triangle[0].uv[0] + y * triangle[1].uv[0] + z * triangle[2].uv[0];
	float v = x * triangle[0].uv[1] + y * triangle[1].uv[1] + z * triangle[2].uv[1];
	u = wrap01(u);
	v = wrap01(v);

	float spec = 16.f;
	Vector4f n0 = x * triangle[0].normal + y * triangle[1].normal + z * triangle[2].normal;
	n0[3] = 0.f;
	if (n0.head<3>().squaredNorm() > 1e-8f) {
		n0.normalize();
	}

	float shadowVisibility = 1.0f;
	if (ray_shadow_on == 1 && ray_backend != nullptr && !light_dirs_world.empty()) {
		Vector3f worldPos = x * triangle[0].world_position + y * triangle[1].world_position + z * triangle[2].world_position;
		Vector3f worldNormal = (x * triangle[0].world_normal + y * triangle[1].world_normal + z * triangle[2].world_normal).head<3>();
		if (worldNormal.squaredNorm() > 1e-8f) {
			worldNormal.normalize();
		}
		Vector3f rayDir = -light_dirs_world[0];
		if (rayDir.squaredNorm() > 1e-8f) {
			rayDir.normalize();
			float bias = 0.0025f;
			Vector3f rayOrigin = worldPos + worldNormal * bias;
			shadowVisibility = ray_backend->occludedDirectional(rayOrigin, rayDir, 0.001f, 1.0e6f) ? 0.0f : 1.0f;
		}
	}
	else if (shadow_on == 1) {
		float nDotL = light_dirs.empty() ? 1.0f : std::abs(n0.dot(light_dirs[0]));
		float bias = std::max(0.006f * (1.0f - nDotL), 0.0025f);
		float normalOffset = 0.0006f * (1.0f - nDotL);
		int radius = 1;

		if (shadow_cascade_on && shadowmap_near.rows() > 0 && shadowmap_far.rows() > 0) {
			float shadow_posx_near = x * triangle[0].shadow_position_near[0] + y * triangle[1].shadow_position_near[0] + z * triangle[2].shadow_position_near[0];
			float shadow_posy_near = x * triangle[0].shadow_position_near[1] + y * triangle[1].shadow_position_near[1] + z * triangle[2].shadow_position_near[1];
			float shadow_posz_near = x * triangle[0].shadow_position_near[2] + y * triangle[1].shadow_position_near[2] + z * triangle[2].shadow_position_near[2];
			float shadow_posx_far = x * triangle[0].shadow_position_far[0] + y * triangle[1].shadow_position_far[0] + z * triangle[2].shadow_position_far[0];
			float shadow_posy_far = x * triangle[0].shadow_position_far[1] + y * triangle[1].shadow_position_far[1] + z * triangle[2].shadow_position_far[1];
			float shadow_posz_far = x * triangle[0].shadow_position_far[2] + y * triangle[1].shadow_position_far[2] + z * triangle[2].shadow_position_far[2];

			float viewDepth = -(x * triangle[0].view_position[2] + y * triangle[1].view_position[2] + z * triangle[2].view_position[2]);
			float split = cascade_split;
			float blend = cascade_blend;
			float t = 0.0f;
			if (blend > 1e-6f) {
				t = std::clamp((viewDepth - (split - blend)) / (2.0f * blend), 0.0f, 1.0f);
			}

			if (shadow_posz_near > 0.65f) {
				radius = 2;
			}
			if (shadow_posz_near > 0.85f) {
				radius = 3;
			}
			float visNear = sampleShadowVisibility(shadowmap_near, shadow_posx_near, shadow_posy_near, shadow_posz_near - normalOffset, bias, radius);

			radius = 1;
			if (shadow_posz_far > 0.65f) {
				radius = 2;
			}
			if (shadow_posz_far > 0.85f) {
				radius = 3;
			}
			float visFar = sampleShadowVisibility(shadowmap_far, shadow_posx_far, shadow_posy_far, shadow_posz_far - normalOffset, bias, radius);
			shadowVisibility = visNear * (1.0f - t) + visFar * t;
		}
		else {
			float shadow_posx = x * triangle[0].shadow_position[0] + y * triangle[1].shadow_position[0] + z * triangle[2].shadow_position[0];
			float shadow_posy = x * triangle[0].shadow_position[1] + y * triangle[1].shadow_position[1] + z * triangle[2].shadow_position[1];
			float shadow_posz = x * triangle[0].shadow_position[2] + y * triangle[1].shadow_position[2] + z * triangle[2].shadow_position[2];
			if (shadow_posz > 0.65f) {
				radius = 2;
			}
			if (shadow_posz > 0.85f) {
				radius = 3;
			}
			shadowVisibility = sampleShadowVisibility(shadowmap, shadow_posx, shadow_posy, shadow_posz - normalOffset, bias, radius);
		}
	}

	Vector4f col(255.f, 255.f, 255.f, 0.f);
	if (difftexture >= 0) {
		MyColor coltex = sampleTexture(textures, difftexture, u, v);
		col[0] = coltex.b;
		col[1] = coltex.g;
		col[2] = coltex.r;
	}
	if (baseColorTex >= 0) {
		MyColor coltex = sampleTexture(textures, baseColorTex, u, v);
		col[0] = coltex.b;
		col[1] = coltex.g;
		col[2] = coltex.r;
	}

	if (nmtexture >= 0) {
		MyColor col_nm = sampleTexture(textures, nmtexture, u, v);
		Vector3f normalTS(
			col_nm.r / 255.f * 2.f - 1.f,
			col_nm.g / 255.f * 2.f - 1.f,
			col_nm.b / 255.f * 2.f - 1.f);
		normalTS[0] *= normal_strength;
		normalTS[1] *= normal_strength;
		normalTS[2] = std::clamp(normalTS[2], -1.0f, 1.0f);
		Vector3f T = triangle[0].tangent.head<3>() * x + triangle[1].tangent.head<3>() * y + triangle[2].tangent.head<3>() * z;
		Vector3f B = triangle[0].bitangent.head<3>() * x + triangle[1].bitangent.head<3>() * y + triangle[2].bitangent.head<3>() * z;
		Vector3f Nbase = n0.head<3>();
		if (T.squaredNorm() > 1e-8f && B.squaredNorm() > 1e-8f && Nbase.squaredNorm() > 1e-8f) {
			Nbase.normalize();
			T = (T - Nbase * Nbase.dot(T));
			if (T.squaredNorm() > 1e-8f) {
				T.normalize();
			}
			B = Nbase.cross(T);
			if (B.squaredNorm() > 1e-8f) {
				B.normalize();
			}
			Matrix3f TBN;
			TBN.col(0) = T;
			TBN.col(1) = B;
			TBN.col(2) = Nbase;
			Vector3f nMapped = TBN * normalTS;
			if (nMapped.squaredNorm() > 1e-8f) {
				nMapped.normalize();
				n0 = Vector4f(nMapped[0], nMapped[1], nMapped[2], 0.f);
			}
		}
	}
	if (spectexture >= 0) {
		MyColor col_spec = sampleTexture(textures, spectexture, u, v);
		spec = col_spec.b / 1.f;
	}

	MaterialPbrChannelMap map = pbr_channel_map;
	Vector3f albedo = Vector3f(col[2], col[1], col[0]) / 255.0f;
	albedo = albedo.array().pow(2.2f);

	float metallic = 0.0f;
	float roughness = 0.8f;
	if (metallicRoughnessTex >= 0) {
		MyColor mr = sampleTexture(textures, metallicRoughnessTex, u, v);
		roughness = std::clamp(channelValue01(mr, map.roughness_channel), 0.04f, 1.0f);
		metallic = std::clamp(channelValue01(mr, map.metallic_channel), 0.0f, 1.0f);
	}
	if (metallicTex >= 0) {
		MyColor mt = sampleTexture(textures, metallicTex, u, v);
		metallic = std::clamp(channelValue01(mt, map.metallic_channel), 0.0f, 1.0f);
	}
	if (roughnessTex >= 0) {
		MyColor rt = sampleTexture(textures, roughnessTex, u, v);
		roughness = std::clamp(channelValue01(rt, map.roughness_channel), 0.04f, 1.0f);
	}

	float ao = 1.0f;
	if (aoTex >= 0) {
		MyColor aoSample = sampleTexture(textures, aoTex, u, v);
		ao = std::clamp(channelValue01(aoSample, map.ao_channel), 0.0f, 1.0f);
	}

	Vector3f emissive(0.f, 0.f, 0.f);
	if (emissiveTex >= 0) {
		MyColor em = sampleTexture(textures, emissiveTex, u, v);
		if (em.channels >= 3) {
			emissive = Vector3f(em.b, em.g, em.r) / 255.0f;
		}
		else {
			float e = channelValue01(em, map.emissive_channel);
			emissive = Vector3f(e, e, e);
		}
		emissive = emissive.array().pow(2.2f);
	}

	Vector3f N = n0.head<3>();
	if (N.squaredNorm() > 1e-8f) {
		N.normalize();
	}
	Vector3f V = -(x * triangle[0].view_position + y * triangle[1].view_position + z * triangle[2].view_position);
	if (V.squaredNorm() <= 1e-8f) {
		V = Vector3f(0.f, 0.f, 1.f);
	}
	V.normalize();

	Vector3f primaryLightDir = Vector3f(0.0f, 0.0f, 1.0f);
	if (!light_dirs.empty() && light_dirs[0].head<3>().squaredNorm() > 1e-8f) {
		primaryLightDir = -light_dirs[0].head<3>().normalized();
	}
	Vector3f primaryLightColor = !light_colors.empty() ? light_colors[0] : Vector3f::Ones();
	primaryLightColor[0] = std::max(primaryLightColor[0], 0.0f);
	primaryLightColor[1] = std::max(primaryLightColor[1], 0.0f);
	primaryLightColor[2] = std::max(primaryLightColor[2], 0.0f);
	float primaryNdotL = std::max(N.dot(primaryLightDir), 0.0f);
	float primaryNdotV = std::max(N.dot(V), 0.0f);
	Vector3f primaryHalf = V + primaryLightDir;
	if (primaryHalf.squaredNorm() > 1e-8f) {
		primaryHalf.normalize();
	}
	float programmableSpecular = std::pow(std::max(N.dot(primaryHalf), 0.0f), std::max(8.0f, spec * 1.35f));
	float rimFactor = std::pow(std::max(1.0f - primaryNdotV, 0.0f), 2.0f);
	float halfLambert = primaryNdotL * 0.5f + 0.5f;

	if (force_programmable_shader && programmable_shader_program) {
		ProgrammableShaderProgram::Inputs inputs;
		inputs.uv_u = u;
		inputs.uv_v = v;
		inputs.base_r = albedo[0];
		inputs.base_g = albedo[1];
		inputs.base_b = albedo[2];
		inputs.base_luma = 0.2126f * albedo[0] + 0.7152f * albedo[1] + 0.0722f * albedo[2];
		inputs.n_x = N[0];
		inputs.n_y = N[1];
		inputs.n_z = N[2];
		inputs.v_x = V[0];
		inputs.v_y = V[1];
		inputs.v_z = V[2];
		inputs.l_x = primaryLightDir[0];
		inputs.l_y = primaryLightDir[1];
		inputs.l_z = primaryLightDir[2];
		inputs.light_r = primaryLightColor[0];
		inputs.light_g = primaryLightColor[1];
		inputs.light_b = primaryLightColor[2];
		inputs.ndotl = primaryNdotL;
		inputs.ndotv = primaryNdotV;
		inputs.half_lambert = halfLambert;
		inputs.rim = rimFactor;
		inputs.shadow = shadowVisibility;
		inputs.metallic = metallic;
		inputs.roughness = roughness;
		inputs.ao = ao;
		inputs.emissive_r = emissive[0];
		inputs.emissive_g = emissive[1];
		inputs.emissive_b = emissive[2];
		inputs.ambient = phong_ambient_strength;
		inputs.specular = programmableSpecular * phong_specular_strength;
		inputs.exposure = exposure;

		float outR = 0.0f;
		float outG = 0.0f;
		float outB = 0.0f;
		if (programmable_shader_program->evaluate(inputs, outR, outG, outB)) {
			Vector3f colorLinear(std::max(outR, 0.0f), std::max(outG, 0.0f), std::max(outB, 0.0f));
			colorLinear *= exposure;
			Vector3f colorMapped = tonemapReinhard(colorLinear);
			Vector3f colorSRGB = linearToSrgb(colorMapped.cwiseMax(Vector3f::Zero()));
			color = Vec3b(
				clampColor(colorSRGB[2] * 255.0f),
				clampColor(colorSRGB[1] * 255.0f),
				clampColor(colorSRGB[0] * 255.0f));
			return 1;
		}
	}

	bool materialHasPbr = baseColorTex >= 0 || metallicTex >= 0 || roughnessTex >= 0 || metallicRoughnessTex >= 0 || aoTex >= 0 || emissiveTex >= 0;
	bool usePbr = materialHasPbr && !force_stylized_phong && !(force_programmable_shader && programmable_shader_program);
	if (usePbr) {
		Vector3f F0(0.04f, 0.04f, 0.04f);
		F0 = F0 + (albedo - F0) * metallic;

		auto fresnelSchlick = [](float cosTheta, const Vector3f& F0) {
			return F0 + (Vector3f::Ones() - F0) * std::pow(1.0f - cosTheta, 5.0f);
		};
		auto fresnelSchlickRoughness = [](float cosTheta, const Vector3f& F0, float roughnessValue) {
			Vector3f oneMinusRoughness = Vector3f::Ones() * (1.0f - roughnessValue);
			Vector3f maxValue(
				std::max(oneMinusRoughness[0], F0[0]),
				std::max(oneMinusRoughness[1], F0[1]),
				std::max(oneMinusRoughness[2], F0[2]));
			return F0 + (maxValue - F0) * std::pow(1.0f - cosTheta, 5.0f);
		};
		auto distributionGGX = [](const Vector3f& N, const Vector3f& H, float rough) {
			float a = rough * rough;
			float a2 = a * a;
			float NdotH = std::max(N.dot(H), 0.0f);
			float NdotH2 = NdotH * NdotH;
			float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
			return a2 / std::max(3.1415926f * denom * denom, 1e-6f);
		};
		auto geometrySchlickGGX = [](float NdotV, float rough) {
			float r = rough + 1.0f;
			float k = (r * r) / 8.0f;
			return NdotV / std::max(NdotV * (1.0f - k) + k, 1e-6f);
		};
		auto geometrySmith = [&](const Vector3f& N, const Vector3f& V, const Vector3f& L, float rough) {
			float NdotV = std::max(N.dot(V), 0.0f);
			float NdotL = std::max(N.dot(L), 0.0f);
			float ggx1 = geometrySchlickGGX(NdotV, rough);
			float ggx2 = geometrySchlickGGX(NdotL, rough);
			return ggx1 * ggx2;
		};

		Vector3f Lo(0.f, 0.f, 0.f);
		for (int i = 0; i < light_dirs.size(); ++i) {
			Vector3f L = -light_dirs[i].head<3>();
			if (L.squaredNorm() <= 1e-8f) {
				continue;
			}
			L.normalize();
			Vector3f H = (V + L);
			if (H.squaredNorm() <= 1e-8f) {
				continue;
			}
			H.normalize();

			float NdotL = std::max(N.dot(L), 0.0f);
			float NdotV = std::max(N.dot(V), 0.0f);
			if (NdotL <= 1e-6f || NdotV <= 1e-6f) {
				continue;
			}

			float NDF = distributionGGX(N, H, roughness);
			float G = geometrySmith(N, V, L, roughness);
			Vector3f F = fresnelSchlick(std::max(H.dot(V), 0.0f), F0);
			Vector3f numerator = NDF * G * F;
			float denom = std::max(4.0f * NdotV * NdotL, 1e-6f);
			Vector3f specular = numerator / denom;

			Vector3f kS = F;
			Vector3f kD = Vector3f::Ones() - kS;
			kD *= (1.0f - metallic);

			Vector3f radiance = (i < static_cast<int>(light_colors.size())) ? light_colors[i] : Vector3f::Ones();
			float shadowFactor = 0.35f + 0.65f * shadowVisibility;
			Vector3f brdf = kD.cwiseProduct(albedo) / 3.1415926f + specular;
			Lo += brdf.cwiseProduct(radiance) * (NdotL * shadowFactor);
		}

		float NdotV = std::max(N.dot(V), 0.0f);
		Vector3f F = fresnelSchlickRoughness(NdotV, F0, roughness);
		Vector3f kS = F;
		Vector3f kD = (Vector3f::Ones() - kS) * (1.0f - metallic);
		Vector3f diffuseIbl = Vector3f::Zero();
		Vector3f specularIbl = Vector3f::Zero();
		Vector3f R = (2.0f * N.dot(V) * N - V).normalized();
		if (ibl_on && !environment_mips.empty()) {
			Vector3f irradiance = sampleEnvironmentLatLong(environment_mips, N, 1.0f);
			diffuseIbl = irradiance.cwiseProduct(albedo).cwiseProduct(kD) * ibl_diffuse_strength;
			if (!brdf_lut.empty() && brdf_lut_size > 0) {
				float lutX = std::clamp(NdotV, 0.0f, 1.0f);
				float lutY = std::clamp(roughness, 0.0f, 1.0f);
				int ix = std::clamp(static_cast<int>(lutX * static_cast<float>(brdf_lut_size - 1)), 0, brdf_lut_size - 1);
				int iy = std::clamp(static_cast<int>(lutY * static_cast<float>(brdf_lut_size - 1)), 0, brdf_lut_size - 1);
				Vector2f envBrdf = brdf_lut[iy * brdf_lut_size + ix];
				Vector3f prefiltered = sampleEnvironmentLatLong(environment_mips, R, roughness);
				Vector3f specTerm = F * envBrdf[0] + Vector3f::Ones() * envBrdf[1];
				specularIbl = prefiltered.cwiseProduct(specTerm) * ibl_specular_strength;
			}
		}
		else {
			Vector3f sky = sampleSkyIrradiance(N);
			diffuseIbl = sky.cwiseProduct(albedo).cwiseProduct(kD) * sky_light_strength;
			specularIbl = sampleSkyIrradiance(R).cwiseProduct(F) * (sky_light_strength * 0.35f);
		}

		Vector3f colorLinear = (diffuseIbl * ao + specularIbl * ao + Lo + emissive) * exposure;
		Vector3f colorMapped = tonemapReinhard(colorLinear);
		Vector3f colorSRGB = linearToSrgb(colorMapped);
		color = Vec3b(
			clampColor(colorSRGB[2] * 255.0f),
			clampColor(colorSRGB[1] * 255.0f),
			clampColor(colorSRGB[0] * 255.0f));
		return 1;
	}
	float specStrength = phong_specular_strength;
	float shininess = std::max(24.0f, spec * 1.35f);
	Vector3f diffuseLighting = Vector3f::Zero();
	Vector3f specularLighting = Vector3f::Zero();

	for (int i = 0; i < light_dirs.size(); ++i) {
		Vector3f L = -light_dirs[i].head<3>();
		if (L.squaredNorm() <= 1e-8f) {
			continue;
		}
		L.normalize();
		float NdotL = std::max(N.dot(L), 0.0f);
		if (NdotL <= 1e-6f) {
			continue;
		}
		float diffuseTerm = phong_toon_diffuse ? quantizeToonDiffuse(NdotL) : NdotL;
		Vector3f H = (V + L);
		if (H.squaredNorm() > 1e-8f) {
			H.normalize();
		}
		float specTerm = std::pow(std::max(N.dot(H), 0.0f), shininess);
		if (phong_hard_specular) {
			specTerm = specTerm > 0.42f ? 1.0f : 0.0f;
		}
		Vector3f lightEnergy = Vector3f::Ones();
		if (i < static_cast<int>(light_colors.size())) {
			lightEnergy = light_colors[i];
			lightEnergy[0] = std::max(lightEnergy[0], 0.0f);
			lightEnergy[1] = std::max(lightEnergy[1], 0.0f);
			lightEnergy[2] = std::max(lightEnergy[2], 0.0f);
		}
		if (phong_primary_light_only) {
			lightEnergy *= (i == 0) ? 1.0f : phong_secondary_light_scale;
		}
		diffuseLighting += lightEnergy * diffuseTerm;
		specularLighting += lightEnergy * (specStrength * specTerm);
	}

	float shadowDiffuse = 0.18f + 0.82f * shadowVisibility;
	float shadowSpec = 0.85f + 0.15f * shadowVisibility;

	Vector3f diffuse = albedo.cwiseProduct(diffuseLighting) * shadowDiffuse;
	Vector3f specular = specularLighting * shadowSpec;
	Vector3f ambientColor = albedo * phong_ambient_strength;
	Vector3f colorLinear = (ambientColor + diffuse + specular) * exposure;
	Vector3f outputLinear = phong_use_tonemap ? tonemapReinhard(colorLinear) : colorLinear.cwiseMax(Vector3f::Zero());
	Vector3f colorSrgb = linearToSrgb(outputLinear);
	color = Vec3b(
		clampColor(colorSrgb[2] * 255.0f),
		clampColor(colorSrgb[1] * 255.0f),
		clampColor(colorSrgb[0] * 255.0f));
	return 1;
}
int ComplexShader::fragmentShader_shadow(float x, float y, float z, float x_shadow, float y_shadow, float z_shadow, Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	u = x * uvs[indexs[0]][0] + y * uvs[indexs[1]][0] + z * uvs[indexs[2]][0];
	v = x * uvs[indexs[0]][1] + y * uvs[indexs[1]][1] + z * uvs[indexs[2]][1];
	u = wrap01(u);
	v = wrap01(v);
	n0[0] = x * normals[indexs[0]][0] + y * normals[indexs[1]][0] + z * normals[indexs[2]][0];
	n0[1] = x * normals[indexs[0]][1] + y * normals[indexs[1]][1] + z * normals[indexs[2]][1];
	n0[2] = x * normals[indexs[0]][2] + y * normals[indexs[1]][2] + z * normals[indexs[2]][2];
	n0[3] = 0.f;
	if (n0.head<3>().squaredNorm() > 1e-8f) {
		n0.normalize();
	}

	float shadowVisibility = 1.0f;
	if (shadow_on == 1) {
		float shadow_posx = x_shadow * shadowpositions[indexs[0]][0] + y_shadow * shadowpositions[indexs[1]][0] + z_shadow * shadowpositions[indexs[2]][0];
		float shadow_posy = x_shadow * shadowpositions[indexs[0]][1] + y_shadow * shadowpositions[indexs[1]][1] + z_shadow * shadowpositions[indexs[2]][1];
		float shadow_posz = x_shadow * shadowpositions[indexs[0]][2] + y_shadow * shadowpositions[indexs[1]][2] + z_shadow * shadowpositions[indexs[2]][2];
		float nDotL = light_dirs.empty() ? 1.0f : std::abs(n0.dot(light_dirs[0]));
		float bias = std::max(0.006f * (1.0f - nDotL), 0.0025f);
		float normalOffset = 0.0006f * (1.0f - nDotL);
		int radius = 1;
		if (shadow_posz > 0.65f) {
			radius = 2;
		}
		if (shadow_posz > 0.85f) {
			radius = 3;
		}
		shadowVisibility = sampleShadowVisibility(shadowmap, shadow_posx, shadow_posy, shadow_posz - normalOffset, bias, radius);
	}

	if (difftexture >= 0) {
		MyColor coltex = sampleTexture(textures, difftexture, u, v);
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}

	if (nmtexture >= 0) {

		MyColor col_nm = sampleTexture(textures, nmtexture, u, v);
		n0 = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
		if (n0.head<3>().squaredNorm() > 1e-8f) {
			n0.normalize();
		}
	}
	if (spectexture >= 0) {
		MyColor col_spec = sampleTexture(textures, spectexture, u, v);
		spec = col_spec.b / 1.f;
	}
	//Vec4f l;

	for (int i = 0; i < light_dirs.size(); i++) {

		Vector4f r = light_dirs[i] - 2.f * (n0.dot(light_dirs[i])) * n0;
		if (r.head<3>().squaredNorm() > 1e-8f) {
			r.normalize();
		}
		intense_diff += max(-n0.dot(light_dirs[i]), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}

	float shadowFactor = 0.35f + 0.65f * shadowVisibility;
	float light = shadowFactor * (intense_diff + 0.6f * intense_spec);
	color = Vec3b(clampColor(ambient + col[0] * light),
		clampColor(ambient + col[1] * light),
		clampColor(ambient + col[2] * light));
	return 1;
};
int ShadowShader::vertexShader(const Vector4f& position0, int t) {
	positions[t].noalias() = uniform_VP * uniform_P * uniform_V * uniform_M * position0;
	
	float w = positions[t][3];
	if (w == 0) { positions[t].noalias() = positions[t]; }
	else {
		positions[t].noalias() = positions[t] / w;
	}
	return 1;
};
int ShadowShader::fragmentShader(Vector3f& bc, Vec3b& color) {
	return 1;

};
static bool shadowTriangleMaybeVisible(const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, int width, int height) {
	const float minx = std::min({ v0[0], v1[0], v2[0] });
	const float maxx = std::max({ v0[0], v1[0], v2[0] });
	const float miny = std::min({ v0[1], v1[1], v2[1] });
	const float maxy = std::max({ v0[1], v1[1], v2[1] });
	const float minz = std::min({ v0[2], v1[2], v2[2] });
	const float maxz = std::max({ v0[2], v1[2], v2[2] });
	if (maxx < 0.0f || minx > static_cast<float>(width - 1) || maxy < 0.0f || miny > static_cast<float>(height - 1)) {
		return false;
	}
	if (maxz < 0.0f || minz > 1.0f) {
		return false;
	}
	Vector2f e1(v1[0] - v0[0], v1[1] - v0[1]);
	Vector2f e2(v2[0] - v0[0], v2[1] - v0[1]);
	float area2 = e1[0] * e2[1] - e1[1] * e2[0];
	return std::abs(area2) > 1e-8f;
}
int ShadowShader::drawTriagle( const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, RenderDepthBuffer& zbuff) {
	/*
	if (vertexs.size() > 3 || vertexs.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}*/
	
	int weight = zbuff.cols();
	int height = zbuff.rows();

	//Vector2f bboxmin(weight - 1, height - 1);
	//Vector2f bboxmax(0, 0);
	//Vector2f clamp(weight - 1, height - 1);
	int minx = weight - 1, miny = height - 1, maxx = 0, maxy = 0;
	{
		minx = max(0, min(minx, (int)v0[0])); minx = max(0, min(minx, (int)v1[0])); minx = max(0, min(minx, (int)v2[0]));
		miny = max(0, min(miny, (int)v0[1])); miny = max(0, min(miny, (int)v1[1])); miny = max(0, min(miny, (int)v2[1]));
		maxx = min(weight - 1, max(maxx, (int)v0[0])); maxx = min(weight - 1, max(maxx, (int)v1[0])); maxx = min(weight - 1, max(maxx, (int)v2[0]));
		maxy = min(height - 1, max(maxy, (int)v0[1])); maxy = min(height - 1, max(maxy, (int)v1[1])); maxy = min(height - 1, max(maxy, (int)v2[1]));
	}

	//return (maxx - minx + 1) * (maxy - miny + 1);
	for (int x = minx; x <= maxx; x++) {
		for (int y = miny; y <= maxy; y++) {
			//cout <<"x=" << x<<"y=" << y << endl;
			
			//int sample = (v0[2] + 1) * 2;
			//cout << "z=" << v0[2] << endl;
			float x0 = x, y0 = y, z0 = v0[2];
			barycentric5(v0, v1, v2, x + 0.5, y + 0.5, x0, y0, z0);
			if (x0 < 0 || y0 < 0 || z0 < 0) continue;
			float z = 0.f;
			z = x0 * v0[2] + y0 * v1[2] + z0 * v2[2];
			//cout << "z=" << z << endl;
			if (z >= static_cast<float>(zbuff(y, x)))continue;
			zbuff(y, x) = makeRenderDepth(z);
			//cout << "zbuff(y, x) =" << zbuff(y, x) << endl;
		}
	}
	return 1;
};
int ShadowShader::drawObject( Model& mymodel, RenderDepthBuffer& zbuff, PassProfile* profile) {
	double vertexMs = 0.0;
	double rasterMs = 0.0;
	int width = zbuff.cols();
	int height = zbuff.rows();
	for (int i = 0; i < mymodel.meshes.size(); i++) {
		Mesh& mesh = mymodel.meshes[i];
		if (mesh.vertexCount() == 0 || mesh.indices.empty()) {
			continue;
		}
		auto vertexStart = std::chrono::high_resolution_clock::now();
		positions.clear();
		positions.resize(mesh.vertexCount());
		#pragma omp parallel for
		for (int j = 0; j < static_cast<int>(mesh.vertexCount()); j += 1) {
			vertexShader(mesh.vertexPosition(j), j);

		}
		auto vertexEnd = std::chrono::high_resolution_clock::now();
		vertexMs += durationMsLocal(vertexStart, vertexEnd);
		auto rasterStart = std::chrono::high_resolution_clock::now();
		for (int j = 0; j < mesh.indices.size(); j += 3) {
			Vector4f& v0 = positions[mesh.indices[j]];
			Vector4f& v1 = positions[mesh.indices[j + 1]];
			Vector4f& v2 = positions[mesh.indices[j + 2]];
			if (!shadowTriangleMaybeVisible(v0, v1, v2, width, height)) {
				continue;
			}
			this->drawTriagle( v0, v1, v2, zbuff);
		}
		auto rasterEnd = std::chrono::high_resolution_clock::now();
		rasterMs += durationMsLocal(rasterStart, rasterEnd);
	}
	if (profile != nullptr) {
		profile->vertex_ms += vertexMs;
		profile->raster_ms += rasterMs;
	}
	return 1;
};
