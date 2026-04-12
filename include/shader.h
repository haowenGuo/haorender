#pragma once
#include"model.h"
#include<opencv2/opencv.hpp>
#include <tgaimage.h>
#include<cmath>
#include <Eigen/Dense>
#include <array>
using namespace cv;
using namespace Eigen;
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
	vector<float> reciprocal_ws;
	MaterialPbrChannelMap pbr_channel_map;
	//vector<Vector4f> positions;
	vector<Vector4f> light_dirs;
	vector<Vector4f> point_light;
	Matrix4f uniform_M;
	Matrix4f uniform_MIT;
	Matrix4f uniform_V;
	Matrix4f uniform_P;
	Matrix4f uniform_LV ;
	Matrix4f uniform_LP ;
	Matrix4f uniform_LVP;
	Matrix4f uniform_LV_near;
	Matrix4f uniform_LP_near;
	Matrix4f uniform_LVP_near;
	Matrix4f uniform_LV_far;
	Matrix4f uniform_LP_far;
	Matrix4f uniform_LVP_far;
	vector<MyImage> textures;
	MatrixXd shadowmap;
	MatrixXd shadowmap_near;
	MatrixXd shadowmap_far;
	int shadow_on = 0;
	int shadow_cascade_on = 0;
	float cascade_split = 2.5f;
	float cascade_blend = 0.5f;
	float normal_strength = 1.0f;
	float exposure = 1.0f;
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

	Vector4f vertexShader(const vertex& vertexs, const Matrix4f& mvp);
	int drawTriagle_completed(Mat& im, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, int indexs[3], float z0, float z1, float z2);
	int drawTriagle_clipped(Mat& im, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3], const TileBounds* tile);
	int buildClippedTriangles(const int indexs[3], int width, int height, std::vector<std::array<RasterVertex, 3>>& out);
	int vertexShader2(const Vector4f& position0, const Vector2f& uv0, const Vector4f& n0, const Vector4f& t0, const Vector4f& b0, const Matrix4f& mvp, int t);
	int fragmentShader(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader2(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader3(float x, float y, float z , Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]);
	int fragmentShaderTriangle(float x, float y, float z, Vec3b& color, int difftexture, int nmtexture, int spectexture, int baseColorTex, int metallicTex, int roughnessTex, int metallicRoughnessTex, int aoTex, int emissiveTex, const RasterVertex triangle[3]);
	int fragmentShader_shadow(float x, float y, float z, float x_shadow, float y_shadow, float z_shadow, Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]);
};

class ShadowShader :public Shader {
public:
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
	int drawObject(Model& mymodel, MatrixXd& zbuff);
	int vertexShader(const Vector4f& position0, int t);
	int fragmentShader(Vector3f& bc, Vec3b& color);
	int drawTriagle( const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, MatrixXd& zbuff);
};
