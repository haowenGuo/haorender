#include "tgaimage.h"
#include "opencv2/opencv.hpp"
#include "eigen3/Eigen/Dense"
#include<vector>
#include<cmath>
#include<fstream>
#include"drawer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include"model.h"
#include"render.h"
#include <chrono>

using namespace cv;
using namespace std;
using namespace Eigen;

Vector3f eye = { 0.f,0.f, 0.f };
Vector3f centre = { 0.0f,0.0f, 0.f };
Vector3f up = { 0.0f,0.f, 0.0f };
bool drawing = false;  // 是否开始绘制
int zoomLevel = 100;
Point startPoint, endPoint;  // 矩形起点和终点
static void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    if (event == EVENT_LBUTTONDOWN) {
        drawing = true;
        startPoint = Point(x, y);
    }
    else if (event == EVENT_LBUTTONUP) {
        drawing = false;
        endPoint = Point(x, y);
        eye += Vector3f((endPoint.x - startPoint.x) * 0.001f, (endPoint.y - startPoint.y) * 0.001f, 0.0f);
        //cout << eye << endl;
    }
    else if (event == EVENT_MOUSEWHEEL) {
        // 获取滚轮增量（正值表示向上滚动，负值表示向下滚动）
        int delta = getMouseWheelDelta(flags);

        // 计算缩放比例
       
        double euclidean_dist = (eye - centre).norm();
        double scale = (delta / 1000.0) * euclidean_dist;
        //0.0f, 0.0f, -1.0f
        // 应用缩放
        eye += Vector3f(0.f, 0.f, -scale);
        
    }
}

int main(int argc, char** argv) {

	int height = 600;
	int weight = 800;
    
	//Mat image = Mat::zeros(height, weight,CV_8UC3);
    Mat image = Mat::Mat(height, weight, CV_8UC3, Scalar(155, 155, 155));
	namedWindow("main", WINDOW_NORMAL);
	resizeWindow("main", weight, height);
    setMouseCallback("main", mouseCallback, NULL);
    auto start = std::chrono::high_resolution_clock::now();
    Model mymodel;
    mymodel.modelread("../resources/luxi/luxini.pmx");
    //mymodel.modelread("../resources/BusGameMap/uploads_files_2720101_BusGameMap.obj"); 
    //mymodel.modelread("../resources/dragon/Dragon 2.5_fbx.fbx"); 
    /*int id = mymodel.TextureFromFile("african_head_diffuse.tga", "../resources");
    mymodel.meshes[0].textures.push_back(texture(id,"texture_diffuse"));
    MyImage& ima = mymodel.images[0];*/
    //shared_ptr<TGAImage> tgaim = readTga("../resources/african_head_diffuse.tga");
   
    render myrender(height, weight);
    myrender.setComplexShader(mymodel);
    myrender.set_projection(90, 5.0f / 4.0f, 0.1f, 100.0f);
    float max_size = max(abs(mymodel.maxx - mymodel.minx), abs(mymodel.maxy - mymodel.miny));
    max_size = max(max_size, abs(mymodel.maxz - mymodel.minz));
    myrender.set_scal(2.f/ max_size, 2.f / max_size, 2.f / max_size);
    myrender.set_rotate(0.f, Vector3f(1.f, 0.f, 0.f));
    //myrender.set_translation(0, 0.f, -1.0f);
    myrender.add_Light(Vector4f(-1.0f, -1.0f, -1.0f, 0.f));
    eye = { 0.f,0.f, 2.0f };
    //Vector4f forward(0.0f, 0.0f, -1.0f)
    centre = { 0.0f,0.0f, .0f };
    up = { 0.0f,1.0f, 0.0f };
    myrender.set_view(eye, centre, up);
    myrender.set_viewport(0, 0, weight, height);

    start = std::chrono::high_resolution_clock::now();
    myrender.openShadow();
    myrender.draw_completed(image, mymodel);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    cout << "渲染总执行时间: " << duration.count() << " 毫秒" << endl;
    imshow("main", image);

	//drawTriagle_tga_texture(image, weight, height, vers, texture, zbuff);
    
    while (true) {
        // 显示图像
        myrender.clear(image);
        myrender.set_view(eye, centre, up);
        myrender.draw_completed(image, mymodel);
        imshow("main", image);
        // 等待按键，超时时间为30毫秒（适用于视频处理）
        int key = waitKey(30) & 0xFF;  // 只取低8位，确保兼容性

        // 根据按键进行不同操作
        switch (key) {
        case 27:  // ESC键(ASCII码为27)退出程序
            return 0;
        case 's':  // 按下's'键保存图像
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(20.f, Vector3f(1.f, 0.f, 0.f));
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'w':  // 按下'+'键增加亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(-20.f, Vector3f(1.f, 0.f, 0.f));
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'a':  // 按下'-'键降低亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(-20.f, Vector3f(0.f, 1.f, 0.f));
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'd':  // 按下'-'键降低亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(20.f, Vector3f(0.f, 1.f, 0.f));
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case -1:  // 没有按键被按下
            break;
        default:  // 其他按键
            break;
        }
    }
	waitKey(0);
}

