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

int main(int argc, char** argv) {
    
    
	int height = 600;
	int weight = 800;
	Mat image = Mat::zeros(height, weight,CV_8UC3);
	namedWindow("main", WINDOW_NORMAL);
	resizeWindow("main", weight, height);

    auto start = std::chrono::high_resolution_clock::now();
    Model mymodel;
    //mymodel.modelread("../resources/luxi/luxini.pmx");
    mymodel.modelread("../resources/dragon/Dragon 2.5_fbx.fbx"); 
    render myrender(height, weight);
    myrender.setComplexShader(mymodel);
    myrender.set_projection(90, 5.0f / 4.0f, 0.1f, 2000.0f);
    myrender.set_rotate(0.f, Vector3f(1.f, 0.f, 0.f));
    myrender.set_translation(0, 0.f, -1.0f);
    myrender.add_Light(Vector4f(-1.0f, -1.0f, -1.0f, 0.f));
  
    Vector3f eye = { 0.1f,0.1f, 100.0f };
    Vector3f centre = { 0.0f,0.0f, -1.0f };
    Vector3f up = { 0.0f,1.0f, 0.0f };
    myrender.set_view(eye, centre, up);
    myrender.set_viewport(0, 0, weight, height);

    start = std::chrono::high_resolution_clock::now();
    myrender.draw_completed(image, mymodel);
    
   /* start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < height; i++)
    {
        for (int t = 0; t < weight*2; t++) {
            for (int p = 0; p < 2000; p++);
            //image.at<Vec3b>(i, t) = Vec3b(0.f, 0.f, 0.f);
            //float a = 120.5121f;
            //float b = 26.52213256f;
            //float c = a*b;
            //c = c + b;
            //for (int p = 0; p < 1000; p++);
            //fun(Vec3b(0.f, 0.f, 0.f));
        }
        
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    cout << "渲染总执行时间: " << duration.count() << " 毫秒" << endl;*/
    imshow("main", image);
	//drawTriagle_tga_texture(image, weight, height, vers, texture, zbuff);
    
    while (true) {
        // 显示图像

        // 等待按键，超时时间为30毫秒（适用于视频处理）
        int key = waitKey(30) & 0xFF;  // 只取低8位，确保兼容性

        // 根据按键进行不同操作
        switch (key) {
        case 27:  // ESC键(ASCII码为27)退出程序
            cout << "ESC键被按下，程序退出" << endl;
            return 0;
        case 's':  // 按下's'键保存图像
            eye += Vector3f(0.0f, -1.f, 0.0f) ;
            myrender.clear(image);
            myrender.set_rotate(20.f, Vector3f(1.f, 0.f, 0.f));
            //myrender.set_view(eye, centre, up);
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'w':  // 按下'+'键增加亮度
            eye += Vector3f(0.0f, 1.f, 0.0f);
            myrender.clear(image);
            myrender.set_rotate(-20.f, Vector3f(1.f, 0.f, 0.f));
            //myrender.set_view(eye, centre, up);
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'a':  // 按下'-'键降低亮度
            eye += Vector3f(20.f, 0.0f, 0.0f);
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case 'd':  // 按下'-'键降低亮度
            eye += Vector3f(-20.f, 0.0f, 0.0f);
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.draw_completed(image, mymodel);
            imshow("main", image);
            break;
        case -1:  // 没有按键被按下
            break;
        default:  // 其他按键
            break;
        }
    }
	//myrender.draw_0(image, mymodel);
	//imshow("main", image);
	waitKey(0);
    
}

