#pragma once
#include"model.h"
#include<opencv2/opencv.hpp>
#include <tgaimage.h>
#include<cmath>
#include <Eigen/Dense>
#include <array>
#include <memory>
#include "programmable_shader.h"
using namespace cv;
using namespace Eigen;
class RayTracingBackend;
class Shader {
public:
	virtual ~Shader() = 0;
};
class SimpleShader :public Shader {
public:
	SimpleShader(){}
	~SimpleShader() {}
	vector<Vec2f> uvs;
	vector<Vec4f> normals;
	//vector<Vec4f> positions;
	vector<Vec4f> light_dirs;
	vector<Vec4f> point_light;
	Mat uniform_M;
	Mat uniform_MIT;
	Mat uniform_V;
	shared_ptr <TGAImage> tgatexture;
	shared_ptr <TGAImage> nmtexture;
	shared_ptr <TGAImage> spectexture;
	void addLight(Vec4f light);
	void setUniform_M(const Mat & m);
	void setUniform_MIT(const Mat& m);
	void setUniform_V(const Mat& m);
	void setUV(vector<Vec2f>& uv0);
	void setNormal(vector<Vec4f>& normal0);
	//void setPointions(vector<Vec4f>& position0);
	void setTgatexture(const shared_ptr<TGAImage>& tgatext);
	void setNMTexture(const shared_ptr<TGAImage>& tgatext);
	void setSpecTexture(const shared_ptr<TGAImage>& tgatext);
	vec4 vertexShader(const vertex& vertexs, const Mat& mvp);
	int fragmentShader(Vec3f &bc, Vec3b &color);

};
class ComplexShader :public Shader {
public:
	struct RasterVertex {
		Vector4f clip_position = Vector4f::Zero();
		Vector4f screen_position = Vector4f::Zero();
		Vector4f shadow_position = Vector4f::Zero();
		Vector4f shadow_position_near = Vector4f::Zero();
		Vector4f shadow_position_far = Vector4f::Zero();
		Vector2f uv = Vector2f::Zero();
		Vector4f normal = Vector4f::Zero();
		Vector4f tangent = Vector4f::Zero();
		Vector4f bitangent = Vector4f::Zero();
		Vector3f view_position = Vector3f::Zero();
		Vector3f world_position = Vector3f::Zero();
		Vector4f world_normal = Vector4f::Zero();
		float reciprocal_w = 1.0f;
	};
	struct TileBounds {
		int minx = 0;
		int miny = 0;
		int maxx = 0;
		int maxy = 0;
	};

	ComplexShader() {}
	ComplexShader(const Model& m) {}
	ComplexShader(string& modelpath) {}
	~ComplexShader() {}
	vector<Vector4f> positions;
	vector<Vector4f> clip_positions;
	vector<Vector4f> shadowpositions;
	vector<Vector4f> shadowpositions_near;
	vector<Vector4f> shadowpositions_far;
	vector<Vector2f> uvs;
	vector<Vector4f> normals;
	vector<Vector4f> tangents;
	vector<Vector4f> bitangents;
	vector<Vector3f> view_positions;
	vector<Vector3f> world_positions;
	vector<Vector4f> world_normals;
	vector<float> reciprocal_ws;
	MaterialPbrChannelMap pbr_channel_map;
	//vector<Vector4f> positions;
	vector<Vector4f> light_dirs;
	vector<Vector3f> light_colors;
	vector<Vector4f> point_light;
	Matrix4f uniform_M = Matrix4f::Identity();
	Matrix4f uniform_MIT = Matrix4f::Identity();
	Matrix4f uniform_V = Matrix4f::Identity();
	Matrix4f uniform_P = Matrix4f::Identity();
	Matrix4f uniform_LV = Matrix4f::Identity();
	Matrix4f uniform_LP = Matrix4f::Identity();
	Matrix4f uniform_LVP = Matrix4f::Identity();
	Matrix4f uniform_LV_near = Matrix4f::Identity();
	Matrix4f uniform_LP_near = Matrix4f::Identity();
	Matrix4f uniform_LVP_near = Matrix4f::Identity();
	Matrix4f uniform_LV_far = Matrix4f::Identity();
	Matrix4f uniform_LP_far = Matrix4f::Identity();
	Matrix4f uniform_LVP_far = Matrix4f::Identity();
	vector<MyImage> textures;
	RenderDepthBuffer shadowmap;
	RenderDepthBuffer shadowmap_near;
	RenderDepthBuffer shadowmap_far;
	int shadow_on = 0;
	int ray_shadow_on = 0;
	int shadow_cascade_on = 0;
	float cascade_split = 2.5f;
	float cascade_blend = 0.5f;
	float normal_strength = 1.0f;
	float exposure = 1.0f;
	vector<Vector3f> light_dirs_world;
	vector<MyImage> environment_mips;
	vector<Vector2f> brdf_lut;
	int brdf_lut_size = 0;
	int ibl_on = 0;
	float ibl_diffuse_strength = 0.55f;
	float ibl_specular_strength = 0.8f;
	float sky_light_strength = 0.2f;
	int force_stylized_phong = 0;
	int phong_use_tonemap = 0;
	int phong_primary_light_only = 1;
	float phong_secondary_light_scale = 0.18f;
	float phong_ambient_strength = 0.035f;
	float phong_specular_strength = 0.14f;
	int phong_hard_specular = 0;
	int phong_toon_diffuse = 0;
	int force_programmable_shader = 0;
	std::shared_ptr<ProgrammableShaderProgram> programmable_shader_program;
	const RayTracingBackend* ray_backend = nullptr;
	Matrix4f uniform_M_worldIT = Matrix4f::Identity();
	Vector2f uv[3];
	Vector4f n[3];
	Vector4f position[3];
	void addLight(Vector4f light);
	void setUniform_M( const Matrix4f& m);
	void setUniform_MIT(const Matrix4f& m);
	void setUniform_V(const Matrix4f& m);
	void setUniform_P(const Matrix4f& p);
	void setUV(vector<Vector2f>& uv0);
	void setUV0(const Vector2f &uv0,int index);
	void setN(const Vector4f &n0, int index);
	void setP(const Vector4f &p0, int index);
	void setNormal(vector<Vector4f>& normal0);
	//void setPointions(vector<Vec4f>& position0);
	void setTexture(const vector<MyImage>& textures0);
	void setEnvironmentMap(const MyImage& image);
	void clearEnvironmentMap();
	void buildBrdfLut(int size = 128);

	Vector4f vertexShader(const vertex& vertexs, const Matrix4f& mvp);
	int drawTriagle_completed(Mat& im, RenderDepthBuffer& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, int indexs[3], float z0, float z1, float z2);
	int drawTriagle_clipped(Mat& im, RenderDepthBuffer& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3], const TileBounds* tile);
	int buildClippedTriangles(const int indexs[3], int width, int height, std::vector<std::array<RasterVertex, 3>>& out);
	int vertexShader2(const Vector4f& position0, const Vector2f& uv0, const Vector4f& n0, const Vector4f& t0, const Vector4f& b0, const Matrix4f& screen_mvp, const Matrix4f& clip_mvp, const Matrix4f& view_model, int t);
	int fragmentShader(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader2(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader3(float x, float y, float z , Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]);
	int fragmentShaderTriangle(float x, float y, float z, Vec3b& color, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3]);
	int fragmentShader_shadow(float x, float y, float z, float x_shadow, float y_shadow, float z_shadow, Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]);
};

class ShadowShader :public Shader {
public:
	struct PassProfile {
		double vertex_ms = 0.0;
		double raster_ms = 0.0;
	};
	ShadowShader() { positions.reserve(30000); }
	ShadowShader(const Model& m) {}
	ShadowShader(string& modelpath) {}
	~ShadowShader() {}
	vector<Vector4f> positions;
	//vector<Vector4f> positions;
	vector<Vector4f> light_dirs;
	vector<Vector4f> point_light;
	Matrix4f uniform_M;
	Matrix4f uniform_V;
	Matrix4f uniform_P;
	Matrix4f uniform_VP;
	int drawObject(Model& mymodel, RenderDepthBuffer& zbuff, PassProfile* profile = nullptr);
	int vertexShader(const Vector4f& position0, int t);
	int fragmentShader(Vector3f& bc, Vec3b& color);
	int drawTriagle( const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, RenderDepthBuffer& zbuff);
};
