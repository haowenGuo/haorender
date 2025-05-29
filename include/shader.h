#pragma once
#include"model.h"
#include<opencv2/opencv.hpp>
#include <tgaimage.h>
#include<cmath>
#include "eigen3/Eigen/Dense"
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
	ComplexShader() {}
	ComplexShader(const Model& m) {}
	ComplexShader(string& modelpath) {}
	~ComplexShader() {}
	vector<Vector4f> positions;
	vector<Vector4f> shadowpositions;
	vector<Vector2f> uvs;
	vector<Vector4f> normals;
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
	vector<MyImage> textures;
	MatrixXd shadowmap;
	int shadow_on = 0;
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
	int drawTriagle_completed(Mat& im, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture, int indexs[3], float z0, float z1, float z2);
	int vertexShader2(const Vector4f& position0, const Vector2f& uv0, const Vector4f& n0, const Matrix4f& mvp, int t);
	int fragmentShader(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader2(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture);
	int fragmentShader3(float x, float y, float z , Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]);
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