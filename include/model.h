#pragma once
#include <iostream>
#include<vector>
#include<cmath>
#include<fstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include<string>
#include<opencv2/opencv.hpp>
#include<stb_image.h>
#include<tgaimage.h>
#include "eigen3/Eigen/Dense"
using namespace std;
using namespace cv;
using namespace Eigen;
const double MY_PI=2.1415926;
Mat get_view_matrix(Vec3f eye_pos, Vec3f centre, Vec3f up);
Mat get_model_matrix( float dx, float dy, float dz);
Mat get_model_matrix(float angle, const Vec3f& axis);
Mat get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar);
struct MyColor {
public:
	union {
		struct {
			unsigned char r, g, b, a;
		};
		unsigned char raw[4];
		unsigned int val;
	};
	int channels;
	MyColor() {}
	MyColor(const unsigned char* p, int channels0) : val(0), channels(channels0) {
		for (int i = 0; i < channels; i++) {
			raw[i] = p[i];
		}
	}
	MyColor(unsigned char b0, unsigned char g0, unsigned char  r0, unsigned char  a0, int channels0):b(b0),g(b0),r(b0),a(b0),channels(channels0) {}
	MyColor(int val0, int channels0):val(val0), channels(channels0) {}
	MyColor& operator =(const MyColor& c) {
		if (this != &c) {
			channels = c.channels;
			val = c.val;
		}
		return *this;
	}

};
class MyImage {
public:
	unsigned char* data=nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
	MyImage() {  }
	MyImage(const MyImage& p) {
		if (this != &p) {
			delete[] data;
			width = p.width;
			height = p.height;
			channels = p.channels;
			unsigned long nbytes = width * height * channels;
			data = new unsigned char[nbytes];
			memcpy(data, p.data, nbytes);
		}
	};
	MyImage(const MatrixXd& p) {
		delete[] data;
		width = p.cols();
		height = p.rows();
		channels = 1;
		unsigned long nbytes = width * height * channels;
		try {
			data = new unsigned char[nbytes];
		}
		catch (const std::bad_alloc& e) {
			width = 0;
			height = 0;
			channels = 0;
			throw std::runtime_error("Memory allocation failed for image data");
		}

		// 转换矩阵数据到图像格式
		// 使用Eigen的系数访问器进行高效遍历
		for (Eigen::Index y = 0; y < height; ++y) {
			for (Eigen::Index x = 0; x < width; ++x) {
				// 对矩阵元素进行范围限制并转换为unsigned char
				const double value = p(y, x);
				data[x + y * width] = static_cast<unsigned char>(
					std::max(0.0, std::min(255.0, value * 255.0))
					);
			}
		}
	};
	MyImage(MyImage&& p) noexcept { 
			if (this != &p) {
				delete[] data;
				data = p.data;
				width = p.width;
				height = p.height;
				channels = p.channels;
				p.data = nullptr;
			}
	}
	MyImage& operator=(MyImage&& p) noexcept {
		if (this != &p) {
			delete[] data;
			data = p.data;
			width = p.width;
			height = p.height;
			channels = p.channels;
			p.data = nullptr;
		}
		return *this;
	}
	MyImage(unsigned char* data0, int width0, int height0, int channels0) {
		width = width0;
		height = height0;
		channels = channels0;
		unsigned long nbytes = width * height * channels;
		data = new unsigned char[nbytes];
		memcpy(data, data0, nbytes);
	}
	MyImage(const string& path) {}

	MyColor get(int x, int y) { 
		if (!data || x < 0 || y < 0 || x >= width || y >= height) {
			return MyColor();
		}
		return MyColor(data + (x + y * width) * channels, channels);
	}
	bool set(int x, int y, MyColor c) {
		for(int i=0;i<channels;i++)
		*(data + (x + y * width) * channels+i) = c.raw[i];
	};
	~MyImage() {
		if (data) delete[] data; 
	};
	MyImage& operator =(const MyImage& img) {
		if (this != &img) {
			if (data) delete[] data;
			width = img.width;
			height = img.height;
			channels = img.channels;
			unsigned long nbytes = width * height * channels;
			data = new unsigned char[nbytes];
			memcpy(data, img.data, nbytes);
		}
		return *this;
	};
	void operator =(MatrixXd& img) {
		if (data) delete[] data;
		width = img.cols();
		height = img.rows();
		channels = 1;
		unsigned long nbytes = width * height * channels;
		try {
			data = new unsigned char[nbytes];
		}
		catch (const std::bad_alloc& e) {
			width = 0;
			height = 0;
			channels = 0;
			throw std::runtime_error("Memory allocation failed for image data");
		}

		// 转换矩阵数据到图像格式
		// 使用Eigen的系数访问器进行高效遍历
		for (Eigen::Index y = 0; y < height; ++y) {
			for (Eigen::Index x = 0; x < width; ++x) {
				// 对矩阵元素进行范围限制并转换为unsigned char
				const double value = img(y, x);
				data[x + y * width] = static_cast<unsigned char>(
					std::max(0.0, std::min(255.0, value * 255.0))
					);
			}
		}
	};
	int get_width()  const { return width; };
	int get_height()  const { return height; };
	int get_bytespp()  const { return channels; };
	void clear(){ memset((void*)data, 0, width * height * channels); };
};

struct vec2;
struct vec3;
struct vec4;
struct vec4 {
	float x;
	float y;
	float z;
	float w;
	vec4() { x = 0; y = 0; z = 0; w = 1; }
	vec4(vec3& v3); 
	vec4(const vec4& v4) :x(v4.x), y(v4.y), z(v4.z), w(v4.w) {}
	vec4(float x0, float y0, float z0, float w0) :x(x0), y(y0), z(z0), w(w0) {}
	vec4 operator+(const vec4& v) const {
		return vec4(x + v.x, y + v.y, z + v.z, w + v.w);
	}
	vec4 operator*(const Mat& mat) const {
		float w0 = (x * mat.at<float>(3, 0) + y * mat.at<float>(3, 1) + z * mat.at<float>(3, 2) + w * mat.at<float>(3, 3));
		if (w0 == 0) { return *this; }
		return vec4((x * mat.at<float>(0,0)+ y * mat.at<float>(0, 1)+z * mat.at<float>(0, 2)+w * mat.at<float>(0, 3))/w0,
			(x * mat.at<float>(1, 0) + y * mat.at<float>(1, 1)+ z * mat.at<float>(1, 2) + w * mat.at<float>(1, 3))/w0,
			(x * mat.at<float>(2, 0) + y * mat.at<float>(2, 1) + z * mat.at<float>(2, 2) + w * mat.at<float>(2, 3))/w0,
			1);
	}
	vec4 operator-(const vec4& v) const {
		return vec4(x - v.x, y - v.y, z - v.z, w - v.w);
	}
};
struct vec3 {
	float x;
	float y;
	float z;
	vec3() { x = 0; y = 0; z = 0; }
	vec3(const vec3& v3) :x(v3.x), y(v3.y),z(v3.z) {}
	vec3(float x0, float y0, float z0) :x(x0), y(y0), z(z0) {}
	vec3(const vec4& v4) :x(v4.x), y(v4.y), z(v4.z) {}
	vec3 operator+(const vec3& v) const {
		return vec3(x + v.x, y + v.y, z + v.z);
	}
	vec3 operator-(const vec3& v) const {
		return vec3(x - v.x, y - v.y, z - v.z);
	}
	vec3 operator=(const vec4& v4) const {
		return vec3(v4);
	}
};
struct vec2 {
	float x;
	float y;
	vec2() { x = 0; y = 0; }
	vec2(const vec2& v2) :x(v2.x), y(v2.y) {}
	vec2(float x0, float y0) :x(x0), y(y0) {}
	vec2(const vec3& v3) :x(v3.x), y(v3.y) {}
	vec2 operator+(const vec2& v) const {
		return vec2(x + v.x, y + v.y);
	}
	vec2 operator-(const vec2& v) const {
		return vec2(x - v.x, y - v.y);
	}
	vec2 operator=(const vec3& v3) const {
		return vec2(v3);
	}
};


struct vertex {
	Vector4f v;
	Vector4f n;
	vector<Vector2f> textures;
	vertex() {
	}
	vertex(const vertex& v0) {
		v = v0.v;
		n = v0.n;
		textures = v0.textures;
	}
	vertex(Vector4f v0, Vector4f n0,vector<Vector2f> textures0) {
		v = v0;
		n = n0;
		textures = textures0;
	}
};
struct texture {
	int id;
	string type;
	texture() { id = -1; type = ""; }
	texture(int id0, string type0) { id = id0; type = type0; }
};
class Mesh {
public:
	Mesh() {

	}
	Mesh(vertex & ve) {
		vertices.push_back(ve);
	}
	Mesh(vector<vertex> vertices, vector<unsigned int> indices, vector<texture> textures) {
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;
	}
	
	~Mesh() {

	}
	vector<vertex> vertices;
	vector<unsigned int> indices;
	vector<texture>      textures;

	
};

class Model {
public:
	Model() {

	}
	Model(Mesh& me) {
		meshes.push_back(me);
	}
	~Model() {

	}
	int modelread(const string& filepath);
	int processNode(aiNode* node, const aiScene* scene);
	int TextureFromFile(const char* path, const string directory);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	vector<texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName);
	vector<Mesh> meshes;
	vector<MyImage> images;
	string directory;
	float maxx=-10000.f, minx = 10000.f, maxz = -10000.f, minz = 10000.f, maxy = -10000.f, miny = 10000.f;
};

Model modelread(const string& filepath);
