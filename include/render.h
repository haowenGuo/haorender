#pragma once
#include"tgaimage.h"
#include"opencv2/opencv.hpp"
#include"render.h"
#include"model.h"
#include"drawer.h"
#include"shader.h"
#include"raytrace_backend.h"
#include<memory>
#include <Eigen/Dense>
using namespace cv;
using namespace std;
using namespace Eigen;
class ProgrammableShaderProgram;
shared_ptr<TGAImage> readTga(const char* path);
enum class ShadowTechnique {
	ShadowMap = 0,
	RasterEmbree = 1
};
enum class ShadingLook {
	RealisticPbr = 0,
	StylizedPhong = 1,
	Programmable = 2
};
class render {
public:
	struct FrameProfile {
		double clear_ms = 0.0;
		double shadow_near_ms = 0.0;
		double shadow_far_ms = 0.0;
		double shadow_near_vertex_ms = 0.0;
		double shadow_near_raster_ms = 0.0;
		double shadow_far_vertex_ms = 0.0;
		double shadow_far_raster_ms = 0.0;
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
	int add_Light(const Vector4f& l, const Vector3f& color);
	void setLights(const vector<Vector4f>& dirs, const vector<Vector3f>& colors);
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
	int draw_ShadowTexture(RenderDepthBuffer& image, Model& mymodel, float extent, float depth, Matrix4f& outLV, Matrix4f& outLP, Matrix4f& outLVP, ShadowShader::PassProfile* profile = nullptr);
	int clear(Mat& image);
	int setTexture(const char* path);
	int setNMTexture(const char* path);
	int setSpecTexture(const char* path);
	int setEnvironmentMap(const char* path);
	void setBackgroundColor(const Scalar& color) { clear_color = color; }
	void setBackgroundImage(const Mat& image) { clear_background_image = image.clone(); }
	void clearBackgroundImage() { clear_background_image.release(); }
	int closeBackCut();
	int closeZbuff() { zbuff_open = 0; return 1; };
	int openShadow() { shadow_on = 1; if (complexshader) complexshader->shadow_on = 1; return 1; };
	int setSimpleShader();
	int setComplexShader(const Model& m);
	void setShadowTechnique(ShadowTechnique mode) { shadow_technique = mode; }
	ShadowTechnique getShadowTechnique() const { return shadow_technique; }
	void setShadingLook(ShadingLook mode) { shading_look = mode; }
	ShadingLook getShadingLook() const { return shading_look; }
	void setProgrammableShaderProgram(const shared_ptr<ProgrammableShaderProgram>& program);
	const shared_ptr<ProgrammableShaderProgram>& getProgrammableShaderProgram() const { return programmable_shader_program; }
	void setPhongHardSpecular(bool enabled) { phong_hard_specular = enabled ? 1 : 0; }
	bool getPhongHardSpecular() const { return phong_hard_specular != 0; }
	void setPhongToonDiffuse(bool enabled) { phong_toon_diffuse = enabled ? 1 : 0; }
	bool getPhongToonDiffuse() const { return phong_toon_diffuse != 0; }
	bool embreeAvailable() const { return ray_backend && ray_backend->available(); }
	const char* shadowTechniqueName() const {
		return shadow_technique == ShadowTechnique::RasterEmbree ? "Raster+Embree" : "ShadowMap";
	}
	const char* shadingLookName() const {
		switch (shading_look) {
		case ShadingLook::StylizedPhong:
			return "Stylized Phong";
		case ShadingLook::Programmable:
			return "Programmable";
		case ShadingLook::RealisticPbr:
		default:
			return "Realistic PBR";
		}
	}
	const FrameProfile& getLastProfile() const { return last_profile; }
	int height;
	int weight;
	int backcut=1;
	int zbuff_open = 1;
	int shadow_on = 0;
	int shadow_cascade_enabled = 1;
	int shadow_width = 2048;
	int shadow_height = 2048;
	int shadow_near_width = 1536;
	int shadow_near_height = 1536;
	int shadow_far_width = 1024;
	int shadow_far_height = 1024;
	float shadow_near_extent = 1.4f;
	float shadow_near_depth = 3.0f;
	float shadow_far_extent = 4.0f;
	float shadow_far_depth = 8.0f;
	float shadow_cascade_split = 2.2f;
	float shadow_cascade_blend = 0.6f;
	float exposure = 1.0f;
	float normal_strength = 0.7f;
	int ibl_enabled = 1;
	float ibl_diffuse_strength = 0.55f;
	float ibl_specular_strength = 0.8f;
	float sky_light_strength = 0.2f;
	vector<Vector4f> light_dir;
	vector<Vector3f> light_color;
	
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
	MyImage environment_map;
	Scalar clear_color = Scalar(155, 155, 155);
	Mat clear_background_image;
	FrameProfile last_profile;
	vector<vector<int>> tile_bins_cache;
	ShadowTechnique shadow_technique = ShadowTechnique::ShadowMap;
	ShadingLook shading_look = ShadingLook::RealisticPbr;
	int phong_hard_specular = 0;
	int phong_toon_diffuse = 0;
	int phong_use_tonemap = 0;
	int phong_primary_light_only = 1;
	float phong_secondary_light_scale = 0.12f;
	float phong_ambient_strength = 0.03f;
	float phong_specular_strength = 0.12f;
	shared_ptr<ProgrammableShaderProgram> programmable_shader_program;
	std::shared_ptr<RayTracingBackend> ray_backend;
	int ray_scene_valid = 0;
	int ray_scene_dirty = 1;
	RenderDepthBuffer cached_shadow_map_near;
	RenderDepthBuffer cached_shadow_map_far;
	Matrix4f cached_lv_near = Matrix4f::Identity();
	Matrix4f cached_lp_near = Matrix4f::Identity();
	Matrix4f cached_lvp_near = Matrix4f::Identity();
	Matrix4f cached_lv_far = Matrix4f::Identity();
	Matrix4f cached_lp_far = Matrix4f::Identity();
	Matrix4f cached_lvp_far = Matrix4f::Identity();
	int shadow_cache_valid = 0;
	int shadow_cache_dirty = 1;
};
