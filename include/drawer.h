#pragma once
#include<iostream>
#include "opencv2/opencv.hpp"
#include <tgaimage.h>
#include"model.h"
#include"opencv2/opencv.hpp"
#include"shader.h"
using namespace cv;
using namespace std;

Vec3f barycentric(const vector<vector<int>>& pts, Vec2i P);
Vec3f barycentric2(const vector<vector<float>>& pts, Vec2i P);
Vec3f barycentric3(const vector<vec4>& pts, Vec2i P);
Vector3f barycentric4(const vector<Vector4f>& pts, Vector2i P);
int drawLine(Mat& im, int weight, int height, const vector<vector<int>>& vers);
int drawLine(Mat& im, int weight, int height, const vector<vector<int>>& vers, const Vec3b& color);
int drawTriagle(Mat& im, int weight, int height, const vector<vector<int>>& vers);
int drawTriagleProfile(Mat& im, int weight, int height, const vector<vector<int>>& ves, const Vec3b& color);
int drawTriagle(Mat& im, int weight, int height, const vector<vector<int>>& vers, const Vec3b& color);


int drawTriagle2(Mat& im, int weight, int height, const vector<vector<int>>& ves, const Vec3b& color, Mat &zbuff);
int drawTriagle_tga_texture(Mat& im, int weight, int height , Mat& mvp,vector<vertex>& ves, TGAImage& texture, Mat& zbuff,Vec3f light_dir);
int drawTriagle_shader(Mat& im, const vector<vec4>& vertexs, SimpleShader &shader, Mat& zbuff);
int drawTriagle_completed(Mat& im, const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, ComplexShader& shader, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture,int indexs[3]);