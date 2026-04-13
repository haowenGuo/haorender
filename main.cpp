#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#endif

#include "tgaimage.h"
#include "opencv2/opencv.hpp"
#include <Eigen/Dense>
#include <vector>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <array>
#include <ctime>
#include <chrono>
#include <string>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include "drawer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "model.h"
#include "render.h"

using namespace cv;
using namespace std;
using namespace Eigen;

namespace {
const char* kRenderWindow = "main";
const char* kDashboardWindow = "dashboard";
const char* kSceneWindow = "scene_controls";
const char* kShadingWindow = "shading_controls";
const char* kLightWindow = "light_controls";
const int kNearShadowResolutionOptions[] = { 512, 1024, 1536, 2048, 3072 };
const int kFarShadowResolutionOptions[] = { 256, 512, 1024, 1536, 2048 };
const Vector3f kWorldUp(0.0f, 1.0f, 0.0f);

Vector3f eye = { 0.0f, 0.0f, 3.0f };
Vector3f centre = { 0.0f, 0.0f, 0.0f };
Vector3f up = { 0.0f, 1.0f, 0.0f };

struct OrbitCameraState {
    Vector3f target = Vector3f::Zero();
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 3.0f;
    bool rotating = false;
    bool panning = false;
    Point lastMouse = Point(0, 0);
};

struct LightControl {
    int yaw = 45;
    int pitch = 125;
    int intensity = 260;
    int r = 255;
    int g = 235;
    int b = 215;
};

struct UiState {
    int shadow_enabled = 1;
    int shadow_technique = 0;
    int backface_culling = 1;
    int shadow_cascade_enabled = 1;
    int near_shadow_res_preset = 2;
    int far_shadow_res_preset = 2;
    int shadow_near_extent = 140;
    int shadow_near_depth = 300;
    int shadow_far_extent = 400;
    int shadow_far_depth = 800;
    int cascade_split = 220;
    int cascade_blend = 60;
    int fov = 90;
    int exposure = 100;
    int normal_strength = 70;

    int shading_look = 0;
    int ibl_enabled = 1;
    int ibl_diffuse = 55;
    int ibl_specular = 80;
    int sky_light = 20;
    int metallic_channel = 2;
    int roughness_channel = 1;
    int ao_channel = 0;
    int emissive_channel = 0;

    int phong_hard_specular = 0;
    int phong_toon_diffuse = 0;
    int phong_use_tonemap = 0;
    int phong_primary_light_only = 1;
    int phong_secondary_scale = 12;
    int phong_ambient = 3;
    int phong_specular = 12;

    int active_lights = 3;
    array<LightControl, 3> lights;
};

struct ButtonInfo {
    string id;
    string label;
    Rect rect;
};

struct AppState {
    int render_width = 800;
    int render_height = 600;
    render renderer;
    Mat render_image;
    Model model;
    UiState ui;
    UiState applied_ui;
    bool ui_applied = false;
    string model_path;
    string environment_path;
    string status_message = "Ready";
    int status_frames = 0;
    float default_camera_distance = 3.0f;
    render::FrameProfile last_profile;
    vector<ButtonInfo> dashboard_buttons;
    bool request_load_model = false;
    bool request_load_environment = false;
    bool request_clear_environment = false;
    bool request_reset_camera = false;
    bool request_save_screenshot = false;

    AppState()
        : renderer(render_height, render_width),
          render_image(render_height, render_width, CV_8UC3, Scalar(155, 155, 155)) {
        ui.lights[0].yaw = 45;
        ui.lights[0].pitch = 125;
        ui.lights[0].intensity = 260;
        ui.lights[0].r = 255;
        ui.lights[0].g = 235;
        ui.lights[0].b = 215;

        ui.lights[1].yaw = 284;
        ui.lights[1].pitch = 110;
        ui.lights[1].intensity = 65;
        ui.lights[1].r = 176;
        ui.lights[1].g = 196;
        ui.lights[1].b = 255;

        ui.lights[2].yaw = 167;
        ui.lights[2].pitch = 120;
        ui.lights[2].intensity = 25;
        ui.lights[2].r = 255;
        ui.lights[2].g = 225;
        ui.lights[2].b = 204;

        applied_ui = ui;
    }
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

void enableFastFpModes() {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}

void setStatus(AppState& app, const string& message) {
    app.status_message = message;
    app.status_frames = 240;
    cout << "[UI] " << message << endl;
}

string shortenPath(const string& path, size_t maxLength = 52) {
    if (path.size() <= maxLength) {
        return path;
    }
    if (maxLength < 8) {
        return path.substr(0, maxLength);
    }
    return path.substr(0, maxLength / 2 - 2) + " ... " + path.substr(path.size() - maxLength / 2 + 1);
}

int presetValue(const int* values, size_t count, int index) {
    if (count == 0) {
        return 0;
    }
    index = std::clamp(index, 0, static_cast<int>(count) - 1);
    return values[index];
}

MaterialPbrChannelMap currentPbrMap(const UiState& ui) {
    MaterialPbrChannelMap map;
    map.metallic_channel = ui.metallic_channel;
    map.roughness_channel = ui.roughness_channel;
    map.ao_channel = ui.ao_channel;
    map.emissive_channel = ui.emissive_channel;
    return map;
}

void applyPbrChannelMap(Model& model, const MaterialPbrChannelMap& map, bool verbose) {
    for (auto& mesh : model.meshes) {
        mesh.pbr_channel_map = map;
    }
    if (verbose) {
        cout << "[PBR] Channel map: metallic=" << map.metallic_channel
             << " roughness=" << map.roughness_channel
             << " ao=" << map.ao_channel
             << " emissive=" << map.emissive_channel << endl;
    }
}

Vector4f buildIncomingLight(const LightControl& control) {
    float yaw = static_cast<float>(control.yaw) * static_cast<float>(MY_PI) / 180.0f;
    float elevation = static_cast<float>(control.pitch - 90) * static_cast<float>(MY_PI) / 180.0f;
    float cosElevation = std::cos(elevation);
    Vector3f sceneToLight(
        cosElevation * std::sin(yaw),
        std::sin(elevation),
        cosElevation * std::cos(yaw));
    if (sceneToLight.squaredNorm() <= 1e-8f) {
        sceneToLight = Vector3f(0.0f, 1.0f, 0.0f);
    }
    sceneToLight.normalize();
    Vector3f incoming = -sceneToLight;
    return Vector4f(incoming[0], incoming[1], incoming[2], 0.0f);
}

Vector3f buildLightColor(const LightControl& control) {
    float intensity = static_cast<float>(control.intensity) / 100.0f;
    Vector3f color(
        static_cast<float>(control.r) / 255.0f,
        static_cast<float>(control.g) / 255.0f,
        static_cast<float>(control.b) / 255.0f);
    return color * intensity;
}

bool pbrMapChanged(const UiState& a, const UiState& b) {
    return a.metallic_channel != b.metallic_channel ||
           a.roughness_channel != b.roughness_channel ||
           a.ao_channel != b.ao_channel ||
           a.emissive_channel != b.emissive_channel;
}

bool lightingChanged(const UiState& a, const UiState& b) {
    if (a.active_lights != b.active_lights) {
        return true;
    }
    for (size_t i = 0; i < a.lights.size(); ++i) {
        const LightControl& lhs = a.lights[i];
        const LightControl& rhs = b.lights[i];
        if (lhs.yaw != rhs.yaw || lhs.pitch != rhs.pitch || lhs.intensity != rhs.intensity ||
            lhs.r != rhs.r || lhs.g != rhs.g || lhs.b != rhs.b) {
            return true;
        }
    }
    return false;
}

bool shadowSettingsChanged(const UiState& a, const UiState& b) {
    return a.shadow_enabled != b.shadow_enabled ||
           a.shadow_technique != b.shadow_technique ||
           a.shadow_cascade_enabled != b.shadow_cascade_enabled ||
           a.near_shadow_res_preset != b.near_shadow_res_preset ||
           a.far_shadow_res_preset != b.far_shadow_res_preset ||
           a.shadow_near_extent != b.shadow_near_extent ||
           a.shadow_near_depth != b.shadow_near_depth ||
           a.shadow_far_extent != b.shadow_far_extent ||
           a.shadow_far_depth != b.shadow_far_depth ||
           a.cascade_split != b.cascade_split ||
           a.cascade_blend != b.cascade_blend;
}

bool sceneProjectionChanged(const UiState& a, const UiState& b) {
    return a.fov != b.fov;
}

bool createTimestampedScreenshot(AppState& app) {
    std::filesystem::create_directories("Screenshots");
    std::time_t now = std::time(nullptr);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &now);
#else
    tmNow = *std::localtime(&now);
#endif
    char fileName[128];
    std::strftime(fileName, sizeof(fileName), "haorender_%Y%m%d_%H%M%S.png", &tmNow);
    std::filesystem::path path = std::filesystem::path("Screenshots") / fileName;
    if (!imwrite(path.string(), app.render_image)) {
        setStatus(app, "Failed to save screenshot");
        return false;
    }
    setStatus(app, "Screenshot saved: " + path.string());
    return true;
}

#ifdef _WIN32
string openFileDialog(const char* title, const char* filter) {
    char fileBuffer[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = title;
    if (GetOpenFileNameA(&ofn)) {
        return string(fileBuffer);
    }
    return "";
}
#else
string openFileDialog(const char*, const char*) {
    return "";
}
#endif

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

string findDefaultModelPath() {
    const vector<string> candidates = {
        "../../Resources/MAIFU/IF.fbx",
        "../Resources/MAIFU/IF.fbx",
        "Resources/MAIFU/IF.fbx",
        "../../Resources/风影/风影.pmx",
        "../Resources/风影/风影.pmx",
        "Resources/风影/风影.pmx"
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return "../Resources/MAIFU/IF.fbx";
}

void configureModelTransform(AppState& app) {
    app.renderer.model = Matrix4f::Identity();
    float maxSize = max(abs(app.model.maxx - app.model.minx), abs(app.model.maxy - app.model.miny));
    maxSize = max(maxSize, abs(app.model.maxz - app.model.minz));
    if (maxSize <= 1e-6f) {
        maxSize = 1.0f;
    }
    Vector3f modelCenter(
        0.5f * (app.model.maxx + app.model.minx),
        0.5f * (app.model.maxy + app.model.miny),
        0.5f * (app.model.maxz + app.model.minz));
    app.renderer.set_translation(-modelCenter[0], -modelCenter[1], -modelCenter[2]);
    app.renderer.set_scal(2.0f / maxSize, 2.0f / maxSize, 2.0f / maxSize);
    app.default_camera_distance = 3.0f;
    resetCamera(Vector3f::Zero(), app.default_camera_distance);
}

bool loadModelFromPath(AppState& app, const string& path) {
    Model loadedModel;
    if (loadedModel.modelread(path) != 1) {
        setStatus(app, "Failed to load model");
        return false;
    }
    applyPbrChannelMap(loadedModel, currentPbrMap(app.ui), false);
    app.model = std::move(loadedModel);
    app.model_path = path;
    app.renderer.setComplexShader(app.model);
    app.renderer.set_projection(static_cast<float>(std::clamp(app.ui.fov, 30, 120)), static_cast<float>(app.render_width) / static_cast<float>(app.render_height), 0.1f, 100.0f);
    app.renderer.set_viewport(0.0f, 0.0f, static_cast<float>(app.render_width), static_cast<float>(app.render_height));
    configureModelTransform(app);
    if (!app.environment_path.empty()) {
        app.renderer.setEnvironmentMap(app.environment_path.c_str());
    }
    else {
        app.renderer.setEnvironmentMap(nullptr);
    }
    app.renderer.shadow_cache_valid = 0;
    app.renderer.shadow_cache_dirty = 1;
    app.renderer.ray_scene_valid = 0;
    app.renderer.ray_scene_dirty = 1;
    app.ui_applied = false;
    setStatus(app, "Loaded model: " + shortenPath(path, 64));
    return true;
}

bool loadEnvironmentFromPath(AppState& app, const string& path) {
    if (app.renderer.setEnvironmentMap(path.c_str()) != 1) {
        setStatus(app, "Failed to load environment");
        return false;
    }
    app.environment_path = path;
    setStatus(app, "Loaded environment: " + shortenPath(path, 64));
    return true;
}

void applyUiToRenderer(AppState& app) {
    const bool firstApply = !app.ui_applied;
    const UiState& ui = app.ui;
    const UiState& previous = app.applied_ui;

    app.renderer.setShadingLook(ui.shading_look == 1 ? ShadingLook::StylizedPhong : ShadingLook::RealisticPbr);
    app.renderer.setShadowTechnique(ui.shadow_technique == 1 ? ShadowTechnique::RasterEmbree : ShadowTechnique::ShadowMap);
    app.renderer.shadow_on = ui.shadow_enabled;
    app.renderer.backcut = ui.backface_culling;
    app.renderer.shadow_cascade_enabled = ui.shadow_cascade_enabled;
    app.renderer.shadow_near_width = presetValue(kNearShadowResolutionOptions, std::size(kNearShadowResolutionOptions), ui.near_shadow_res_preset);
    app.renderer.shadow_near_height = app.renderer.shadow_near_width;
    app.renderer.shadow_far_width = presetValue(kFarShadowResolutionOptions, std::size(kFarShadowResolutionOptions), ui.far_shadow_res_preset);
    app.renderer.shadow_far_height = app.renderer.shadow_far_width;
    app.renderer.shadow_near_extent = std::max(0.5f, static_cast<float>(ui.shadow_near_extent) / 100.0f);
    app.renderer.shadow_near_depth = std::max(0.5f, static_cast<float>(ui.shadow_near_depth) / 100.0f);
    app.renderer.shadow_far_extent = std::max(0.5f, static_cast<float>(ui.shadow_far_extent) / 100.0f);
    app.renderer.shadow_far_depth = std::max(0.5f, static_cast<float>(ui.shadow_far_depth) / 100.0f);
    app.renderer.shadow_cascade_split = std::max(0.1f, static_cast<float>(ui.cascade_split) / 100.0f);
    app.renderer.shadow_cascade_blend = std::max(0.0f, static_cast<float>(ui.cascade_blend) / 100.0f);
    app.renderer.exposure = std::max(0.0f, static_cast<float>(ui.exposure) / 100.0f);
    app.renderer.normal_strength = std::max(0.0f, static_cast<float>(ui.normal_strength) / 100.0f);
    app.renderer.ibl_enabled = ui.ibl_enabled;
    app.renderer.ibl_diffuse_strength = std::max(0.0f, static_cast<float>(ui.ibl_diffuse) / 100.0f);
    app.renderer.ibl_specular_strength = std::max(0.0f, static_cast<float>(ui.ibl_specular) / 100.0f);
    app.renderer.sky_light_strength = std::max(0.0f, static_cast<float>(ui.sky_light) / 100.0f);
    app.renderer.setPhongHardSpecular(ui.phong_hard_specular != 0);
    app.renderer.setPhongToonDiffuse(ui.phong_toon_diffuse != 0);
    app.renderer.phong_use_tonemap = ui.phong_use_tonemap;
    app.renderer.phong_primary_light_only = ui.phong_primary_light_only;
    app.renderer.phong_secondary_light_scale = std::max(0.0f, static_cast<float>(ui.phong_secondary_scale) / 100.0f);
    app.renderer.phong_ambient_strength = std::max(0.0f, static_cast<float>(ui.phong_ambient) / 100.0f);
    app.renderer.phong_specular_strength = std::max(0.0f, static_cast<float>(ui.phong_specular) / 100.0f);

    if (firstApply || sceneProjectionChanged(ui, previous)) {
        app.renderer.set_projection(static_cast<float>(std::clamp(ui.fov, 30, 120)), static_cast<float>(app.render_width) / static_cast<float>(app.render_height), 0.1f, 100.0f);
    }

    if (firstApply || shadowSettingsChanged(ui, previous)) {
        app.renderer.shadow_cache_valid = 0;
        app.renderer.shadow_cache_dirty = 1;
    }

    if (firstApply || pbrMapChanged(ui, previous)) {
        applyPbrChannelMap(app.model, currentPbrMap(ui), false);
    }

    if (firstApply || lightingChanged(ui, previous)) {
        vector<Vector4f> lightDirs;
        vector<Vector3f> lightColors;
        for (int i = 0; i < std::clamp(ui.active_lights, 0, 3); ++i) {
            lightDirs.push_back(buildIncomingLight(ui.lights[i]));
            lightColors.push_back(buildLightColor(ui.lights[i]));
        }
        app.renderer.setLights(lightDirs, lightColors);
    }

    app.applied_ui = ui;
    app.ui_applied = true;
}

void drawStatusOverlay(Mat& image, const AppState& app) {
    std::ostringstream oss;
    oss << "Look: " << app.renderer.shadingLookName()
        << " | Lights: " << std::clamp(app.ui.active_lights, 0, 3)
        << " | Shadow: " << (app.ui.shadow_enabled ? app.renderer.shadowTechniqueName() : "OFF")
        << " | Backface: " << (app.ui.backface_culling ? "ON" : "OFF");
    if (app.renderer.getShadowTechnique() == ShadowTechnique::RasterEmbree && !app.renderer.embreeAvailable()) {
        oss << " | Embree Fallback";
    }

    const string text = oss.str();
    const int fontFace = FONT_HERSHEY_SIMPLEX;
    const double fontScale = 0.52;
    const int thickness = 1;
    int baseline = 0;
    Size textSize = getTextSize(text, fontFace, fontScale, thickness, &baseline);
    Rect panel(8, 8, textSize.width + 16, textSize.height + 16);
    rectangle(image, panel, Scalar(20, 20, 20), FILLED);
    rectangle(image, panel, Scalar(210, 210, 210), 1);
    putText(image, text, Point(panel.x + 8, panel.y + panel.height - 8), fontFace, fontScale, Scalar(240, 240, 240), thickness, LINE_AA);
}

void addDashboardButton(Mat& panel, AppState& app, const string& id, const string& label, int x, int y, int w, int h) {
    Rect rect(x, y, w, h);
    rectangle(panel, rect, Scalar(52, 58, 64), FILLED);
    rectangle(panel, rect, Scalar(200, 200, 200), 1);
    putText(panel, label, Point(x + 12, y + 28), FONT_HERSHEY_SIMPLEX, 0.55, Scalar(245, 245, 245), 1, LINE_AA);
    app.dashboard_buttons.push_back({ id, label, rect });
}

void drawTextLines(Mat& canvas, const vector<string>& lines, int startX, int startY, const Scalar& color) {
    int y = startY;
    for (const auto& line : lines) {
        putText(canvas, line, Point(startX, y), FONT_HERSHEY_SIMPLEX, 0.47, color, 1, LINE_AA);
        y += 22;
    }
}

void drawDashboard(AppState& app) {
    Mat panel(720, 520, CV_8UC3, Scalar(28, 30, 34));
    app.dashboard_buttons.clear();

    putText(panel, "HaoRender Desktop", Point(16, 32), FONT_HERSHEY_SIMPLEX, 0.82, Scalar(250, 250, 250), 2, LINE_AA);
    putText(panel, "Scene control and debugging dashboard", Point(16, 58), FONT_HERSHEY_SIMPLEX, 0.45, Scalar(185, 185, 185), 1, LINE_AA);

    addDashboardButton(panel, app, "load_model", "Load Model", 16, 84, 150, 40);
    addDashboardButton(panel, app, "load_env", "Load Env", 182, 84, 150, 40);
    addDashboardButton(panel, app, "clear_env", "Clear Env", 348, 84, 150, 40);
    addDashboardButton(panel, app, "reset_camera", "Reset Camera", 16, 136, 150, 40);
    addDashboardButton(panel, app, "save_shot", "Save Screenshot", 182, 136, 180, 40);

    rectangle(panel, Rect(16, 196, 488, 118), Scalar(40, 44, 50), FILLED);
    rectangle(panel, Rect(16, 196, 488, 118), Scalar(110, 110, 110), 1);
    vector<string> sceneLines = {
        "Model: " + (app.model_path.empty() ? string("<none>") : shortenPath(app.model_path)),
        "Environment: " + (app.environment_path.empty() ? string("<sky fallback>") : shortenPath(app.environment_path)),
        "Look: " + string(app.renderer.shadingLookName()),
        "Shadow: " + string(app.ui.shadow_enabled ? app.renderer.shadowTechniqueName() : "OFF"),
        "Backface Culling: " + string(app.ui.backface_culling ? "ON" : "OFF")
    };
    drawTextLines(panel, sceneLines, 28, 224, Scalar(235, 235, 235));

    rectangle(panel, Rect(16, 332, 488, 178), Scalar(40, 44, 50), FILLED);
    rectangle(panel, Rect(16, 332, 488, 178), Scalar(110, 110, 110), 1);
    vector<string> profilerLines = {
        "Profiler",
        "clear: " + to_string(app.last_profile.clear_ms) + " ms",
        "shadow near: " + to_string(app.last_profile.shadow_near_ms) + " ms",
        "shadow far: " + to_string(app.last_profile.shadow_far_ms) + " ms",
        "vertex: " + to_string(app.last_profile.vertex_ms) + " ms",
        "clip + bin: " + to_string(app.last_profile.clip_bin_ms) + " ms",
        "raster + shade: " + to_string(app.last_profile.raster_shade_ms) + " ms",
        "render total: " + to_string(app.last_profile.render_total_ms) + " ms"
    };
    drawTextLines(panel, profilerLines, 28, 360, Scalar(235, 235, 235));

    rectangle(panel, Rect(16, 530, 488, 100), Scalar(40, 44, 50), FILLED);
    rectangle(panel, Rect(16, 530, 488, 100), Scalar(110, 110, 110), 1);
    vector<string> helpLines = {
        "Mouse Left: orbit camera",
        "Mouse Right: pan camera",
        "Mouse Wheel: zoom",
        "Hotkeys: O load model, E load env, C clear env, R reset camera, P save screenshot, ESC exit"
    };
    drawTextLines(panel, helpLines, 28, 558, Scalar(225, 225, 225));

    if (app.status_frames > 0) {
        rectangle(panel, Rect(16, 646, 488, 42), Scalar(64, 76, 92), FILLED);
        rectangle(panel, Rect(16, 646, 488, 42), Scalar(175, 195, 215), 1);
        putText(panel, app.status_message, Point(28, 674), FONT_HERSHEY_SIMPLEX, 0.52, Scalar(245, 245, 245), 1, LINE_AA);
    }

    imshow(kDashboardWindow, panel);
}

void drawSceneSummary(const AppState& app) {
    Mat panel(160, 430, CV_8UC3, Scalar(33, 36, 40));
    vector<string> lines = {
        "Scene / Shadow",
        "FOV: " + to_string(std::clamp(app.ui.fov, 30, 120)),
        "Exposure: " + to_string(app.ui.exposure / 100.0f),
        "Normal Strength: " + to_string(app.ui.normal_strength / 100.0f),
        "Shadow Near Res: " + to_string(presetValue(kNearShadowResolutionOptions, std::size(kNearShadowResolutionOptions), app.ui.near_shadow_res_preset)),
        "Shadow Far Res: " + to_string(presetValue(kFarShadowResolutionOptions, std::size(kFarShadowResolutionOptions), app.ui.far_shadow_res_preset))
    };
    drawTextLines(panel, lines, 14, 28, Scalar(235, 235, 235));
    imshow(kSceneWindow, panel);
}

void drawShadingSummary(const AppState& app) {
    Mat panel(180, 430, CV_8UC3, Scalar(33, 36, 40));
    vector<string> lines = {
        "Shading",
        "Look: " + string(app.renderer.shadingLookName()),
        "IBL: " + string(app.ui.ibl_enabled ? "ON" : "OFF"),
        "PBR map channels M/R/A/E: " +
            to_string(app.ui.metallic_channel) + "/" +
            to_string(app.ui.roughness_channel) + "/" +
            to_string(app.ui.ao_channel) + "/" +
            to_string(app.ui.emissive_channel),
        "Phong hard spec: " + string(app.ui.phong_hard_specular ? "ON" : "OFF"),
        "Phong toon diffuse: " + string(app.ui.phong_toon_diffuse ? "ON" : "OFF"),
        "Phong tonemap: " + string(app.ui.phong_use_tonemap ? "ON" : "OFF")
    };
    drawTextLines(panel, lines, 14, 28, Scalar(235, 235, 235));
    imshow(kShadingWindow, panel);
}

void drawLightSummary(const AppState& app) {
    Mat panel(220, 430, CV_8UC3, Scalar(33, 36, 40));
    vector<string> lines = {
        "Lights",
        "Active lights: " + to_string(std::clamp(app.ui.active_lights, 0, 3))
    };
    for (int i = 0; i < 3; ++i) {
        std::ostringstream oss;
        oss << "L" << i << " yaw=" << app.ui.lights[i].yaw
            << " pitch=" << (app.ui.lights[i].pitch - 90)
            << " int=" << app.ui.lights[i].intensity / 100.0f;
        lines.push_back(oss.str());
    }
    drawTextLines(panel, lines, 14, 28, Scalar(235, 235, 235));
    imshow(kLightWindow, panel);
}

void createTrackbars(AppState& app) {
    namedWindow(kSceneWindow, WINDOW_NORMAL);
    namedWindow(kShadingWindow, WINDOW_NORMAL);
    namedWindow(kLightWindow, WINDOW_NORMAL);
    resizeWindow(kSceneWindow, 430, 720);
    resizeWindow(kShadingWindow, 430, 760);
    resizeWindow(kLightWindow, 430, 980);

    createTrackbar("Shadow Enabled", kSceneWindow, &app.ui.shadow_enabled, 1);
    createTrackbar("Shadow Technique", kSceneWindow, &app.ui.shadow_technique, 1);
    createTrackbar("Backface Culling", kSceneWindow, &app.ui.backface_culling, 1);
    createTrackbar("Shadow Cascades", kSceneWindow, &app.ui.shadow_cascade_enabled, 1);
    createTrackbar("FOV", kSceneWindow, &app.ui.fov, 120);
    createTrackbar("Exposure x100", kSceneWindow, &app.ui.exposure, 300);
    createTrackbar("Normal Strength x100", kSceneWindow, &app.ui.normal_strength, 200);
    createTrackbar("Near Shadow Res", kSceneWindow, &app.ui.near_shadow_res_preset, 4);
    createTrackbar("Far Shadow Res", kSceneWindow, &app.ui.far_shadow_res_preset, 4);
    createTrackbar("Near Extent x100", kSceneWindow, &app.ui.shadow_near_extent, 500);
    createTrackbar("Near Depth x100", kSceneWindow, &app.ui.shadow_near_depth, 1000);
    createTrackbar("Far Extent x100", kSceneWindow, &app.ui.shadow_far_extent, 1000);
    createTrackbar("Far Depth x100", kSceneWindow, &app.ui.shadow_far_depth, 2000);
    createTrackbar("Cascade Split x100", kSceneWindow, &app.ui.cascade_split, 500);
    createTrackbar("Cascade Blend x100", kSceneWindow, &app.ui.cascade_blend, 200);

    createTrackbar("Shading Look", kShadingWindow, &app.ui.shading_look, 1);
    createTrackbar("IBL Enabled", kShadingWindow, &app.ui.ibl_enabled, 1);
    createTrackbar("IBL Diffuse x100", kShadingWindow, &app.ui.ibl_diffuse, 200);
    createTrackbar("IBL Specular x100", kShadingWindow, &app.ui.ibl_specular, 200);
    createTrackbar("Sky Light x100", kShadingWindow, &app.ui.sky_light, 200);
    createTrackbar("Metallic Channel", kShadingWindow, &app.ui.metallic_channel, 3);
    createTrackbar("Roughness Channel", kShadingWindow, &app.ui.roughness_channel, 3);
    createTrackbar("AO Channel", kShadingWindow, &app.ui.ao_channel, 3);
    createTrackbar("Emissive Channel", kShadingWindow, &app.ui.emissive_channel, 3);
    createTrackbar("Phong Hard Spec", kShadingWindow, &app.ui.phong_hard_specular, 1);
    createTrackbar("Phong Toon Diff", kShadingWindow, &app.ui.phong_toon_diffuse, 1);
    createTrackbar("Phong Tonemap", kShadingWindow, &app.ui.phong_use_tonemap, 1);
    createTrackbar("Phong Primary Only", kShadingWindow, &app.ui.phong_primary_light_only, 1);
    createTrackbar("Phong Secondary x100", kShadingWindow, &app.ui.phong_secondary_scale, 100);
    createTrackbar("Phong Ambient x100", kShadingWindow, &app.ui.phong_ambient, 100);
    createTrackbar("Phong Specular x100", kShadingWindow, &app.ui.phong_specular, 200);

    createTrackbar("Active Lights", kLightWindow, &app.ui.active_lights, 3);
    createTrackbar("L0 Yaw", kLightWindow, &app.ui.lights[0].yaw, 359);
    createTrackbar("L0 Pitch (+90)", kLightWindow, &app.ui.lights[0].pitch, 180);
    createTrackbar("L0 Intensity x100", kLightWindow, &app.ui.lights[0].intensity, 400);
    createTrackbar("L0 Color R", kLightWindow, &app.ui.lights[0].r, 255);
    createTrackbar("L0 Color G", kLightWindow, &app.ui.lights[0].g, 255);
    createTrackbar("L0 Color B", kLightWindow, &app.ui.lights[0].b, 255);
    createTrackbar("L1 Yaw", kLightWindow, &app.ui.lights[1].yaw, 359);
    createTrackbar("L1 Pitch (+90)", kLightWindow, &app.ui.lights[1].pitch, 180);
    createTrackbar("L1 Intensity x100", kLightWindow, &app.ui.lights[1].intensity, 400);
    createTrackbar("L1 Color R", kLightWindow, &app.ui.lights[1].r, 255);
    createTrackbar("L1 Color G", kLightWindow, &app.ui.lights[1].g, 255);
    createTrackbar("L1 Color B", kLightWindow, &app.ui.lights[1].b, 255);
    createTrackbar("L2 Yaw", kLightWindow, &app.ui.lights[2].yaw, 359);
    createTrackbar("L2 Pitch (+90)", kLightWindow, &app.ui.lights[2].pitch, 180);
    createTrackbar("L2 Intensity x100", kLightWindow, &app.ui.lights[2].intensity, 400);
    createTrackbar("L2 Color R", kLightWindow, &app.ui.lights[2].r, 255);
    createTrackbar("L2 Color G", kLightWindow, &app.ui.lights[2].g, 255);
    createTrackbar("L2 Color B", kLightWindow, &app.ui.lights[2].b, 255);
}

void processButtonAction(AppState& app, const string& action) {
    if (action == "load_model") {
        app.request_load_model = true;
    }
    else if (action == "load_env") {
        app.request_load_environment = true;
    }
    else if (action == "clear_env") {
        app.request_clear_environment = true;
    }
    else if (action == "reset_camera") {
        app.request_reset_camera = true;
    }
    else if (action == "save_shot") {
        app.request_save_screenshot = true;
    }
}

void processPendingActions(AppState& app) {
    static const char kModelFilter[] =
        "Model Files\0*.obj;*.fbx;*.gltf;*.glb;*.pmx;*.dae;*.3ds;*.stl;*.ply;*.x\0"
        "All Files\0*.*\0";
    static const char kEnvironmentFilter[] =
        "Environment Files\0*.hdr;*.png;*.jpg;*.jpeg;*.bmp;*.tga\0"
        "All Files\0*.*\0";

    if (app.request_load_model) {
        app.request_load_model = false;
        string path = openFileDialog("Load Model", kModelFilter);
        if (!path.empty()) {
            loadModelFromPath(app, path);
        }
    }
    if (app.request_load_environment) {
        app.request_load_environment = false;
        string path = openFileDialog("Load Environment", kEnvironmentFilter);
        if (!path.empty()) {
            loadEnvironmentFromPath(app, path);
        }
    }
    if (app.request_clear_environment) {
        app.request_clear_environment = false;
        app.environment_path.clear();
        app.renderer.setEnvironmentMap(nullptr);
        setStatus(app, "Environment cleared; using sky fallback");
    }
    if (app.request_reset_camera) {
        app.request_reset_camera = false;
        resetCamera(Vector3f::Zero(), app.default_camera_distance);
        setStatus(app, "Camera reset");
    }
    if (app.request_save_screenshot) {
        app.request_save_screenshot = false;
        createTimestampedScreenshot(app);
    }
}

void renderMouseCallback(int event, int x, int y, int flags, void*) {
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
            cameraState.target += (-static_cast<float>(delta.x) * right + static_cast<float>(delta.y) * cameraUp) * panScale;
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

void dashboardMouseCallback(int event, int x, int y, int, void* userdata) {
    if (event != EVENT_LBUTTONDOWN || userdata == nullptr) {
        return;
    }
    AppState* app = static_cast<AppState*>(userdata);
    Point point(x, y);
    for (const auto& button : app->dashboard_buttons) {
        if (button.rect.contains(point)) {
            processButtonAction(*app, button.id);
            return;
        }
    }
}

void updateControlWindows(AppState& app) {
    drawDashboard(app);
    drawSceneSummary(app);
    drawShadingSummary(app);
    drawLightSummary(app);
}

}

int main(int argc, char** argv) {
    enableFastFpModes();

    AppState app;
    string startupModelPath = findDefaultModelPath();
    string startupEnvironmentPath = findDefaultEnvironmentMap();

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        static const string shadowTechniquePrefix = "--shadow-technique=";
        if (arg.rfind(shadowTechniquePrefix, 0) == 0) {
            string mode = arg.substr(shadowTechniquePrefix.size());
            app.ui.shadow_technique = (mode == "embree" || mode == "raster-embree") ? 1 : 0;
        }
        else if (!arg.empty() && arg[0] != '-') {
            startupModelPath = arg;
        }
    }

    namedWindow(kRenderWindow, WINDOW_NORMAL);
    namedWindow(kDashboardWindow, WINDOW_NORMAL);
    resizeWindow(kRenderWindow, app.render_width, app.render_height);
    resizeWindow(kDashboardWindow, 540, 760);
    setMouseCallback(kRenderWindow, renderMouseCallback, nullptr);
    setMouseCallback(kDashboardWindow, dashboardMouseCallback, &app);
    createTrackbars(app);

    if (!loadModelFromPath(app, startupModelPath)) {
        cerr << "模型加载失败: " << startupModelPath << endl;
        return -1;
    }
    if (!startupEnvironmentPath.empty()) {
        loadEnvironmentFromPath(app, startupEnvironmentPath);
    }
    else {
        setStatus(app, "No environment found; using sky fallback");
    }

    while (true) {
        processPendingActions(app);
        applyUiToRenderer(app);

        auto clearStart = std::chrono::high_resolution_clock::now();
        app.renderer.clear(app.render_image);
        auto clearEnd = std::chrono::high_resolution_clock::now();

        app.renderer.set_view(eye, centre, up);
        app.renderer.draw_completed(app.render_image, app.model);
        app.last_profile = app.renderer.getLastProfile();
        app.last_profile.clear_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(clearEnd - clearStart).count();

        drawStatusOverlay(app.render_image, app);
        imshow(kRenderWindow, app.render_image);
        updateControlWindows(app);

        if (app.status_frames > 0) {
            --app.status_frames;
        }

        int key = waitKey(16) & 0xFF;
        if (key == 27) {
            break;
        }
        if (key == 'o' || key == 'O') {
            app.request_load_model = true;
        }
        else if (key == 'e' || key == 'E') {
            app.request_load_environment = true;
        }
        else if (key == 'c' || key == 'C') {
            app.request_clear_environment = true;
        }
        else if (key == 'r' || key == 'R') {
            app.request_reset_camera = true;
        }
        else if (key == 'p' || key == 'P') {
            app.request_save_screenshot = true;
        }
    }

    return 0;
}
