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
#include <sstream>
#include <filesystem>
#include <xmmintrin.h>
#include <pmmintrin.h>

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

void enableFastFpModes() {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}

void drawStatusOverlay(Mat& image, const render& renderer) {
    std::ostringstream oss;
    oss << "Shadow: " << renderer.shadowTechniqueName()
        << " | Embree: " << (renderer.embreeAvailable() ? "ON" : "OFF");
    if (renderer.getShadowTechnique() == ShadowTechnique::RasterEmbree && !renderer.embreeAvailable()) {
        oss << " (Fallback ShadowMap)";
    }

    const string text = oss.str();
    const int fontFace = FONT_HERSHEY_SIMPLEX;
    const double fontScale = 0.55;
    const int thickness = 1;
    int baseline = 0;
    Size textSize = getTextSize(text, fontFace, fontScale, thickness, &baseline);
    Rect panel(8, 8, textSize.width + 16, textSize.height + 16);
    rectangle(image, panel, Scalar(20, 20, 20), FILLED);
    rectangle(image, panel, Scalar(210, 210, 210), 1);
    putText(image, text, Point(panel.x + 8, panel.y + panel.height - 8), fontFace, fontScale, Scalar(240, 240, 240), thickness, LINE_AA);
}

string findDefaultEnvironmentMap() {
    const vector<string> candidates = {
        "../../Resources/IBL/studio_small_08_1k.hdr",
        "../../Resources/IBL/blouberg_sunrise_2_1k.hdr",
        "../../Resources/IBL/royal_esplanade_1k.jpg",
        "../Resources/IBL/studio_small_08_1k.hdr",
        "Resources/IBL/studio_small_08_1k.hdr",
        "../Resources/IBL/blouberg_sunrise_2_1k.hdr",
        "Resources/IBL/blouberg_sunrise_2_1k.hdr",
        "../Resources/IBL/royal_esplanade_1k.jpg",
        "Resources/IBL/royal_esplanade_1k.jpg"
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path) && std::filesystem::file_size(path) > 1024) {
            return path;
        }
    }
    return "";
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
    enableFastFpModes();

	int height = 600;
	int weight = 800;
    
	//Mat image = Mat::zeros(height, weight,CV_8UC3);
    Mat image = Mat::Mat(height, weight, CV_8UC3, Scalar(155, 155, 155));
	namedWindow("main", WINDOW_NORMAL);
	resizeWindow("main", weight, height);
    setMouseCallback("main", mouseCallback, NULL);
    auto start = std::chrono::high_resolution_clock::now();
    Model mymodel; 
    string modelPath = "../Resources/MAIFU/IF.fbx";
    ShadowTechnique startupShadowTechnique = ShadowTechnique::ShadowMap;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        static const string shadowTechniquePrefix = "--shadow-technique=";
        if (arg.rfind(shadowTechniquePrefix, 0) == 0) {
            string mode = arg.substr(shadowTechniquePrefix.size());
            if (mode == "embree" || mode == "raster-embree") {
                startupShadowTechnique = ShadowTechnique::RasterEmbree;
            }
            else {
                startupShadowTechnique = ShadowTechnique::ShadowMap;
            }
        }
        else if (!arg.empty() && arg[0] != '-') {
            modelPath = arg;
        }
    }
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
    myrender.add_Light(Vector4f(-1.0f, -1.0f, -1.0f, 0.f), Vector3f(2.6f, 2.4f, 2.2f));
    myrender.add_Light(Vector4f(0.8f, -0.35f, -0.2f, 0.f), Vector3f(0.45f, 0.50f, 0.65f));
    myrender.add_Light(Vector4f(-0.2f, -0.5f, 0.9f, 0.f), Vector3f(0.25f, 0.22f, 0.20f));
    string envPath = findDefaultEnvironmentMap();
    if (!envPath.empty()) {
        myrender.setEnvironmentMap(envPath.c_str());
    }
    else {
        cout << "[IBL] 未找到默认环境贴图，将使用天空光 fallback" << endl;
    }
    resetCamera(Vector3f::Zero(), 3.0f);
    myrender.set_view(eye, centre, up);
    myrender.set_viewport(0.0f, 0.0f, static_cast<float>(weight), static_cast<float>(height));

    myrender.openShadow();
    myrender.setShadowTechnique(startupShadowTechnique);
    printShadowTechnique(myrender);
    start = std::chrono::high_resolution_clock::now();
    myrender.draw_completed(image, mymodel);
    drawStatusOverlay(image, myrender);
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
        drawStatusOverlay(image, myrender);
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
            drawStatusOverlay(image, myrender);
            imshow("main", image);
            break;
        case 'w':  // 按下'+'键增加亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(-20.f, Vector3f(1.f, 0.f, 0.f));
            myrender.draw_completed(image, mymodel);
            drawStatusOverlay(image, myrender);
            imshow("main", image);
            break;
        case 'a':  // 按下'-'键降低亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(-20.f, Vector3f(0.f, 1.f, 0.f));
            myrender.draw_completed(image, mymodel);
            drawStatusOverlay(image, myrender);
            imshow("main", image);
            break;
        case 'd':  // 按下'-'键降低亮度
            myrender.clear(image);
            myrender.set_view(eye, centre, up);
            myrender.set_rotate(20.f, Vector3f(0.f, 1.f, 0.f));
            myrender.draw_completed(image, mymodel);
            drawStatusOverlay(image, myrender);
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
