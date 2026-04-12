#include "tgaimage.h"
#include "opencv2/opencv.hpp"
#include <Eigen/Dense>
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
#include <string>

using namespace cv;
using namespace std;
using namespace Eigen;

namespace {
Vector3f eye = { 0.f,0.f, 3.f };
Vector3f centre = { 0.0f,0.0f, 0.f };
Vector3f up = { 0.0f,1.0f, 0.0f };
const Vector3f kWorldUp(0.0f, 1.0f, 0.0f);

struct OrbitCameraState {
    Vector3f target = Vector3f::Zero();
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 3.0f;
    bool rotating = false;
    bool panning = false;
    Point lastMouse = Point(0, 0);
};

OrbitCameraState cameraState;

float clampPitch(float pitch) {
    const float pitchLimit = 1.5f;
    return std::max(-pitchLimit, std::min(pitchLimit, pitch));
}

void updateCameraPose() {
    const float cosPitch = std::cos(cameraState.pitch);
    const Vector3f offset(
        cameraState.distance * cosPitch * std::sin(cameraState.yaw),
        cameraState.distance * std::sin(cameraState.pitch),
        cameraState.distance * cosPitch * std::cos(cameraState.yaw));

    eye = cameraState.target + offset;
    centre = cameraState.target;

    Vector3f forward = (centre - eye).normalized();
    Vector3f right = forward.cross(kWorldUp);
    if (right.squaredNorm() <= 1e-8f) {
        right = Vector3f(1.0f, 0.0f, 0.0f);
    }
    else {
        right.normalize();
    }
    up = right.cross(forward).normalized();
}

void resetCamera(const Vector3f& target, float distance) {
    cameraState.target = target;
    cameraState.yaw = 0.0f;
    cameraState.pitch = 0.0f;
    cameraState.distance = std::max(distance, 0.3f);
    updateCameraPose();
}

void applyPbrChannelMap(Model& model, const MaterialPbrChannelMap& map) {
    for (auto& mesh : model.meshes) {
        mesh.pbr_channel_map = map;
    }
    cout << "[PBR] Channel map: metallic=" << map.metallic_channel
        << " roughness=" << map.roughness_channel
        << " ao=" << map.ao_channel
        << " emissive=" << map.emissive_channel << endl;
}

void printShadowTechnique(const render& renderer) {
    cout << "[Shadow] mode=" << renderer.shadowTechniqueName()
        << " embreeAvailable=" << (renderer.embreeAvailable() ? 1 : 0);
    if (renderer.getShadowTechnique() == ShadowTechnique::RasterEmbree && !renderer.embreeAvailable()) {
        cout << " fallback=ShadowMap";
    }
    cout << endl;
}

void printFrameProfile(const render::FrameProfile& profile) {
    cout << "[Profiler] clear=" << profile.clear_ms
        << " ms, shadow near=" << profile.shadow_near_ms
        << " ms (v=" << profile.shadow_near_vertex_ms
        << ", r=" << profile.shadow_near_raster_ms << ")"
        << " ms, shadow far=" << profile.shadow_far_ms
        << " ms (v=" << profile.shadow_far_vertex_ms
        << ", r=" << profile.shadow_far_raster_ms << ")"
        << " ms, vertex=" << profile.vertex_ms
        << " ms, clip+bin=" << profile.clip_bin_ms
        << " ms, raster+shade=" << profile.raster_shade_ms
        << " ms, render total=" << profile.render_total_ms
        << " ms, imshow=" << profile.imshow_ms
        << " ms" << endl;
}
}

static void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    const Point currentPoint(x, y);

    if (event == EVENT_LBUTTONDOWN) {
        cameraState.rotating = true;
        cameraState.lastMouse = currentPoint;
    }
    else if (event == EVENT_RBUTTONDOWN) {
        cameraState.panning = true;
        cameraState.lastMouse = currentPoint;
    }
    else if (event == EVENT_LBUTTONUP) {
        cameraState.rotating = false;
    }
    else if (event == EVENT_RBUTTONUP) {
        cameraState.panning = false;
    }
    else if (event == EVENT_MOUSEMOVE) {
        const Point delta = currentPoint - cameraState.lastMouse;
        cameraState.lastMouse = currentPoint;

        if (cameraState.rotating && (flags & EVENT_FLAG_LBUTTON)) {
            const float orbitSensitivity = 0.008f;
            cameraState.yaw -= delta.x * orbitSensitivity;
            cameraState.pitch = clampPitch(cameraState.pitch - delta.y * orbitSensitivity);
            updateCameraPose();
        }
        else if (cameraState.panning && (flags & EVENT_FLAG_RBUTTON)) {
            Vector3f forward = (centre - eye).normalized();
            Vector3f right = forward.cross(up);
            if (right.squaredNorm() > 1e-8f) {
                right.normalize();
            }
            Vector3f cameraUp = up.normalized();
            const float panScale = std::max(cameraState.distance, 0.5f) * 0.0015f;
            const float deltaX = static_cast<float>(delta.x);
            const float deltaY = static_cast<float>(delta.y);
            cameraState.target += (-deltaX * right + deltaY * cameraUp) * panScale;
            updateCameraPose();
        }
    }
    else if (event == EVENT_MOUSEWHEEL) {
        int delta = getMouseWheelDelta(flags);
        float zoomFactor = std::pow(0.88f, delta / 120.0f);
        cameraState.distance = std::max(0.3f, std::min(50.0f, cameraState.distance * zoomFactor));
        updateCameraPose();
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
   //string modelPath = argc > 1 ? argv[1] : "../Resources/风影/风影.pmx";
    string modelPath = argc > 1 ? argv[1] : "../Resources/MAIFU/IF.fbx";
    if (mymodel.modelread(modelPath) != 1) {
        cerr << "模型加载失败: " << modelPath << endl;
        return -1;
    }
    MaterialPbrChannelMap pbrMap;
    pbrMap.metallic_channel = 2;
    pbrMap.roughness_channel = 1;
    pbrMap.ao_channel = 0;
    pbrMap.emissive_channel = 0;
    applyPbrChannelMap(mymodel, pbrMap);
    //mymodel.modelread("../resources/BusGameMap/uploads_files_2720101_BusGameMap.obj"); 
    //mymodel.modelread("../resources/dragon/Dragon 2.5_fbx.fbx"); 
    /*int id = mymodel.TextureFromFile("african_head_diffuse.tga", "../resources");
    mymodel.meshes[0].textures.push_back(texture(id,"texture_diffuse"));
    MyImage& ima = mymodel.images[0];*/
    //shared_ptr<TGAImage> tgaim = readTga("../resources/african_head_diffuse.tga");
   
    render myrender(height, weight);
    myrender.setComplexShader(mymodel);
    myrender.set_projection(90, static_cast<float>(weight) / static_cast<float>(height), 0.1f, 100.0f);
    float max_size = max(abs(mymodel.maxx - mymodel.minx), abs(mymodel.maxy - mymodel.miny));
    max_size = max(max_size, abs(mymodel.maxz - mymodel.minz));
    if (max_size <= 1e-6f) {
        max_size = 1.f;
    }
    Vector3f modelCenter(
        0.5f * (mymodel.maxx + mymodel.minx),
        0.5f * (mymodel.maxy + mymodel.miny),
        0.5f * (mymodel.maxz + mymodel.minz));
    myrender.set_translation(-modelCenter[0], -modelCenter[1], -modelCenter[2]);
    myrender.set_scal(2.f/ max_size, 2.f / max_size, 2.f / max_size);
    myrender.set_rotate(0.f, Vector3f(1.f, 0.f, 0.f));
    //myrender.set_translation(0, 0.f, -1.0f);
    myrender.add_Light(Vector4f(-1.0f, -1.0f, -1.0f, 0.f));
    resetCamera(Vector3f::Zero(), 3.0f);
    myrender.set_view(eye, centre, up);
    myrender.set_viewport(0.0f, 0.0f, static_cast<float>(weight), static_cast<float>(height));

    myrender.openShadow();
    printShadowTechnique(myrender);
    start = std::chrono::high_resolution_clock::now();
    myrender.draw_completed(image, mymodel);
    auto showStart = std::chrono::high_resolution_clock::now();
    imshow("main", image);
    auto showEnd = std::chrono::high_resolution_clock::now();
    render::FrameProfile startupProfile = myrender.getLastProfile();
    startupProfile.clear_ms = 0.0;
    startupProfile.imshow_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(showEnd - showStart).count();
    printFrameProfile(startupProfile);

	//drawTriagle_tga_texture(image, weight, height, vers, texture, zbuff);
    
    while (true) {
        // 显示图像
        auto clearStart = std::chrono::high_resolution_clock::now();
        myrender.clear(image);
        auto clearEnd = std::chrono::high_resolution_clock::now();
        myrender.set_view(eye, centre, up);
        myrender.draw_completed(image, mymodel);
        auto showStartLoop = std::chrono::high_resolution_clock::now();
        imshow("main", image);
        auto showEndLoop = std::chrono::high_resolution_clock::now();
        render::FrameProfile frameProfile = myrender.getLastProfile();
        frameProfile.clear_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(clearEnd - clearStart).count();
        frameProfile.imshow_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(showEndLoop - showStartLoop).count();
        printFrameProfile(frameProfile);
        // 等待按键，超时时间为30毫秒（适用于视频处理）
        int key = waitKey(30) & 0xFF;  // 只取低8位，确保兼容性

        // 根据按键进行不同操作
        switch (key) {
        case 27:  // ESC键(ASCII码为27)退出程序
            return 0;
        case 'r':
            resetCamera(Vector3f::Zero(), 3.0f);
            break;
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
        case '1': {
            MaterialPbrChannelMap map;
            map.metallic_channel = 2;
            map.roughness_channel = 1;
            map.ao_channel = 0;
            map.emissive_channel = 0;
            applyPbrChannelMap(mymodel, map);
            break;
        }
        case '2': {
            MaterialPbrChannelMap map;
            map.metallic_channel = 1;
            map.roughness_channel = 0;
            map.ao_channel = 0;
            map.emissive_channel = 0;
            applyPbrChannelMap(mymodel, map);
            break;
        }
        case '3': {
            MaterialPbrChannelMap map;
            map.metallic_channel = 0;
            map.roughness_channel = 1;
            map.ao_channel = 0;
            map.emissive_channel = 0;
            applyPbrChannelMap(mymodel, map);
            break;
        }
        case '4': {
            myrender.setShadowTechnique(ShadowTechnique::ShadowMap);
            printShadowTechnique(myrender);
            break;
        }
        case '5': {
            myrender.setShadowTechnique(ShadowTechnique::RasterEmbree);
            printShadowTechnique(myrender);
            break;
        }
        case -1:  // 没有按键被按下
            break;
        default:  // 其他按键
            break;
        }
    }
	waitKey(0);
}
