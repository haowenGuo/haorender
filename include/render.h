#pragma once
#include"tgaimage.h"
#include"opencv2/opencv.hpp"
#include"render.h"
#include"model.h"
#include"drawer.h"
#include"shader.h"
#include<memory>
#include <Eigen/Dense>
using namespace cv;
using namespace std;
using namespace Eigen;
shared_ptr<TGAImage> readTga(const char* path);
class render {
public:
	struct FrameProfile {
		double clear_ms = 0.0;
		double shadow_near_ms = 0.0;
		double shadow_far_ms = 0.0;
		double vertex_ms = 0.0;
		double clip_bin_ms = 0.0;
		double raster_shade_ms = 0.0;
		double imshow_ms = 0.0;
		double render_total_ms = 0.0;
	};
	render(){
		height = 600;
		weight = 800;
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
	}
	render(int h,int w):height(h),weight(w) {
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
	}
	render(int height0, int weight0, const char* path) :height(height0), weight(weight0) {
		texture = readTga(path);
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
	}
	render(int height0, int weight0, const char* path0, const char* path1) :height(height0), weight(weight0) {
		texture = readTga(path0);
		nmtexture = readTga(path1);
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
	}
	render(int height0, int weight0, const char* path0, const char* path1, const char* path2) :height(height0), weight(weight0) {
		texture = readTga(path0);
		nmtexture = readTga(path1);
		spectexture = readTga(path2);
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));

	}
	render(const render & ren) :height(ren.height), weight(ren.weight) {
		texture =ren.texture;
		nmtexture = ren.nmtexture;
		spectexture = ren.spectexture;
		zbuff = RenderDepthBuffer(height, weight);
		zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
	}
	int add_Light(const Vector4f& l);
	int set_translation(float dx, float dy, float dz);
	int set_scal(float dx, float dy, float dz);
	int set_rotate(float angel, const Vector3f& axis);
	int set_view(Vector3f eye_pos, Vector3f centre, Vector3f up);

	Matrix4f get_view(Vector3f eye_pos, Vector3f centre, Vector3f up);

	Matrix4f get_view(Vector3f& light_dir);

	int set_projection(float eye_fov, float aspect_ratio, float zNear, float zFar);
	Matrix4f getOrthographic(float left, float right, float bottom, float top, float, float);
	int set_viewport(float x, float y, float w, float h);
	Matrix4f get_viewport(float x, float y, float w, float h);
	Matrix4f get_viewport2(float x, float y, float w, float h, float near, float far);
	//int draw_simple(Mat &image,Model &model);
	int draw_completed(Mat& image, Model& mymodel);
	int draw_ShadowTexture(RenderDepthBuffer& image, Model& mymodel);
	int draw_ShadowTexture(RenderDepthBuffer& image, Model& mymodel, float extent, float depth, Matrix4f& outLV, Matrix4f& outLP, Matrix4f& outLVP);
	int clear(Mat& image);
	int setTexture(const char* path);
	int setNMTexture(const char* path);
	int setSpecTexture(const char* path);
	int closeBackCut();
	int closeZbuff() { zbuff_open = 0; return 1; };
	int openShadow() { shadow_on = 1; if (complexshader) complexshader->shadow_on = 1; return 1; };
	int setSimpleShader();
	int setComplexShader(const Model& m);
	const FrameProfile& getLastProfile() const { return last_profile; }
	int height;
	int weight;
	int backcut=1;
	int zbuff_open = 1;
	int shadow_on = 0;
	int shadow_width = 2048;
	int shadow_height = 2048;
	vector<Vector4f> light_dir;
	
	RenderDepthBuffer zbuff;
	Matrix4f model = Matrix4f::Identity();
	Matrix4f view = Matrix4f::Identity();
	Matrix4f projection = Matrix4f::Identity();
	Matrix4f viewport = Matrix4f::Identity();
	shared_ptr<SimpleShader> myshader;
	shared_ptr<ComplexShader> complexshader;
	shared_ptr<TGAImage> texture;
	shared_ptr<TGAImage> nmtexture ;
	shared_ptr<TGAImage> spectexture ;
	FrameProfile last_profile;
	vector<vector<int>> tile_bins_cache;
};
