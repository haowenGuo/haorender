#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "opencv2/opencv.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <xmmintrin.h>
#include <pmmintrin.h>

#include "model.h"
#include "programmable_shader.h"
#include "render.h"

using namespace cv;
using namespace std;
using namespace Eigen;

namespace {
const int kNearShadowResolutionOptions[] = { 512, 1024, 1536, 2048, 3072 };
const int kFarShadowResolutionOptions[] = { 256, 512, 1024, 1536, 2048 };
const Vector3f kWorldUp(0.0f, 1.0f, 0.0f);
struct ResolutionPreset {
    int width;
    int height;
};
const ResolutionPreset kRenderResolutionPresets[] = {
    { 640, 360 },
    { 800, 450 },
    { 960, 540 },
    { 1280, 720 },
    { 1600, 900 },
    { 1920, 1080 },
    { 2560, 1440 },
    { 2560, 1600 },
    { 3440, 1440 },
    { 3200, 1800 },
    { 3840, 1600 },
    { 3840, 2160 },
    { 5120, 1440 },
    { 5120, 2880 },
    { 7680, 4320 }
};

struct ShaderTemplatePreset {
    const char* name_en;
    const char* name_zh;
    const char* source;
};

const ShaderTemplatePreset kProgrammableShaderTemplates[] = {
    {
        "Default Lit",
        "默认光照",
        R"(lambert = (0.12 + ndotl * shadow) * ao;
highlight = pow(max(ndotl, 0.0), 8.0) * specular * 0.35;
rim_light = pow(rim, 2.5) * 0.18;
r = base_r * lambert * light_r + emissive_r + highlight + rim_light;
g = base_g * lambert * light_g + emissive_g + highlight + rim_light;
b = base_b * lambert * light_b + emissive_b + highlight + rim_light;
)"
    },
    {
        "Toon Bands",
        "卡通分段",
        R"(band = smoothstep(0.32, 0.68, ndotl * shadow);
diffuse = mix(0.24, 1.0, band) * ao;
rim_light = smoothstep(0.72, 0.92, rim) * 0.18;
spec_band = step(0.92, ndotl) * specular * 0.12;
r = base_r * diffuse * light_r + rim_light + spec_band;
g = base_g * diffuse * light_g + rim_light + spec_band;
b = base_b * diffuse * light_b + rim_light + spec_band;
)"
    },
    {
        "Rim + Shadow Debug",
        "描边与阴影调试",
        R"(shadow_mix = shadow * 0.8 + 0.2;
rim_light = pow(rim, 3.0);
r = base_r * shadow_mix + rim_light * 0.15;
g = base_g * shadow_mix + rim_light * 0.45;
b = base_b * shadow_mix + rim_light;
)"
    },
    {
        "Normal Debug",
        "法线调试",
        R"(r = n_x * 0.5 + 0.5;
g = n_y * 0.5 + 0.5;
b = n_z * 0.5 + 0.5;
)"
    },
    {
        "Shadow Debug",
        "阴影调试",
        R"(r = shadow;
g = shadow;
b = shadow;
)"
    }
};

enum class UiLanguage {
    English = 0,
    Chinese = 1
};

enum class UiTheme {
    Graphite = 0,
    Ocean = 1,
    Pearl = 2
};

struct ThemePalette {
    QString window;
    QString panel;
    QString surface;
    QString elevated;
    QString border;
    QString accent;
    QString text;
    QString muted;
    QString viewport;
    QString button;
    QString buttonHover;
};

ThemePalette themePalette(UiTheme theme) {
    switch (theme) {
    case UiTheme::Ocean:
        return { "#f4f8fb", "#dfeaf1", "#edf4f8", "#ffffff", "#b8ccd8", "#2e8ea7", "#15323b", "#58727d", "#dbe6ec", "#f7fbfd", "#e7f2f7" };
    case UiTheme::Pearl:
        return { "#f6f0ea", "#efe4d8", "#fbf7f3", "#ffffff", "#d6c3af", "#a9774b", "#3a2b1f", "#7a6a5a", "#efe6de", "#fffdfa", "#f5ede6" };
    case UiTheme::Graphite:
    default:
        return { "#16181d", "#1b1f26", "#1c2027", "#232a33", "#323948", "#58becc", "#edf1f5", "#9da7b3", "#101318", "#252b35", "#2c3440" };
    }
}

class CollapsibleGroupBox : public QGroupBox {
public:
    explicit CollapsibleGroupBox(const QString& title, QWidget* parent = nullptr)
        : QGroupBox(title, parent) {
        setCheckable(true);
        setChecked(true);
        connect(this, &QGroupBox::toggled, this, [this](bool expanded) { updateCollapsedState(expanded); });
    }

    void updateCollapsedState(bool expanded) {
        if (layout() == nullptr) {
            return;
        }
        for (int i = 0; i < layout()->count(); ++i) {
            if (QLayoutItem* item = layout()->itemAt(i)) {
                if (QWidget* widget = item->widget()) {
                    widget->setVisible(expanded);
                }
                else if (QLayout* childLayout = item->layout()) {
                    for (int j = 0; j < childLayout->count(); ++j) {
                        if (QWidget* childWidget = childLayout->itemAt(j)->widget()) {
                            childWidget->setVisible(expanded);
                        }
                    }
                }
            }
        }
    }
};

class IntSliderField : public QWidget {
public:
    IntSliderField(int minValue, int maxValue, int step = 1, QWidget* parent = nullptr)
        : QWidget(parent), min_(minValue), max_(maxValue), step_(std::max(step, 1)) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setRange(min_, max_);
        slider_->setSingleStep(step_);
        slider_->setPageStep(step_ * 4);
        slider_->setStyleSheet("QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: #94a3b833; }"
                               "QSlider::handle:horizontal { width: 16px; margin: -5px 0; border-radius: 8px; background: #58becc; }");
        value_label_ = new QLabel(this);
        value_label_->setMinimumWidth(52);
        value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(slider_, 1);
        layout->addWidget(value_label_);
        connect(slider_, &QSlider::valueChanged, this, [this](int value) { value_label_->setText(QString::number(value)); });
        setValue(min_);
    }

    int value() const { return slider_->value(); }
    void setValue(int value) { slider_->setValue(std::clamp(value, min_, max_)); }
    QSlider* slider() const { return slider_; }

private:
    int min_;
    int max_;
    int step_;
    QSlider* slider_ = nullptr;
    QLabel* value_label_ = nullptr;
};

class DoubleSliderField : public QWidget {
public:
    DoubleSliderField(double minValue, double maxValue, double stepValue, int decimals = 2, QWidget* parent = nullptr)
        : QWidget(parent), min_(minValue), max_(maxValue), step_(std::max(stepValue, 0.0001)), decimals_(decimals) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        slider_ = new QSlider(Qt::Horizontal, this);
        const int steps = std::max(1, static_cast<int>(std::round((max_ - min_) / step_)));
        slider_->setRange(0, steps);
        slider_->setSingleStep(1);
        slider_->setPageStep(std::max(1, steps / 20));
        slider_->setStyleSheet("QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: #94a3b833; }"
                               "QSlider::handle:horizontal { width: 16px; margin: -5px 0; border-radius: 8px; background: #58becc; }");
        value_label_ = new QLabel(this);
        value_label_->setMinimumWidth(60);
        value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(slider_, 1);
        layout->addWidget(value_label_);
        connect(slider_, &QSlider::valueChanged, this, [this](int) { value_label_->setText(QString::number(value(), 'f', decimals_)); });
        setValue(min_);
    }

    double value() const {
        return min_ + static_cast<double>(slider_->value()) * step_;
    }
    void setValue(double value) {
        const double clamped = std::max(min_, std::min(max_, value));
        slider_->setValue(static_cast<int>(std::round((clamped - min_) / step_)));
        value_label_->setText(QString::number(clamped, 'f', decimals_));
    }
    QSlider* slider() const { return slider_; }

private:
    double min_;
    double max_;
    double step_;
    int decimals_;
    QSlider* slider_ = nullptr;
    QLabel* value_label_ = nullptr;
};

struct OrbitCameraState {
    Vector3f target = Vector3f::Zero();
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 3.0f;
    bool rotating = false;
    bool panning = false;
    QPoint lastMouse;
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

UiState makeDefaultUiState() {
    UiState ui;
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
    return ui;
}

bool sanitizeUiState(UiState& ui) {
    const UiState defaults = makeDefaultUiState();
    const bool noEffectiveLights = std::all_of(ui.lights.begin(), ui.lights.end(), [](const LightControl& light) {
        return light.intensity <= 0 || (light.r <= 4 && light.g <= 4 && light.b <= 4);
    });
    const bool suspiciousZeroLighting =
        ui.exposure <= 0 &&
        ui.normal_strength <= 0 &&
        ui.ibl_enabled == 0 &&
        ui.ibl_diffuse <= 0 &&
        ui.ibl_specular <= 0 &&
        ui.sky_light <= 0 &&
        ui.phong_ambient <= 0 &&
        ui.phong_specular <= 0 &&
        noEffectiveLights;

    if (suspiciousZeroLighting) {
        ui = defaults;
        return true;
    }

    ui.shadow_enabled = std::clamp(ui.shadow_enabled, 0, 1);
    ui.shadow_technique = std::clamp(ui.shadow_technique, 0, 1);
    ui.backface_culling = std::clamp(ui.backface_culling, 0, 1);
    ui.shadow_cascade_enabled = std::clamp(ui.shadow_cascade_enabled, 0, 1);
    ui.near_shadow_res_preset = std::clamp(ui.near_shadow_res_preset, 0, static_cast<int>(std::size(kNearShadowResolutionOptions)) - 1);
    ui.far_shadow_res_preset = std::clamp(ui.far_shadow_res_preset, 0, static_cast<int>(std::size(kFarShadowResolutionOptions)) - 1);
    ui.shadow_near_extent = std::clamp(ui.shadow_near_extent, 50, 500);
    ui.shadow_near_depth = std::clamp(ui.shadow_near_depth, 50, 1200);
    ui.shadow_far_extent = std::clamp(ui.shadow_far_extent, 50, 1000);
    ui.shadow_far_depth = std::clamp(ui.shadow_far_depth, 50, 2000);
    ui.cascade_split = std::clamp(ui.cascade_split, 10, 500);
    ui.cascade_blend = std::clamp(ui.cascade_blend, 0, 200);
    ui.fov = std::clamp(ui.fov, 30, 120);
    ui.exposure = std::clamp(ui.exposure, 0, 300);
    ui.normal_strength = std::clamp(ui.normal_strength, 0, 200);
    ui.shading_look = std::clamp(ui.shading_look, 0, 2);
    ui.ibl_enabled = std::clamp(ui.ibl_enabled, 0, 1);
    ui.ibl_diffuse = std::clamp(ui.ibl_diffuse, 0, 200);
    ui.ibl_specular = std::clamp(ui.ibl_specular, 0, 200);
    ui.sky_light = std::clamp(ui.sky_light, 0, 200);
    ui.metallic_channel = std::clamp(ui.metallic_channel, 0, 3);
    ui.roughness_channel = std::clamp(ui.roughness_channel, 0, 3);
    ui.ao_channel = std::clamp(ui.ao_channel, 0, 3);
    ui.emissive_channel = std::clamp(ui.emissive_channel, 0, 3);
    ui.phong_hard_specular = std::clamp(ui.phong_hard_specular, 0, 1);
    ui.phong_toon_diffuse = std::clamp(ui.phong_toon_diffuse, 0, 1);
    ui.phong_use_tonemap = std::clamp(ui.phong_use_tonemap, 0, 1);
    ui.phong_primary_light_only = std::clamp(ui.phong_primary_light_only, 0, 1);
    ui.phong_secondary_scale = std::clamp(ui.phong_secondary_scale, 0, 100);
    ui.phong_ambient = std::clamp(ui.phong_ambient, 0, 100);
    ui.phong_specular = std::clamp(ui.phong_specular, 0, 200);
    ui.active_lights = std::clamp(ui.active_lights, 0, 3);
    for (auto& light : ui.lights) {
        light.yaw = std::clamp(light.yaw, 0, 359);
        light.pitch = std::clamp(light.pitch, 0, 180);
        light.intensity = std::clamp(light.intensity, 0, 400);
        light.r = std::clamp(light.r, 0, 255);
        light.g = std::clamp(light.g, 0, 255);
        light.b = std::clamp(light.b, 0, 255);
    }
    return false;
}

struct ModelStats {
    size_t mesh_count = 0;
    size_t triangle_count = 0;
    size_t vertex_count = 0;
};

struct AppState {
    int render_width = 1280;
    int render_height = 820;
    bool manual_render_resolution = false;
    int render_resolution_preset = 3;
    render renderer;
    Mat render_image;
    Model model;
    UiState ui;
    UiState applied_ui;
    bool ui_applied = false;
    string model_path;
    string environment_path;
    string background_image_path;
    string programmable_shader_source = ProgrammableShaderProgram::defaultSource();
    string programmable_shader_status = "Not compiled";
    shared_ptr<ProgrammableShaderProgram> programmable_shader_program;
    bool programmable_shader_valid = false;
    string status_message = "Ready";
    float default_camera_distance = 3.0f;
    render::FrameProfile last_profile;
    OrbitCameraState camera;
    Vector3f eye = Vector3f(0.0f, 0.0f, 3.0f);
    Vector3f centre = Vector3f::Zero();
    Vector3f up = Vector3f(0.0f, 1.0f, 0.0f);
    Scalar background_color = Scalar(155, 155, 155);
    int background_mode = 0; // 0 color, 1 image

    AppState()
        : renderer(render_height, render_width),
          render_image(render_height, render_width, CV_8UC3, Scalar(155, 155, 155)) {
        ui = makeDefaultUiState();
        applied_ui = ui;
    }
};

float clampPitch(float pitch) {
    const float pitchLimit = 1.5f;
    return std::max(-pitchLimit, std::min(pitchLimit, pitch));
}

void updateCameraPose(AppState& app) {
    const float cosPitch = std::cos(app.camera.pitch);
    const Vector3f offset(
        app.camera.distance * cosPitch * std::sin(app.camera.yaw),
        app.camera.distance * std::sin(app.camera.pitch),
        app.camera.distance * cosPitch * std::cos(app.camera.yaw));

    app.eye = app.camera.target + offset;
    app.centre = app.camera.target;

    Vector3f forward = (app.centre - app.eye).normalized();
    Vector3f right = forward.cross(kWorldUp);
    if (right.squaredNorm() <= 1e-8f) {
        right = Vector3f(1.0f, 0.0f, 0.0f);
    }
    else {
        right.normalize();
    }
    app.up = right.cross(forward).normalized();
}

void resetCamera(AppState& app, const Vector3f& target, float distance) {
    app.camera.target = target;
    app.camera.yaw = 0.0f;
    app.camera.pitch = 0.0f;
    app.camera.distance = std::max(distance, 0.3f);
    updateCameraPose(app);
}

void enableFastFpModes() {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}

void setStatus(AppState& app, const string& message) {
    app.status_message = message;
    cout << "[UI] " << message << endl;
}

QString qPath(const string& path) {
    return QString::fromLocal8Bit(path.c_str());
}

string nativePathString(const QString& path) {
    QByteArray bytes = QDir::toNativeSeparators(path).toLocal8Bit();
    return string(bytes.constData(), static_cast<size_t>(bytes.size()));
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
           a.backface_culling != b.backface_culling ||
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
    resetCamera(app, Vector3f::Zero(), app.default_camera_distance);
}

void resizeRenderBuffers(AppState& app, int width, int height) {
    width = std::max(width, 320);
    height = std::max(height, 240);
    if (app.render_width == width && app.render_height == height) {
        return;
    }
    app.render_width = width;
    app.render_height = height;
    app.render_image = Mat(height, width, CV_8UC3, Scalar(155, 155, 155));
    app.renderer.height = height;
    app.renderer.weight = width;
    app.renderer.zbuff = RenderDepthBuffer(height, width);
    app.renderer.zbuff.setConstant(makeRenderDepth(RENDER_DEPTH_CLEAR));
    app.renderer.tile_bins_cache.clear();
    app.renderer.set_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    app.renderer.set_projection(static_cast<float>(std::clamp(app.ui.fov, 30, 120)), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
    app.renderer.shadow_cache_valid = 0;
    app.renderer.shadow_cache_dirty = 1;
}

bool loadModelFromPath(AppState& app, const string& path, string* errorMessage = nullptr) {
    Model loadedModel;
    if (loadedModel.modelread(path) != 1) {
        if (errorMessage) {
            *errorMessage = "Failed to load model";
        }
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
    setStatus(app, "Loaded model: " + path);
    return true;
}

bool loadEnvironmentFromPath(AppState& app, const string& path, string* errorMessage = nullptr) {
    if (app.renderer.setEnvironmentMap(path.c_str()) != 1) {
        if (errorMessage) {
            *errorMessage = "Failed to load environment";
        }
        setStatus(app, "Failed to load environment");
        return false;
    }
    app.environment_path = path;
    setStatus(app, "Loaded environment: " + path);
    return true;
}

void clearEnvironment(AppState& app) {
    app.environment_path.clear();
    app.renderer.setEnvironmentMap(nullptr);
    setStatus(app, "Environment cleared; using sky fallback");
}

void syncRenderBackground(AppState& app) {
    app.renderer.setBackgroundColor(app.background_color);
    if (app.background_mode == 1 && !app.background_image_path.empty()) {
        Mat background = imread(app.background_image_path, IMREAD_UNCHANGED);
        if (!background.empty()) {
            app.renderer.setBackgroundImage(background);
            return;
        }
        app.background_mode = 0;
    }
    app.renderer.clearBackgroundImage();
}

void applyUiToRenderer(AppState& app) {
    const bool firstApply = !app.ui_applied;
    const UiState& ui = app.ui;
    const UiState& previous = app.applied_ui;

    app.renderer.setShadingLook(
        ui.shading_look == 1 ? ShadingLook::StylizedPhong :
        (ui.shading_look == 2 ? ShadingLook::Programmable : ShadingLook::RealisticPbr));
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

QString formatMs(double value) {
    return QString::number(value, 'f', value >= 100.0 ? 0 : 2) + " ms";
}

ModelStats gatherModelStats(const Model& model) {
    ModelStats stats;
    stats.mesh_count = model.meshes.size();
    for (const auto& mesh : model.meshes) {
        stats.vertex_count += mesh.vertexCount();
        stats.triangle_count += mesh.indices.size() / 3;
    }
    return stats;
}

QString shadowModeText(const AppState& app) {
    if (!app.ui.shadow_enabled) {
        return "Shadow OFF";
    }
    QString text = QString::fromLatin1(app.renderer.shadowTechniqueName());
    if (app.renderer.getShadowTechnique() == ShadowTechnique::RasterEmbree && !app.renderer.embreeAvailable()) {
        text += " (fallback)";
    }
    return text;
}

QString channelLabel(int index) {
    static const array<const char*, 4> labels = { "R", "G", "B", "A" };
    index = std::clamp(index, 0, static_cast<int>(labels.size()) - 1);
    return QString::fromLatin1(labels[index]);
}

QPalette createApplicationPalette(UiTheme theme) {
    const ThemePalette p = themePalette(theme);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(p.window));
    palette.setColor(QPalette::WindowText, QColor(p.text));
    palette.setColor(QPalette::Base, QColor(p.surface));
    palette.setColor(QPalette::AlternateBase, QColor(p.elevated));
    palette.setColor(QPalette::ToolTipBase, QColor(p.elevated));
    palette.setColor(QPalette::ToolTipText, QColor(p.text));
    palette.setColor(QPalette::Text, QColor(p.text));
    palette.setColor(QPalette::Button, QColor(p.button));
    palette.setColor(QPalette::ButtonText, QColor(p.text));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(p.accent));
    palette.setColor(QPalette::HighlightedText, QColor(theme == UiTheme::Graphite ? "#11161a" : "#ffffff"));
    palette.setColor(QPalette::Mid, QColor(p.border));
    palette.setColor(QPalette::PlaceholderText, QColor(p.muted));
    return palette;
}

void applyApplicationTheme(QApplication& app, UiTheme theme) {
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setPalette(createApplicationPalette(theme));
}

class RenderViewport : public QWidget {
public:
    explicit RenderViewport(AppState& app, QWidget* parent = nullptr)
        : QWidget(parent), app_(app) {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setFrame(QImage frame) {
        frame_ = std::move(frame);
        update();
    }

    void setInteractionCallback(function<void()> callback) {
        interaction_callback_ = std::move(callback);
    }

    void setResizeCallback(function<void(int, int)> callback) {
        resize_callback_ = std::move(callback);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPalette pal = palette();
        const QColor matte = pal.color(QPalette::Window);
        const QColor surface = pal.color(QPalette::Base);
        const QColor edge = pal.color(QPalette::Mid);
        const QColor text = pal.color(QPalette::WindowText);
        painter.fillRect(rect(), matte);

        QRect targetRect = rect().adjusted(18, 18, -18, -18);
        if (!frame_.isNull()) {
            QSize imageSize = frame_.size();
            imageSize.scale(targetRect.size(), Qt::KeepAspectRatio);
            QRect imageRect(
                QPoint(targetRect.center().x() - imageSize.width() / 2, targetRect.center().y() - imageSize.height() / 2),
                imageSize);
            painter.setPen(QPen(edge, 1));
            painter.setBrush(surface);
            painter.drawRoundedRect(targetRect, 8, 8);
            painter.drawImage(imageRect, frame_);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(surface.red(), surface.green(), surface.blue(), 220));
        painter.drawRoundedRect(QRect(24, 24, 420, 40), 6, 6);
        painter.setPen(text);
        painter.setFont(QFont("Segoe UI", 10, QFont::DemiBold));
        QString overlay = QString("%1  |  %2  |  %3")
            .arg(QString::fromLatin1(app_.renderer.shadingLookName()))
            .arg(shadowModeText(app_))
            .arg(formatMs(app_.last_profile.render_total_ms));
        painter.drawText(QRect(38, 24, 390, 40), Qt::AlignVCenter | Qt::AlignLeft, overlay);

        painter.setBrush(QColor(surface.red(), surface.green(), surface.blue(), 200));
        painter.drawRoundedRect(QRect(24, height() - 54, 330, 34), 6, 6);
        painter.setPen(pal.color(QPalette::Text));
        painter.setFont(QFont("Segoe UI", 9));
        painter.drawText(QRect(38, height() - 54, 300, 34), Qt::AlignVCenter | Qt::AlignLeft,
                         "LMB orbit  RMB pan  Wheel zoom");

        if (app_.model.meshes.empty()) {
            painter.setPen(text);
            painter.setFont(QFont("Segoe UI", 16, QFont::DemiBold));
            painter.drawText(rect(), Qt::AlignCenter, "Load a model to start rendering");
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            app_.camera.rotating = true;
        }
        else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            app_.camera.panning = true;
        }
        app_.camera.lastMouse = event->pos();
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            app_.camera.rotating = false;
        }
        else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            app_.camera.panning = false;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        QPoint delta = event->pos() - app_.camera.lastMouse;
        app_.camera.lastMouse = event->pos();

        if (app_.camera.rotating && (event->buttons() & Qt::LeftButton)) {
            const float orbitSensitivity = 0.008f;
            app_.camera.yaw -= static_cast<float>(delta.x()) * orbitSensitivity;
            app_.camera.pitch = clampPitch(app_.camera.pitch - static_cast<float>(delta.y()) * orbitSensitivity);
            updateCameraPose(app_);
            if (interaction_callback_) {
                interaction_callback_();
            }
        }
        else if (app_.camera.panning && (event->buttons() & (Qt::RightButton | Qt::MiddleButton))) {
            Vector3f forward = (app_.centre - app_.eye).normalized();
            Vector3f right = forward.cross(app_.up);
            if (right.squaredNorm() > 1e-8f) {
                right.normalize();
            }
            Vector3f cameraUp = app_.up.normalized();
            const float panScale = std::max(app_.camera.distance, 0.5f) * 0.0015f;
            app_.camera.target += (-static_cast<float>(delta.x()) * right + static_cast<float>(delta.y()) * cameraUp) * panScale;
            updateCameraPose(app_);
            if (interaction_callback_) {
                interaction_callback_();
            }
        }
        QWidget::mouseMoveEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        const QPoint delta = event->angleDelta();
        if (delta.y() != 0) {
            float zoomFactor = std::pow(0.88f, static_cast<float>(delta.y()) / 120.0f);
            app_.camera.distance = std::max(0.3f, std::min(50.0f, app_.camera.distance * zoomFactor));
            updateCameraPose(app_);
            if (interaction_callback_) {
                interaction_callback_();
            }
        }
        event->accept();
    }

    void resizeEvent(QResizeEvent* event) override {
        if (resize_callback_) {
            resize_callback_(event->size().width(), event->size().height());
        }
        QWidget::resizeEvent(event);
    }

private:
    AppState& app_;
    QImage frame_;
    function<void()> interaction_callback_;
    function<void(int, int)> resize_callback_;
};

class HaoRenderWindow : public QMainWindow {
public:
    HaoRenderWindow(const string& startupModelPath, const string& startupEnvironmentPath, int startupShadowTechnique, QWidget* parent = nullptr)
        : QMainWindow(parent) {
        app_.ui.shadow_technique = startupShadowTechnique;
        resetCamera(app_, Vector3f::Zero(), app_.default_camera_distance);
        loadSession();
        applyThemeToApplication();

        setWindowTitle(tx("HaoRender Studio", "HaoRender 工作台"));
        resize(1380, 860);
        setMinimumSize(900, 620);

        buildUi();
        if (restore_last_session_ && !pending_geometry_.isEmpty()) {
            restoreGeometry(pending_geometry_);
        }
        connectControls();
        syncWidgetsFromUi();
        compileProgrammableShader(false);
        applyRenderResolutionPreference(false, false, false);
        updateLightSwatches();
        updateLightTabState();
        updateMaterialsTabs();
        updateInspector();

        render_timer_ = new QTimer(this);
        connect(render_timer_, &QTimer::timeout, this, [this]() {
            if (render_requested_) {
                renderFrame();
            }
        });
        render_timer_->start(16);

        string initialModelPath = restore_last_session_ && !app_.model_path.empty() ? app_.model_path : startupModelPath;
        string initialEnvPath = restore_last_session_ && !app_.environment_path.empty() ? app_.environment_path : startupEnvironmentPath;

        syncRenderBackground(app_);

        if (!initialModelPath.empty() && std::filesystem::exists(initialModelPath)) {
            string errorMessage;
            if (!loadModelFromPath(app_, initialModelPath, &errorMessage)) {
                showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
            }
        }
        else {
            showStatus(tx("No startup model found. Use Open Model to begin.", "没有找到启动模型，请先导入模型。"), false);
        }

        if (!initialEnvPath.empty()) {
            string errorMessage;
            if (!loadEnvironmentFromPath(app_, initialEnvPath, &errorMessage)) {
                showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
            }
        }
        else {
            showStatus(tx("No environment map found. Using sky fallback.", "没有找到环境图，使用天空回退。"), false);
        }

        updateMaterialsTabs();
        updateInspector();
        updateStatusChips();
        if (session_recovered_) {
            saveSession();
            showStatus(tx("Recovered a corrupted session and restored lighting defaults.",
                          "检测到损坏的会话参数，已恢复默认灯光与曝光。"), false);
        }
        requestRender();
    }

private:
    QString tx(const QString& english, const QString& chinese) const {
        return language_ == UiLanguage::Chinese ? chinese : english;
    }

    ThemePalette paletteColors() const {
        return themePalette(theme_);
    }

    QString buttonStyle() const {
        const auto p = paletteColors();
        return QString("QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:8px 12px; }"
                       "QPushButton:hover { background:%4; border-color:%5; }")
            .arg(p.button, p.text, p.border, p.buttonHover, p.accent);
    }

    QString comboStyle() const {
        const auto p = paletteColors();
        return QString("QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:6px 8px; min-height:28px; }"
                       "QComboBox::drop-down { border:none; width:20px; }"
                       "QAbstractItemView { background:%4; color:%2; border:1px solid %3; selection-background-color:%5; }")
            .arg(p.elevated, p.text, p.border, p.surface, p.accent);
    }

    QString spinStyle() const {
        const auto p = paletteColors();
        return QString("QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:6px 8px; min-height:28px; }")
            .arg(p.elevated, p.text, p.border);
    }

    QString sectionStyle() const {
        const auto p = paletteColors();
        return QString("QGroupBox { background:%1; border:1px solid %2; border-radius:10px; margin-top:16px; padding:12px; font-weight:600; color:%3; }"
                       "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color:%4; }")
            .arg(p.surface, p.border, p.text, p.accent);
    }

    QLabel* createChip(const QString& text) {
        auto* label = new QLabel(text, this);
        const auto p = paletteColors();
        label->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:8px; padding:6px 10px; color:%3;")
                                 .arg(p.elevated, p.border, p.text));
        return label;
    }

    QWidget* makeScrollableTab(QWidget* inner) {
        auto* area = new QScrollArea();
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        area->setWidget(inner);
        return area;
    }

    CollapsibleGroupBox* makeSection(const QString& title) {
        auto* box = new CollapsibleGroupBox(title);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        box->setStyleSheet(sectionStyle());
        return box;
    }

    QSpinBox* makeSpin(int minValue, int maxValue, int step = 1) {
        auto* spin = new QSpinBox(this);
        spin->setRange(minValue, maxValue);
        spin->setSingleStep(step);
        spin->setStyleSheet(spinStyle());
        return spin;
    }

    QDoubleSpinBox* makeDoubleSpin(double minValue, double maxValue, double step, int decimals = 2) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minValue, maxValue);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        spin->setStyleSheet(spinStyle().replace("QSpinBox", "QDoubleSpinBox"));
        return spin;
    }

    void applyThemeToApplication() const {
        if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
            applyApplicationTheme(*app, theme_);
        }
    }

    QString translatedLookName() const {
        if (app_.ui.shading_look == 1) {
            return tx("Stylized Phong", "风格化 Phong");
        }
        if (app_.ui.shading_look == 2) {
            return tx("Programmable", "可编程着色");
        }
        return tx("Realistic PBR", "写实 PBR");
    }

    QString translatedShadowMode() const {
        if (!app_.ui.shadow_enabled) {
            return tx("Shadow Off", "阴影关闭");
        }
        QString mode = app_.ui.shadow_technique == 1 ? tx("Raster + Embree", "光栅 + Embree") : tx("Shadow Map", "阴影贴图");
        if (app_.ui.shadow_technique == 1 && !app_.renderer.embreeAvailable()) {
            mode += tx(" (Fallback)", "（回退）");
        }
        return mode;
    }

    QString embreeStatusText() const {
        return app_.renderer.embreeAvailable() ? tx("Available", "可用") : tx("Fallback / Unavailable", "回退 / 不可用");
    }

    QColor currentBackgroundColorQ() const {
        return QColor(
            static_cast<int>(app_.background_color[2]),
            static_cast<int>(app_.background_color[1]),
            static_cast<int>(app_.background_color[0]));
    }

    QString resolutionPresetText(int index) const {
        index = std::clamp(index, 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1);
        const ResolutionPreset& preset = kRenderResolutionPresets[index];
        return QString("%1 x %2").arg(preset.width).arg(preset.height);
    }

    QString programmableTemplateText(int index) const {
        index = std::clamp(index, 0, static_cast<int>(std::size(kProgrammableShaderTemplates)) - 1);
        const ShaderTemplatePreset& preset = kProgrammableShaderTemplates[index];
        return tx(preset.name_en, preset.name_zh);
    }

    std::string programmableTemplateSource(int index) const {
        index = std::clamp(index, 0, static_cast<int>(std::size(kProgrammableShaderTemplates)) - 1);
        return kProgrammableShaderTemplates[index].source;
    }

    void updateShadingControlState() {
        const int lookIndex = std::clamp(app_.ui.shading_look, 0, 2);
        if (shading_mode_stack_) {
            shading_mode_stack_->setCurrentIndex(lookIndex);
        }
    }

    void updateResolutionControlState() {
        if (render_resolution_combo_) {
            render_resolution_combo_->setEnabled(true);
            render_resolution_combo_->setToolTip(app_.manual_render_resolution
                                                     ? tx("Using fixed internal render resolution.", "当前使用固定内部渲染分辨率。")
                                                     : tx("Choose a preset to switch to fixed internal render resolution.", "选择预设后会切换到固定内部渲染分辨率。"));
        }
    }

    void applyRenderResolutionPreference(bool useViewportSizeWhenAuto, bool persist, bool scheduleRender = true) {
        if (app_.manual_render_resolution) {
            const int presetIndex = std::clamp(app_.render_resolution_preset, 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1);
            const ResolutionPreset& preset = kRenderResolutionPresets[presetIndex];
            resizeRenderBuffers(app_, preset.width, preset.height);
        }
        else if (useViewportSizeWhenAuto && viewport_) {
            const int desiredWidth = std::clamp(viewport_->width() - 20, 720, 1600);
            const int desiredHeight = std::clamp(viewport_->height() - 20, 540, 1200);
            resizeRenderBuffers(app_, desiredWidth, desiredHeight);
        }

        updateResolutionControlState();
        if (resolution_label_) {
            resolution_label_->setText(QString("%1 x %2").arg(app_.render_width).arg(app_.render_height));
        }
        if (persist) {
            saveSession();
        }
        if (scheduleRender) {
            requestRender();
        }
    }

    void updateProgrammableShaderStatusLabel() {
        if (!programmable_shader_status_label_) {
            return;
        }
        programmable_shader_status_label_->setText(QString::fromLocal8Bit(app_.programmable_shader_status.c_str()));
        const QString color = app_.programmable_shader_valid ? "#1f7a52" : "#a63d40";
        programmable_shader_status_label_->setStyleSheet(QString("color:%1; font-weight:600;").arg(color));
    }

    void compileProgrammableShader(bool showMessage) {
        app_.programmable_shader_source = programmable_shader_editor_
                                              ? programmable_shader_editor_->toPlainText().toStdString()
                                              : ProgrammableShaderProgram::defaultSource();
        std::string errorMessage;
        const bool hadPreviousProgram = static_cast<bool>(app_.programmable_shader_program);
        std::shared_ptr<ProgrammableShaderProgram> program =
            ProgrammableShaderProgram::compile(app_.programmable_shader_source, errorMessage);
        if (program) {
            app_.programmable_shader_program = program;
            app_.programmable_shader_valid = true;
            app_.programmable_shader_status = tx("Programmable shader compiled successfully.",
                                                 "可编程着色器编译成功。").toStdString();
            app_.renderer.setProgrammableShaderProgram(program);
            updateProgrammableShaderStatusLabel();
            saveSession();
            if (showMessage) {
                showStatus(tx("Programmable shader compiled.", "可编程着色器已编译。"), false);
            }
            requestRender();
        }
        else {
            app_.programmable_shader_valid = false;
            if (hadPreviousProgram) {
                app_.programmable_shader_status =
                    tx("Compile failed. Keeping the last compiled shader.\n", "编译失败，当前继续使用上一份已编译着色器。\n").toStdString() +
                    errorMessage;
            }
            else {
                app_.programmable_shader_status = errorMessage;
                app_.renderer.setProgrammableShaderProgram(nullptr);
            }
            updateProgrammableShaderStatusLabel();
            if (showMessage) {
                showStatus(tx("Programmable shader compile failed.", "可编程着色器编译失败。"), true);
            }
        }
    }

    void buildUi() {
        buildToolbar();

        auto* central = new QWidget(this);
        auto* centralLayout = new QVBoxLayout(central);
        centralLayout->setContentsMargins(12, 12, 12, 12);
        centralLayout->setSpacing(12);

        auto* header = new QFrame(central);
        header->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:10px; }")
                                  .arg(paletteColors().panel, paletteColors().border));
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(16, 12, 16, 12);
        headerLayout->setSpacing(10);

        auto* titleColumn = new QVBoxLayout();
        auto* title = new QLabel(tx("HaoRender Studio", "HaoRender 工作台"), header);
        title->setFont(QFont("Segoe UI", 20, QFont::DemiBold));
        title->setStyleSheet(QString("color:%1;").arg(paletteColors().text));
        auto* subtitle = new QLabel(tx("Desktop workspace for renderer tuning, scene iteration, and shading validation",
                                       "面向渲染调参、场景迭代与着色验证的桌面工作区"), header);
        subtitle->setStyleSheet(QString("color:%1;").arg(paletteColors().muted));
        subtitle->setWordWrap(true);
        header_title_label_ = title;
        header_subtitle_label_ = subtitle;
        titleColumn->addWidget(header_title_label_);
        titleColumn->addWidget(header_subtitle_label_);

        look_chip_ = createChip("Look");
        shadow_chip_ = createChip("Shadow");
        perf_chip_ = createChip("Frame");
        auto* chipLayout = new QHBoxLayout();
        chipLayout->setContentsMargins(0, 0, 0, 0);
        chipLayout->setSpacing(8);
        chipLayout->addWidget(look_chip_);
        chipLayout->addWidget(shadow_chip_);
        chipLayout->addWidget(perf_chip_);
        chipLayout->addStretch();

        headerLayout->addLayout(titleColumn, 1);
        headerLayout->addLayout(chipLayout);

        auto* viewportFrame = new QFrame(central);
        viewportFrame->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:10px; }")
                                         .arg(paletteColors().viewport, paletteColors().border));
        auto* viewportLayout = new QVBoxLayout(viewportFrame);
        viewportLayout->setContentsMargins(10, 10, 10, 10);
        viewportLayout->setSpacing(0);

        viewport_ = new RenderViewport(app_, viewportFrame);
        viewport_->setMinimumSize(320, 220);
        viewport_->setInteractionCallback([this]() {
            requestRender();
            updateInspector();
        });
        viewport_->setResizeCallback([this](int width, int height) {
            if (!app_.manual_render_resolution) {
                const int desiredWidth = std::clamp(width - 20, 720, 1600);
                const int desiredHeight = std::clamp(height - 20, 540, 1200);
                resizeRenderBuffers(app_, desiredWidth, desiredHeight);
            }
            requestRender();
            updateInspector();
        });
        viewportLayout->addWidget(viewport_);

        centralLayout->addWidget(header);
        centralLayout->addWidget(viewportFrame, 1);
        setCentralWidget(central);

        buildControlsDock();
        buildStatusBar();
    }

    void buildToolbar() {
        auto* toolbar = addToolBar(tx("Actions", "操作"));
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        toolbar->setStyleSheet(QString("QToolBar { background:%1; border:none; border-bottom:1px solid %2; spacing:8px; padding:8px; }"
                                       "QToolButton { background:%3; color:%4; border:1px solid %2; border-radius:8px; padding:8px 12px; }"
                                       "QToolButton:hover { background:%5; border-color:%6; }")
                                   .arg(paletteColors().panel, paletteColors().border, paletteColors().button, paletteColors().text, paletteColors().buttonHover, paletteColors().accent));

        open_model_action_ = new QAction(tx("Open Model", "导入模型"), this);
        open_env_action_ = new QAction(tx("Open Environment", "导入环境图"), this);
        clear_env_action_ = new QAction(tx("Clear Environment", "清除环境图"), this);
        reset_camera_action_ = new QAction(tx("Reset Camera", "重置相机"), this);
        save_snapshot_action_ = new QAction(tx("Save Snapshot", "保存截图"), this);

        connect(open_model_action_, &QAction::triggered, this, [this]() { openModelDialog(); });
        connect(open_env_action_, &QAction::triggered, this, [this]() { openEnvironmentDialog(); });
        connect(clear_env_action_, &QAction::triggered, this, [this]() {
            clearEnvironment(app_);
            updateInspector();
            updateStatusChips();
            saveSession();
            showStatus(QString::fromLocal8Bit(app_.status_message.c_str()), false);
            requestRender();
        });
        connect(reset_camera_action_, &QAction::triggered, this, [this]() {
            resetCamera(app_, Vector3f::Zero(), app_.default_camera_distance);
            showStatus(tx("Camera reset", "相机已重置"), false);
            updateInspector();
            requestRender();
        });
        connect(save_snapshot_action_, &QAction::triggered, this, [this]() { saveScreenshot(); });

        toolbar->addAction(open_model_action_);
        toolbar->addAction(open_env_action_);
        toolbar->addAction(clear_env_action_);
        toolbar->addSeparator();
        toolbar->addAction(reset_camera_action_);
        toolbar->addAction(save_snapshot_action_);

        auto* spacer = new QWidget(this);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(spacer);

        toolbar_hint_label_ = new QLabel(tx("Desktop UI for lighting, shading, shadows, and debugging",
                                            "用于灯光、着色、阴影与调试的桌面界面"), this);
        toolbar_hint_label_->setStyleSheet(QString("color:%1;").arg(paletteColors().muted));
        toolbar->addWidget(toolbar_hint_label_);
    }

    void buildControlsDock() {
        auto* dock = new QDockWidget(tx("Controls", "控制面板"), this);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        const auto p = paletteColors();
        dock->setStyleSheet(QString("QDockWidget { color:%1; }"
                                    "QDockWidget::title { background:%2; border-bottom:1px solid %3; padding:10px 12px; text-align:left; font-weight:600; }")
                                .arg(p.text, p.panel, p.border));

        dock_tabs_ = new QTabWidget(dock);
        dock_tabs_->setStyleSheet(QString("QTabWidget::pane { border:none; }"
                                          "QTabBar::tab { background:%1; color:%2; padding:10px 14px; border-top-left-radius:8px; border-top-right-radius:8px; margin-right:4px; }"
                                          "QTabBar::tab:selected { background:%3; color:%4; }")
                                      .arg(p.surface, p.muted, p.elevated, p.text));
        dock_tabs_->addTab(makeScrollableTab(buildWorkspaceTab()), tx("Workspace", "工作区"));
        dock_tabs_->addTab(makeScrollableTab(buildSceneTab()), tx("Scene", "场景"));
        dock_tabs_->addTab(makeScrollableTab(buildShadingTab()), tx("Shading", "着色"));
        dock_tabs_->addTab(makeScrollableTab(buildLightsTab()), tx("Lights", "灯光"));
        dock_tabs_->addTab(makeScrollableTab(buildMaterialsTab()), tx("Materials", "材质"));
        dock_tabs_->addTab(makeScrollableTab(buildInspectTab()), tx("Inspect", "检查"));
        dock_tabs_->setCurrentIndex(std::clamp(preferred_dock_tab_, 0, dock_tabs_->count() - 1));

        dock->setWidget(dock_tabs_);
        dock->setMinimumWidth(260);
        addDockWidget(Qt::RightDockWidgetArea, dock);
    }

    QWidget* buildWorkspaceTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* interfaceSection = makeSection(tx("Interface", "界面"));
        auto* interfaceForm = new QFormLayout(interfaceSection);
        language_combo_ = new QComboBox(interfaceSection);
        ui_theme_combo_ = new QComboBox(interfaceSection);
        language_combo_->setStyleSheet(comboStyle());
        ui_theme_combo_->setStyleSheet(comboStyle());
        language_combo_->addItems({ "English", QString::fromLocal8Bit("中文") });
        ui_theme_combo_->addItems({ tx("Graphite", "石墨暗色"), tx("Ocean", "海雾浅色"), tx("Pearl", "珍珠暖色") });
        restore_session_check_ = new QCheckBox(tx("Restore last session on startup", "启动时恢复上次会话"), interfaceSection);
        restore_session_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        interfaceForm->addRow(tx("Language", "语言"), language_combo_);
        interfaceForm->addRow(tx("Theme", "主题"), ui_theme_combo_);
        interfaceForm->addRow("", restore_session_check_);

        auto* backgroundSection = makeSection(tx("Render Background", "渲染背景"));
        auto* backgroundForm = new QFormLayout(backgroundSection);
        background_mode_combo_ = new QComboBox(backgroundSection);
        background_mode_combo_->setStyleSheet(comboStyle());
        background_mode_combo_->addItems({ tx("Color", "纯色"), tx("Image", "图片") });
        background_color_button_ = new QPushButton(tx("Pick Background Color", "选择背景颜色"), backgroundSection);
        background_color_button_->setStyleSheet(buttonStyle());
        background_image_button_ = new QPushButton(tx("Load Background Image", "加载背景图片"), backgroundSection);
        background_image_button_->setStyleSheet(buttonStyle());
        background_image_clear_button_ = new QPushButton(tx("Clear Background Image", "清除背景图片"), backgroundSection);
        background_image_clear_button_->setStyleSheet(buttonStyle());
        background_color_preview_ = createChip(tx("Color Preview", "颜色预览"));
        background_image_label_ = new QLabel("-", backgroundSection);
        background_image_label_->setWordWrap(true);
        backgroundForm->addRow(tx("Mode", "模式"), background_mode_combo_);
        backgroundForm->addRow(tx("Color", "颜色"), background_color_button_);
        backgroundForm->addRow("", background_color_preview_);
        backgroundForm->addRow(tx("Image", "图片"), background_image_button_);
        backgroundForm->addRow("", background_image_clear_button_);
        backgroundForm->addRow(tx("Current Image", "当前图片"), background_image_label_);

        auto* presetSection = makeSection(tx("Session & Presets", "会话与预设"));
        auto* presetLayout = new QVBoxLayout(presetSection);
        save_preset_button_ = new QPushButton(tx("Save Preset", "保存预设"), presetSection);
        load_preset_button_ = new QPushButton(tx("Load Preset", "读取预设"), presetSection);
        save_preset_button_->setStyleSheet(buttonStyle());
        load_preset_button_->setStyleSheet(buttonStyle());
        presetLayout->addWidget(save_preset_button_);
        presetLayout->addWidget(load_preset_button_);

        layout->addWidget(interfaceSection);
        layout->addWidget(backgroundSection);
        layout->addWidget(presetSection);
        layout->addStretch();
        return page;
    }

    QWidget* buildSceneTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* cameraSection = makeSection(tx("Camera & View", "相机与视图"));
        auto* cameraForm = new QFormLayout(cameraSection);
        fov_spin_ = new IntSliderField(30, 120, 1, cameraSection);
        auto* cameraResetButton = new QPushButton(tx("Reset Camera", "重置相机"), cameraSection);
        cameraResetButton->setStyleSheet(buttonStyle());
        connect(cameraResetButton, &QPushButton::clicked, this, [this]() {
            resetCamera(app_, Vector3f::Zero(), app_.default_camera_distance);
            showStatus(tx("Camera reset", "相机已重置"), false);
            updateInspector();
            requestRender();
        });
        cameraForm->addRow(tx("Field of View", "视野角"), fov_spin_);
        cameraForm->addRow("", cameraResetButton);

        auto* renderSection = makeSection(tx("Render Tuning", "渲染调节"));
        auto* renderForm = new QFormLayout(renderSection);
        exposure_spin_ = new DoubleSliderField(0.0, 3.0, 0.05, 2, renderSection);
        normal_strength_spin_ = new DoubleSliderField(0.0, 2.0, 0.05, 2, renderSection);
        backface_culling_check_ = new QCheckBox(tx("Enable backface culling", "启用背面剔除"), renderSection);
        manual_resolution_check_ = new QCheckBox(tx("Use fixed preset resolution", "使用固定预设分辨率"), renderSection);
        render_resolution_combo_ = new QComboBox(renderSection);
        backface_culling_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        manual_resolution_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        render_resolution_combo_->setStyleSheet(comboStyle());
        for (size_t i = 0; i < std::size(kRenderResolutionPresets); ++i) {
            render_resolution_combo_->addItem(resolutionPresetText(static_cast<int>(i)));
        }
        renderForm->addRow(tx("Exposure", "曝光"), exposure_spin_);
        renderForm->addRow(tx("Normal Strength", "法线强度"), normal_strength_spin_);
        renderForm->addRow("", manual_resolution_check_);
        renderForm->addRow(tx("Resolution Preset", "分辨率预设"), render_resolution_combo_);
        renderForm->addRow("", backface_culling_check_);

        auto* shadowSection = makeSection(tx("Shadow & Cascades", "阴影与级联"));
        auto* shadowForm = new QFormLayout(shadowSection);
        shadow_enabled_check_ = new QCheckBox(tx("Enable shadows", "启用阴影"), shadowSection);
        shadow_cascade_check_ = new QCheckBox(tx("Enable cascade split", "启用级联分层"), shadowSection);
        shadow_enabled_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        shadow_cascade_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        shadow_technique_combo_ = new QComboBox(shadowSection);
        shadow_technique_combo_->addItems({ tx("Shadow Map", "阴影贴图"), tx("Raster + Embree", "光栅 + Embree") });
        near_shadow_res_combo_ = new QComboBox(shadowSection);
        far_shadow_res_combo_ = new QComboBox(shadowSection);
        near_shadow_res_combo_->setStyleSheet(comboStyle());
        far_shadow_res_combo_->setStyleSheet(comboStyle());
        shadow_technique_combo_->setStyleSheet(comboStyle());
        for (int value : kNearShadowResolutionOptions) {
            near_shadow_res_combo_->addItem(QString::number(value));
        }
        for (int value : kFarShadowResolutionOptions) {
            far_shadow_res_combo_->addItem(QString::number(value));
        }
        shadow_near_extent_spin_ = new DoubleSliderField(0.5, 5.0, 0.05, 2, shadowSection);
        shadow_near_depth_spin_ = new DoubleSliderField(0.5, 12.0, 0.1, 2, shadowSection);
        shadow_far_extent_spin_ = new DoubleSliderField(0.5, 10.0, 0.1, 2, shadowSection);
        shadow_far_depth_spin_ = new DoubleSliderField(0.5, 20.0, 0.1, 2, shadowSection);
        cascade_split_spin_ = new DoubleSliderField(0.1, 5.0, 0.05, 2, shadowSection);
        cascade_blend_spin_ = new DoubleSliderField(0.0, 2.0, 0.05, 2, shadowSection);
        shadowForm->addRow("", shadow_enabled_check_);
        shadowForm->addRow(tx("Shadow Technique", "阴影技术"), shadow_technique_combo_);
        shadowForm->addRow("", shadow_cascade_check_);
        shadowForm->addRow(tx("Near Shadow Resolution", "近阴影分辨率"), near_shadow_res_combo_);
        shadowForm->addRow(tx("Far Shadow Resolution", "远阴影分辨率"), far_shadow_res_combo_);
        shadowForm->addRow(tx("Near Extent", "近范围"), shadow_near_extent_spin_);
        shadowForm->addRow(tx("Near Depth", "近深度"), shadow_near_depth_spin_);
        shadowForm->addRow(tx("Far Extent", "远范围"), shadow_far_extent_spin_);
        shadowForm->addRow(tx("Far Depth", "远深度"), shadow_far_depth_spin_);
        shadowForm->addRow(tx("Cascade Split", "级联分割"), cascade_split_spin_);
        shadowForm->addRow(tx("Cascade Blend", "级联混合"), cascade_blend_spin_);

        layout->addWidget(cameraSection);
        layout->addWidget(renderSection);
        layout->addWidget(shadowSection);
        layout->addStretch();
        return page;
    }

    QWidget* buildShadingTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* lookSection = makeSection(tx("Shading Look", "着色风格"));
        auto* lookForm = new QFormLayout(lookSection);
        shading_look_combo_ = new QComboBox(lookSection);
        shading_look_combo_->addItems({ tx("Realistic PBR", "写实 PBR"), tx("Stylized Phong", "风格化 Phong"), tx("Programmable", "可编程着色") });
        shading_look_combo_->setStyleSheet(comboStyle());
        lookForm->addRow(tx("Look", "模式"), shading_look_combo_);

        shading_mode_stack_ = new QStackedWidget(page);
        shading_mode_stack_->setObjectName("ShadingModeStack");
        shading_mode_stack_->setStyleSheet(QString(
            "QStackedWidget#ShadingModeStack { background:transparent; border:none; }"));

        auto* pbrPage = new QWidget(shading_mode_stack_);
        auto* pbrLayout = new QVBoxLayout(pbrPage);
        pbrLayout->setContentsMargins(0, 0, 0, 0);
        pbrLayout->setSpacing(12);

        pbr_lighting_section_ = makeSection(tx("PBR Lighting", "PBR 光照"));
        auto* pbrLightingForm = new QFormLayout(pbr_lighting_section_);
        ibl_enabled_check_ = new QCheckBox(tx("Enable image based lighting", "启用图像光照"), pbr_lighting_section_);
        ibl_enabled_check_->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        ibl_diffuse_spin_ = new DoubleSliderField(0.0, 2.0, 0.05, 2, pbr_lighting_section_);
        ibl_specular_spin_ = new DoubleSliderField(0.0, 2.0, 0.05, 2, pbr_lighting_section_);
        sky_light_spin_ = new DoubleSliderField(0.0, 2.0, 0.05, 2, pbr_lighting_section_);
        pbrLightingForm->addRow("", ibl_enabled_check_);
        pbrLightingForm->addRow(tx("IBL Diffuse", "IBL 漫反射"), ibl_diffuse_spin_);
        pbrLightingForm->addRow(tx("IBL Specular", "IBL 高光"), ibl_specular_spin_);
        pbrLightingForm->addRow(tx("Sky Light", "天空光"), sky_light_spin_);

        pbr_section_ = makeSection(tx("PBR Channel Mapping", "PBR 通道映射"));
        auto* pbrForm = new QFormLayout(pbr_section_);
        metallic_channel_combo_ = new QComboBox(pbr_section_);
        roughness_channel_combo_ = new QComboBox(pbr_section_);
        ao_channel_combo_ = new QComboBox(pbr_section_);
        emissive_channel_combo_ = new QComboBox(pbr_section_);
        QStringList channelItems = { "R", "G", "B", "A" };
        for (QComboBox* combo : { metallic_channel_combo_, roughness_channel_combo_, ao_channel_combo_, emissive_channel_combo_ }) {
            combo->addItems(channelItems);
            combo->setStyleSheet(comboStyle());
        }
        pbrForm->addRow(tx("Metallic Channel", "金属度通道"), metallic_channel_combo_);
        pbrForm->addRow(tx("Roughness Channel", "粗糙度通道"), roughness_channel_combo_);
        pbrForm->addRow(tx("AO Channel", "AO 通道"), ao_channel_combo_);
        pbrForm->addRow(tx("Emissive Channel", "自发光通道"), emissive_channel_combo_);
        pbrLayout->addWidget(pbr_lighting_section_);
        pbrLayout->addWidget(pbr_section_);
        pbrLayout->addStretch();

        auto* phongPage = new QWidget(shading_mode_stack_);
        auto* phongLayout = new QVBoxLayout(phongPage);
        phongLayout->setContentsMargins(0, 0, 0, 0);
        phongLayout->setSpacing(12);

        phong_section_ = makeSection(tx("Stylized Phong", "风格化 Phong"));
        auto* phongForm = new QFormLayout(phong_section_);
        phong_hard_spec_check_ = new QCheckBox(tx("Hard-edge specular", "硬边高光"), phong_section_);
        phong_toon_diffuse_check_ = new QCheckBox(tx("Band diffuse", "分段漫反射"), phong_section_);
        phong_tonemap_check_ = new QCheckBox(tx("Apply tone mapping", "应用色调映射"), phong_section_);
        phong_primary_only_check_ = new QCheckBox(tx("Primary light only", "仅主光"), phong_section_);
        for (QCheckBox* check : { phong_hard_spec_check_, phong_toon_diffuse_check_, phong_tonemap_check_, phong_primary_only_check_ }) {
            check->setStyleSheet(QString("QCheckBox { color:%1; }").arg(paletteColors().text));
        }
        phong_secondary_spin_ = new DoubleSliderField(0.0, 1.0, 0.01, 2, phong_section_);
        phong_ambient_spin_ = new DoubleSliderField(0.0, 1.0, 0.01, 2, phong_section_);
        phong_specular_spin_ = new DoubleSliderField(0.0, 2.0, 0.02, 2, phong_section_);
        phongForm->addRow("", phong_hard_spec_check_);
        phongForm->addRow("", phong_toon_diffuse_check_);
        phongForm->addRow("", phong_tonemap_check_);
        phongForm->addRow("", phong_primary_only_check_);
        phongForm->addRow(tx("Secondary Light", "辅光"), phong_secondary_spin_);
        phongForm->addRow(tx("Ambient", "环境光"), phong_ambient_spin_);
        phongForm->addRow(tx("Specular", "高光"), phong_specular_spin_);
        phongLayout->addWidget(phong_section_);
        phongLayout->addStretch();

        auto* programmablePage = new QWidget(shading_mode_stack_);
        auto* programmablePageLayout = new QVBoxLayout(programmablePage);
        programmablePageLayout->setContentsMargins(0, 0, 0, 0);
        programmablePageLayout->setSpacing(12);

        programmable_section_ = makeSection(tx("Programmable Shader", "可编程着色器"));
        auto* programmableLayout = new QVBoxLayout(programmable_section_);
        programmableLayout->setSpacing(10);
        auto* programmableIntro = new QLabel(
            tx("Write a fragment-style RGB program. The built-in renderer still provides UV, normal, light, shadow, metallic, roughness, AO, emissive, and exposure data.",
               "编写一个 RGB 片元程序。渲染器会提供 UV、法线、主光、阴影、金属度、粗糙度、AO、自发光和曝光等数据。"),
            programmable_section_);
        programmableIntro->setWordWrap(true);
        programmableIntro->setStyleSheet(QString("color:%1;").arg(paletteColors().muted));

        auto* programmablePresetRow = new QHBoxLayout();
        programmablePresetRow->setContentsMargins(0, 0, 0, 0);
        programmablePresetRow->setSpacing(8);
        programmable_shader_preset_combo_ = new QComboBox(programmable_section_);
        programmable_shader_preset_combo_->setStyleSheet(comboStyle());
        for (size_t i = 0; i < std::size(kProgrammableShaderTemplates); ++i) {
            programmable_shader_preset_combo_->addItem(programmableTemplateText(static_cast<int>(i)));
        }
        programmable_shader_apply_preset_button_ = new QPushButton(tx("Load Example", "载入示例"), programmable_section_);
        programmable_shader_apply_preset_button_->setStyleSheet(buttonStyle());
        programmablePresetRow->addWidget(programmable_shader_preset_combo_, 1);
        programmablePresetRow->addWidget(programmable_shader_apply_preset_button_);

        programmable_shader_guide_ = new QPlainTextEdit(programmable_section_);
        programmable_shader_guide_->setReadOnly(true);
        programmable_shader_guide_->setPlainText(QString::fromStdString(ProgrammableShaderProgram::guideText()));
        programmable_shader_guide_->setMinimumHeight(170);
        programmable_shader_guide_->setStyleSheet(QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:8px; }")
                                                     .arg(paletteColors().elevated, paletteColors().text, paletteColors().border));

        programmable_shader_editor_ = new QPlainTextEdit(programmable_section_);
        programmable_shader_editor_->setPlainText(QString::fromStdString(app_.programmable_shader_source));
        programmable_shader_editor_->setMinimumHeight(200);
        programmable_shader_editor_->setStyleSheet(QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:8px; }")
                                                       .arg(paletteColors().elevated, paletteColors().text, paletteColors().border));

        auto* programmableButtonRow = new QHBoxLayout();
        programmableButtonRow->setContentsMargins(0, 0, 0, 0);
        programmableButtonRow->setSpacing(8);
        programmable_shader_compile_button_ = new QPushButton(tx("Compile Shader", "编译着色器"), programmable_section_);
        programmable_shader_template_button_ = new QPushButton(tx("Reset Template", "恢复模板"), programmable_section_);
        programmable_shader_compile_button_->setStyleSheet(buttonStyle());
        programmable_shader_template_button_->setStyleSheet(buttonStyle());
        programmableButtonRow->addWidget(programmable_shader_compile_button_);
        programmableButtonRow->addWidget(programmable_shader_template_button_);
        programmableButtonRow->addStretch();

        programmable_shader_status_label_ = new QLabel(tx("Not compiled yet.", "尚未编译。"), programmable_section_);
        programmable_shader_status_label_->setWordWrap(true);

        programmableLayout->addWidget(programmableIntro);
        programmableLayout->addLayout(programmablePresetRow);
        programmableLayout->addWidget(programmable_shader_guide_);
        programmableLayout->addWidget(programmable_shader_editor_);
        programmableLayout->addLayout(programmableButtonRow);
        programmableLayout->addWidget(programmable_shader_status_label_);
        programmablePageLayout->addWidget(programmable_section_);
        programmablePageLayout->addStretch();

        shading_mode_stack_->addWidget(pbrPage);
        shading_mode_stack_->addWidget(phongPage);
        shading_mode_stack_->addWidget(programmablePage);

        layout->addWidget(lookSection);
        layout->addWidget(shading_mode_stack_);
        layout->addStretch();
        return page;
    }

    QWidget* buildLightsTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* rigSection = makeSection(tx("Light Rig", "灯光布置"));
        auto* rigForm = new QFormLayout(rigSection);
        active_lights_spin_ = makeSpin(0, 3);
        active_lights_spin_->setStyleSheet(spinStyle());
        rigForm->addRow(tx("Active Lights", "启用灯光数"), active_lights_spin_);
        for (int i = 0; i < 3; ++i) {
            light_cards_[i] = buildLightCard(i);
            layout->addWidget(light_cards_[i]);
        }

        layout->addWidget(rigSection);
        layout->addStretch();
        return page;
    }

    QWidget* buildMaterialsTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* infoSection = makeSection(tx("Material Overview", "材质概览"));
        auto* infoLayout = new QVBoxLayout(infoSection);
        auto* infoLabel = new QLabel(tx("Each mesh gets its own tab with texture paths and counts.", "每个网格会显示独立页签，便于检查纹理路径与贴图数量。"), infoSection);
        infoLabel->setWordWrap(true);
        infoLayout->addWidget(infoLabel);

        materials_tabs_ = new QTabWidget(page);
        materials_tabs_->setStyleSheet(QString("QTabWidget::pane { border:none; }"
                                               "QTabBar::tab { background:%1; color:%2; padding:8px 12px; border-top-left-radius:8px; border-top-right-radius:8px; margin-right:4px; }"
                                               "QTabBar::tab:selected { background:%3; color:%4; }")
                                           .arg(paletteColors().surface, paletteColors().muted, paletteColors().elevated, paletteColors().text));

        layout->addWidget(infoSection);
        layout->addWidget(materials_tabs_);
        return page;
    }

    QWidget* buildLightCard(int index) {
        auto* card = new QFrame(this);
        card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:10px; }")
                                .arg(paletteColors().surface, paletteColors().border));
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(12);

        auto* title = new QLabel(tx("Light ", "灯光 ") + QString::number(index + 1), card);
        title->setStyleSheet(QString("font-size:15px; font-weight:700; color:%1;").arg(paletteColors().text));
        layout->addWidget(title);

        auto* group = makeSection(tx("Directional Light", "方向光"));
        auto* form = new QFormLayout(group);
        light_yaw_spin_[index] = new IntSliderField(0, 359, 1, group);
        light_pitch_spin_[index] = new IntSliderField(-90, 90, 1, group);
        light_intensity_spin_[index] = new DoubleSliderField(0.0, 4.0, 0.05, 2, group);
        light_r_spin_[index] = makeSpin(0, 255);
        light_g_spin_[index] = makeSpin(0, 255);
        light_b_spin_[index] = makeSpin(0, 255);
        light_color_button_[index] = new QPushButton(tx("Pick Color", "选择颜色"), group);
        light_color_button_[index]->setStyleSheet(buttonStyle());

        auto* colorRow = new QHBoxLayout();
        colorRow->setContentsMargins(0, 0, 0, 0);
        colorRow->setSpacing(8);
        colorRow->addWidget(light_r_spin_[index]);
        colorRow->addWidget(light_g_spin_[index]);
        colorRow->addWidget(light_b_spin_[index]);

        form->addRow(tx("Yaw", "偏航"), light_yaw_spin_[index]);
        form->addRow(tx("Elevation", "仰角"), light_pitch_spin_[index]);
        form->addRow(tx("Intensity", "强度"), light_intensity_spin_[index]);
        form->addRow(tx("Color RGB", "颜色 RGB"), colorRow);
        form->addRow("", light_color_button_[index]);

        layout->addWidget(group);
        layout->addStretch();
        return card;
    }

    QWidget* buildInspectTab() {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 20);
        layout->setSpacing(12);

        auto* assetSection = makeSection(tx("Assets", "资源"));
        auto* assetForm = new QFormLayout(assetSection);
        model_path_label_ = new QLabel("-", assetSection);
        environment_path_label_ = new QLabel("-", assetSection);
        model_path_label_->setWordWrap(true);
        environment_path_label_->setWordWrap(true);
        assetForm->addRow(tx("Model", "模型"), model_path_label_);
        assetForm->addRow(tx("Environment", "环境图"), environment_path_label_);

        auto* statsSection = makeSection(tx("Scene Stats", "场景统计"));
        auto* statsForm = new QFormLayout(statsSection);
        mesh_count_label_ = new QLabel("0", statsSection);
        triangle_count_label_ = new QLabel("0", statsSection);
        vertex_count_label_ = new QLabel("0", statsSection);
        resolution_label_ = new QLabel("-", statsSection);
        embree_label_ = new QLabel("-", statsSection);
        camera_label_ = new QLabel("-", statsSection);
        camera_label_->setWordWrap(true);
        statsForm->addRow(tx("Meshes", "网格"), mesh_count_label_);
        statsForm->addRow(tx("Triangles", "三角形"), triangle_count_label_);
        statsForm->addRow(tx("Vertices", "顶点"), vertex_count_label_);
        statsForm->addRow(tx("Render Resolution", "渲染分辨率"), resolution_label_);
        statsForm->addRow(tx("Embree", "Embree"), embree_label_);
        statsForm->addRow(tx("Camera", "相机"), camera_label_);

        auto* profileSection = makeSection(tx("Frame Profiler", "帧分析"));
        auto* profileForm = new QFormLayout(profileSection);
        profile_clear_label_ = new QLabel("-", profileSection);
        profile_shadow_near_label_ = new QLabel("-", profileSection);
        profile_shadow_far_label_ = new QLabel("-", profileSection);
        profile_vertex_label_ = new QLabel("-", profileSection);
        profile_clip_bin_label_ = new QLabel("-", profileSection);
        profile_raster_label_ = new QLabel("-", profileSection);
        profile_total_label_ = new QLabel("-", profileSection);
        profileForm->addRow(tx("Clear", "清屏"), profile_clear_label_);
        profileForm->addRow(tx("Shadow Near", "近阴影"), profile_shadow_near_label_);
        profileForm->addRow(tx("Shadow Far", "远阴影"), profile_shadow_far_label_);
        profileForm->addRow(tx("Vertex", "顶点"), profile_vertex_label_);
        profileForm->addRow(tx("Clip + Bin", "裁剪 + 分桶"), profile_clip_bin_label_);
        profileForm->addRow(tx("Raster + Shade", "光栅 + 着色"), profile_raster_label_);
        profileForm->addRow(tx("Render Total", "总耗时"), profile_total_label_);

        auto* notesSection = makeSection(tx("Last Event", "最近事件"));
        auto* notesLayout = new QVBoxLayout(notesSection);
        last_event_label_ = new QLabel(tx("Ready", "就绪"), notesSection);
        last_event_label_->setWordWrap(true);
        notesLayout->addWidget(last_event_label_);

        layout->addWidget(assetSection);
        layout->addWidget(statsSection);
        layout->addWidget(profileSection);
        layout->addWidget(notesSection);
        layout->addStretch();
        return page;
    }

    void buildStatusBar() {
        auto* bar = statusBar();
        bar->setSizeGripEnabled(true);
        const auto p = paletteColors();
        bar->setStyleSheet(QString("QStatusBar { background:%1; color:%2; border-top:1px solid %3; }")
                               .arg(p.panel, p.text, p.border));
        status_hint_ = new QLabel(tx("Ready", "就绪"), this);
        status_mode_ = createChip(tx("Mode", "模式"));
        status_perf_ = createChip(tx("Perf", "性能"));
        bar->addWidget(status_hint_, 1);
        bar->addPermanentWidget(status_mode_);
        bar->addPermanentWidget(status_perf_);
    }

    void syncWidgetsFromUi() {
        syncing_widgets_ = true;
        vector<QSignalBlocker> blockers;
        blockers.reserve(64);
        auto block = [&blockers](QObject* object) { blockers.emplace_back(object); };

        for (QObject* obj : { static_cast<QObject*>(fov_spin_->slider()), static_cast<QObject*>(exposure_spin_->slider()), static_cast<QObject*>(normal_strength_spin_->slider()),
                              static_cast<QObject*>(backface_culling_check_), static_cast<QObject*>(manual_resolution_check_), static_cast<QObject*>(render_resolution_combo_),
                              static_cast<QObject*>(shadow_enabled_check_), static_cast<QObject*>(shadow_technique_combo_),
                              static_cast<QObject*>(shadow_cascade_check_), static_cast<QObject*>(near_shadow_res_combo_), static_cast<QObject*>(far_shadow_res_combo_),
                              static_cast<QObject*>(shadow_near_extent_spin_->slider()), static_cast<QObject*>(shadow_near_depth_spin_->slider()), static_cast<QObject*>(shadow_far_extent_spin_->slider()),
                              static_cast<QObject*>(shadow_far_depth_spin_->slider()), static_cast<QObject*>(cascade_split_spin_->slider()), static_cast<QObject*>(cascade_blend_spin_->slider()),
                              static_cast<QObject*>(shading_look_combo_), static_cast<QObject*>(ibl_enabled_check_), static_cast<QObject*>(ibl_diffuse_spin_->slider()),
                              static_cast<QObject*>(ibl_specular_spin_->slider()), static_cast<QObject*>(sky_light_spin_->slider()), static_cast<QObject*>(metallic_channel_combo_),
                              static_cast<QObject*>(roughness_channel_combo_), static_cast<QObject*>(ao_channel_combo_), static_cast<QObject*>(emissive_channel_combo_),
                              static_cast<QObject*>(phong_hard_spec_check_), static_cast<QObject*>(phong_toon_diffuse_check_), static_cast<QObject*>(phong_tonemap_check_),
                              static_cast<QObject*>(phong_primary_only_check_), static_cast<QObject*>(phong_secondary_spin_->slider()), static_cast<QObject*>(phong_ambient_spin_->slider()),
                              static_cast<QObject*>(phong_specular_spin_->slider()), static_cast<QObject*>(active_lights_spin_), static_cast<QObject*>(language_combo_),
                              static_cast<QObject*>(ui_theme_combo_), static_cast<QObject*>(restore_session_check_), static_cast<QObject*>(background_mode_combo_) }) {
            block(obj);
        }
        if (programmable_shader_editor_) {
            block(programmable_shader_editor_);
        }
        for (int i = 0; i < 3; ++i) {
            block(light_yaw_spin_[i]->slider());
            block(light_pitch_spin_[i]->slider());
            block(light_intensity_spin_[i]->slider());
            block(light_r_spin_[i]);
            block(light_g_spin_[i]);
            block(light_b_spin_[i]);
        }

        fov_spin_->setValue(app_.ui.fov);
        exposure_spin_->setValue(app_.ui.exposure / 100.0);
        normal_strength_spin_->setValue(app_.ui.normal_strength / 100.0);
        manual_resolution_check_->setChecked(app_.manual_render_resolution);
        render_resolution_combo_->setCurrentIndex(std::clamp(app_.render_resolution_preset, 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1));
        backface_culling_check_->setChecked(app_.ui.backface_culling != 0);
        shadow_enabled_check_->setChecked(app_.ui.shadow_enabled != 0);
        shadow_technique_combo_->setCurrentIndex(app_.ui.shadow_technique);
        shadow_cascade_check_->setChecked(app_.ui.shadow_cascade_enabled != 0);
        near_shadow_res_combo_->setCurrentIndex(app_.ui.near_shadow_res_preset);
        far_shadow_res_combo_->setCurrentIndex(app_.ui.far_shadow_res_preset);
        shadow_near_extent_spin_->setValue(app_.ui.shadow_near_extent / 100.0);
        shadow_near_depth_spin_->setValue(app_.ui.shadow_near_depth / 100.0);
        shadow_far_extent_spin_->setValue(app_.ui.shadow_far_extent / 100.0);
        shadow_far_depth_spin_->setValue(app_.ui.shadow_far_depth / 100.0);
        cascade_split_spin_->setValue(app_.ui.cascade_split / 100.0);
        cascade_blend_spin_->setValue(app_.ui.cascade_blend / 100.0);
        shading_look_combo_->setCurrentIndex(app_.ui.shading_look);
        ibl_enabled_check_->setChecked(app_.ui.ibl_enabled != 0);
        ibl_diffuse_spin_->setValue(app_.ui.ibl_diffuse / 100.0);
        ibl_specular_spin_->setValue(app_.ui.ibl_specular / 100.0);
        sky_light_spin_->setValue(app_.ui.sky_light / 100.0);
        metallic_channel_combo_->setCurrentIndex(app_.ui.metallic_channel);
        roughness_channel_combo_->setCurrentIndex(app_.ui.roughness_channel);
        ao_channel_combo_->setCurrentIndex(app_.ui.ao_channel);
        emissive_channel_combo_->setCurrentIndex(app_.ui.emissive_channel);
        phong_hard_spec_check_->setChecked(app_.ui.phong_hard_specular != 0);
        phong_toon_diffuse_check_->setChecked(app_.ui.phong_toon_diffuse != 0);
        phong_tonemap_check_->setChecked(app_.ui.phong_use_tonemap != 0);
        phong_primary_only_check_->setChecked(app_.ui.phong_primary_light_only != 0);
        phong_secondary_spin_->setValue(app_.ui.phong_secondary_scale / 100.0);
        phong_ambient_spin_->setValue(app_.ui.phong_ambient / 100.0);
        phong_specular_spin_->setValue(app_.ui.phong_specular / 100.0);
        active_lights_spin_->setValue(app_.ui.active_lights);
        language_combo_->setCurrentIndex(language_ == UiLanguage::Chinese ? 1 : 0);
        ui_theme_combo_->setCurrentIndex(static_cast<int>(theme_));
        restore_session_check_->setChecked(restore_last_session_);
        background_mode_combo_->setCurrentIndex(std::clamp(app_.background_mode, 0, 1));
        if (programmable_shader_editor_) {
            programmable_shader_editor_->setPlainText(QString::fromStdString(app_.programmable_shader_source));
        }
        for (int i = 0; i < 3; ++i) {
            light_yaw_spin_[i]->setValue(app_.ui.lights[i].yaw);
            light_pitch_spin_[i]->setValue(app_.ui.lights[i].pitch - 90);
            light_intensity_spin_[i]->setValue(app_.ui.lights[i].intensity / 100.0);
            light_r_spin_[i]->setValue(app_.ui.lights[i].r);
            light_g_spin_[i]->setValue(app_.ui.lights[i].g);
            light_b_spin_[i]->setValue(app_.ui.lights[i].b);
        }
        updateBackgroundPreview();
        updateShadingControlState();
        updateResolutionControlState();
        updateProgrammableShaderStatusLabel();
        syncing_widgets_ = false;
    }

    void readUiFromWidgets() {
        app_.ui.fov = fov_spin_->value();
        app_.ui.exposure = static_cast<int>(std::round(exposure_spin_->value() * 100.0));
        app_.ui.normal_strength = static_cast<int>(std::round(normal_strength_spin_->value() * 100.0));
        app_.ui.backface_culling = backface_culling_check_->isChecked() ? 1 : 0;
        app_.ui.shadow_enabled = shadow_enabled_check_->isChecked() ? 1 : 0;
        app_.ui.shadow_technique = shadow_technique_combo_->currentIndex();
        app_.ui.shadow_cascade_enabled = shadow_cascade_check_->isChecked() ? 1 : 0;
        app_.ui.near_shadow_res_preset = near_shadow_res_combo_->currentIndex();
        app_.ui.far_shadow_res_preset = far_shadow_res_combo_->currentIndex();
        app_.ui.shadow_near_extent = static_cast<int>(std::round(shadow_near_extent_spin_->value() * 100.0));
        app_.ui.shadow_near_depth = static_cast<int>(std::round(shadow_near_depth_spin_->value() * 100.0));
        app_.ui.shadow_far_extent = static_cast<int>(std::round(shadow_far_extent_spin_->value() * 100.0));
        app_.ui.shadow_far_depth = static_cast<int>(std::round(shadow_far_depth_spin_->value() * 100.0));
        app_.ui.cascade_split = static_cast<int>(std::round(cascade_split_spin_->value() * 100.0));
        app_.ui.cascade_blend = static_cast<int>(std::round(cascade_blend_spin_->value() * 100.0));

        app_.ui.shading_look = shading_look_combo_->currentIndex();
        app_.ui.ibl_enabled = ibl_enabled_check_->isChecked() ? 1 : 0;
        app_.ui.ibl_diffuse = static_cast<int>(std::round(ibl_diffuse_spin_->value() * 100.0));
        app_.ui.ibl_specular = static_cast<int>(std::round(ibl_specular_spin_->value() * 100.0));
        app_.ui.sky_light = static_cast<int>(std::round(sky_light_spin_->value() * 100.0));
        app_.ui.metallic_channel = metallic_channel_combo_->currentIndex();
        app_.ui.roughness_channel = roughness_channel_combo_->currentIndex();
        app_.ui.ao_channel = ao_channel_combo_->currentIndex();
        app_.ui.emissive_channel = emissive_channel_combo_->currentIndex();
        app_.ui.phong_hard_specular = phong_hard_spec_check_->isChecked() ? 1 : 0;
        app_.ui.phong_toon_diffuse = phong_toon_diffuse_check_->isChecked() ? 1 : 0;
        app_.ui.phong_use_tonemap = phong_tonemap_check_->isChecked() ? 1 : 0;
        app_.ui.phong_primary_light_only = phong_primary_only_check_->isChecked() ? 1 : 0;
        app_.ui.phong_secondary_scale = static_cast<int>(std::round(phong_secondary_spin_->value() * 100.0));
        app_.ui.phong_ambient = static_cast<int>(std::round(phong_ambient_spin_->value() * 100.0));
        app_.ui.phong_specular = static_cast<int>(std::round(phong_specular_spin_->value() * 100.0));

        app_.ui.active_lights = active_lights_spin_->value();
        for (int i = 0; i < 3; ++i) {
            app_.ui.lights[i].yaw = light_yaw_spin_[i]->value();
            app_.ui.lights[i].pitch = light_pitch_spin_[i]->value() + 90;
            app_.ui.lights[i].intensity = static_cast<int>(std::round(light_intensity_spin_[i]->value() * 100.0));
            app_.ui.lights[i].r = light_r_spin_[i]->value();
            app_.ui.lights[i].g = light_g_spin_[i]->value();
            app_.ui.lights[i].b = light_b_spin_[i]->value();
        }
    }

    void connectControls() {
        auto bindRenderIntSpin = [this](QSpinBox* spin) {
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { onControlChanged(); });
        };
        auto bindRenderDoubleSpin = [this](QDoubleSpinBox* spin) {
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { onControlChanged(); });
        };
        auto bindRenderInt = [this](IntSliderField* field) {
            connect(field->slider(), &QSlider::valueChanged, this, [this](int) { onControlChanged(); });
        };
        auto bindRenderDouble = [this](DoubleSliderField* field) {
            connect(field->slider(), &QSlider::valueChanged, this, [this](int) { onControlChanged(); });
        };
        auto bindRenderCheck = [this](QCheckBox* check) {
            connect(check, &QCheckBox::toggled, this, [this](bool) { onControlChanged(); });
        };
        auto bindRenderCombo = [this](QComboBox* combo) {
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { onControlChanged(); });
        };

        bindRenderInt(fov_spin_);
        bindRenderDouble(exposure_spin_);
        bindRenderDouble(normal_strength_spin_);
        bindRenderCheck(backface_culling_check_);
        connect(manual_resolution_check_, &QCheckBox::toggled, this, [this](bool checked) {
            app_.manual_render_resolution = checked;
            applyRenderResolutionPreference(true, true);
            updateInspector();
        });
        connect(render_resolution_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            app_.render_resolution_preset = std::clamp(index, 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1);
            if (!app_.manual_render_resolution) {
                manual_resolution_check_->setChecked(true);
                return;
            }
            applyRenderResolutionPreference(false, true);
            updateInspector();
        });
        bindRenderCheck(shadow_enabled_check_);
        bindRenderCombo(shadow_technique_combo_);
        bindRenderCheck(shadow_cascade_check_);
        bindRenderCombo(near_shadow_res_combo_);
        bindRenderCombo(far_shadow_res_combo_);
        bindRenderDouble(shadow_near_extent_spin_);
        bindRenderDouble(shadow_near_depth_spin_);
        bindRenderDouble(shadow_far_extent_spin_);
        bindRenderDouble(shadow_far_depth_spin_);
        bindRenderDouble(cascade_split_spin_);
        bindRenderDouble(cascade_blend_spin_);
        bindRenderCombo(shading_look_combo_);
        bindRenderCheck(ibl_enabled_check_);
        bindRenderDouble(ibl_diffuse_spin_);
        bindRenderDouble(ibl_specular_spin_);
        bindRenderDouble(sky_light_spin_);
        bindRenderCombo(metallic_channel_combo_);
        bindRenderCombo(roughness_channel_combo_);
        bindRenderCombo(ao_channel_combo_);
        bindRenderCombo(emissive_channel_combo_);
        bindRenderCheck(phong_hard_spec_check_);
        bindRenderCheck(phong_toon_diffuse_check_);
        bindRenderCheck(phong_tonemap_check_);
        bindRenderCheck(phong_primary_only_check_);
        bindRenderDouble(phong_secondary_spin_);
        bindRenderDouble(phong_ambient_spin_);
        bindRenderDouble(phong_specular_spin_);
        bindRenderIntSpin(active_lights_spin_);

        for (int i = 0; i < 3; ++i) {
            bindRenderInt(light_yaw_spin_[i]);
            bindRenderInt(light_pitch_spin_[i]);
            bindRenderDouble(light_intensity_spin_[i]);
            bindRenderIntSpin(light_r_spin_[i]);
            bindRenderIntSpin(light_g_spin_[i]);
            bindRenderIntSpin(light_b_spin_[i]);
            connect(light_color_button_[i], &QPushButton::clicked, this, [this, i]() {
                QColor current(light_r_spin_[i]->value(), light_g_spin_[i]->value(), light_b_spin_[i]->value());
                QColor color = QColorDialog::getColor(current, this, tx("Light Color", "灯光颜色"));
                if (!color.isValid()) {
                    return;
                }
                light_r_spin_[i]->setValue(color.red());
                light_g_spin_[i]->setValue(color.green());
                light_b_spin_[i]->setValue(color.blue());
                updateLightSwatches();
                onControlChanged();
            });
        }

        connect(language_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            language_ = index == 1 ? UiLanguage::Chinese : UiLanguage::English;
            saveSession();
            rebuildUi();
        });
        connect(ui_theme_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            theme_ = static_cast<UiTheme>(std::clamp(index, 0, 2));
            saveSession();
            rebuildUi();
        });
        connect(restore_session_check_, &QCheckBox::toggled, this, [this](bool checked) {
            restore_last_session_ = checked;
            saveSession();
        });
        connect(background_mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            app_.background_mode = index;
            syncRenderBackground(app_);
            updateBackgroundPreview();
            saveSession();
            requestRender();
        });
        connect(background_color_button_, &QPushButton::clicked, this, [this]() { pickBackgroundColor(); });
        connect(background_image_button_, &QPushButton::clicked, this, [this]() { pickBackgroundImage(); });
        connect(background_image_clear_button_, &QPushButton::clicked, this, [this]() {
            app_.background_image_path.clear();
            app_.background_mode = 0;
            syncRenderBackground(app_);
            updateBackgroundPreview();
            saveSession();
            showStatus(tx("Background image cleared", "背景图片已清除"), false);
            requestRender();
        });
        connect(save_preset_button_, &QPushButton::clicked, this, [this]() { savePresetDialog(); });
        connect(load_preset_button_, &QPushButton::clicked, this, [this]() { loadPresetDialog(); });
        connect(programmable_shader_apply_preset_button_, &QPushButton::clicked, this, [this]() {
            if (programmable_shader_editor_) {
                const int presetIndex = programmable_shader_preset_combo_ ? programmable_shader_preset_combo_->currentIndex() : 0;
                programmable_shader_editor_->setPlainText(QString::fromStdString(programmableTemplateSource(presetIndex)));
            }
            compileProgrammableShader(true);
        });
        connect(programmable_shader_compile_button_, &QPushButton::clicked, this, [this]() { compileProgrammableShader(true); });
        connect(programmable_shader_template_button_, &QPushButton::clicked, this, [this]() {
            if (programmable_shader_editor_) {
                programmable_shader_editor_->setPlainText(QString::fromStdString(ProgrammableShaderProgram::defaultSource()));
            }
            compileProgrammableShader(true);
        });
        connect(programmable_shader_editor_, &QPlainTextEdit::textChanged, this, [this]() {
            app_.programmable_shader_source = programmable_shader_editor_->toPlainText().toStdString();
            app_.programmable_shader_valid = false;
            app_.programmable_shader_status = tx("Programmable shader edited. Click Compile Shader to apply.",
                                                 "可编程着色器已修改，点击“编译着色器”后生效。").toStdString();
            updateProgrammableShaderStatusLabel();
            saveSession();
        });
    }

    void onControlChanged() {
        if (syncing_widgets_) {
            return;
        }
        readUiFromWidgets();
        updateLightSwatches();
        updateLightTabState();
        updateShadingControlState();
        updateStatusChips();
        saveSession();
        requestRender();
    }

    void updateLightSwatches() {
        for (int i = 0; i < 3; ++i) {
            QColor color(light_r_spin_[i]->value(), light_g_spin_[i]->value(), light_b_spin_[i]->value());
            light_color_button_[i]->setStyleSheet(
                QString("QPushButton { background: rgb(%1, %2, %3); color: %4; border: 1px solid #4a5668; border-radius: 6px; padding: 8px 12px; }"
                        "QPushButton:hover { border-color: #58becc; }")
                    .arg(color.red())
                    .arg(color.green())
                    .arg(color.blue())
                    .arg(color.lightness() < 128 ? "#f7f9fb" : "#101419"));
        }
    }

    void updateLightTabState() {
        int activeLights = std::clamp(active_lights_spin_->value(), 0, 3);
        for (int i = 0; i < 3; ++i) {
            if (light_cards_[i]) {
                light_cards_[i]->setVisible(i < activeLights);
            }
        }
    }

    void requestRender() {
        render_requested_ = true;
    }

    void renderFrame() {
        render_requested_ = false;
        readUiFromWidgets();
        applyUiToRenderer(app_);
        syncRenderBackground(app_);

        auto clearStart = std::chrono::high_resolution_clock::now();
        app_.renderer.clear(app_.render_image);
        auto clearEnd = std::chrono::high_resolution_clock::now();

        app_.renderer.set_view(app_.eye, app_.centre, app_.up);
        if (!app_.model.meshes.empty()) {
            app_.renderer.draw_completed(app_.render_image, app_.model);
        }
        app_.last_profile = app_.renderer.getLastProfile();
        app_.last_profile.clear_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(clearEnd - clearStart).count();

        if (!app_.render_image.empty()) {
            QImage frame(app_.render_image.data, app_.render_image.cols, app_.render_image.rows,
                         static_cast<int>(app_.render_image.step), QImage::Format_BGR888);
            viewport_->setFrame(frame.copy());
        }

        updateInspector();
        updateStatusChips();
    }

    void updateInspector() {
        const ModelStats stats = gatherModelStats(app_.model);
        model_path_label_->setText(app_.model_path.empty() ? tx("<none>", "<无>") : qPath(app_.model_path));
        environment_path_label_->setText(app_.environment_path.empty() ? tx("<sky fallback>", "<天空回退>") : qPath(app_.environment_path));
        mesh_count_label_->setText(QString::number(static_cast<qulonglong>(stats.mesh_count)));
        triangle_count_label_->setText(QString::number(static_cast<qulonglong>(stats.triangle_count)));
        vertex_count_label_->setText(QString::number(static_cast<qulonglong>(stats.vertex_count)));
        resolution_label_->setText(QString("%1 x %2").arg(app_.render_width).arg(app_.render_height));
        embree_label_->setText(embreeStatusText());
        camera_label_->setText(QString("target=(%1, %2, %3)  distance=%4")
                                   .arg(app_.camera.target[0], 0, 'f', 2)
                                   .arg(app_.camera.target[1], 0, 'f', 2)
                                   .arg(app_.camera.target[2], 0, 'f', 2)
                                   .arg(app_.camera.distance, 0, 'f', 2));
        profile_clear_label_->setText(formatMs(app_.last_profile.clear_ms));
        profile_shadow_near_label_->setText(formatMs(app_.last_profile.shadow_near_ms));
        profile_shadow_far_label_->setText(formatMs(app_.last_profile.shadow_far_ms));
        profile_vertex_label_->setText(formatMs(app_.last_profile.vertex_ms));
        profile_clip_bin_label_->setText(formatMs(app_.last_profile.clip_bin_ms));
        profile_raster_label_->setText(formatMs(app_.last_profile.raster_shade_ms));
        profile_total_label_->setText(formatMs(app_.last_profile.render_total_ms));
        last_event_label_->setText(QString::fromLocal8Bit(app_.status_message.c_str()));
    }

    void updateStatusChips() {
        look_chip_->setText(QString("%1  %2").arg(tx("Look", "风格")).arg(translatedLookName()));
        shadow_chip_->setText(QString("%1  %2").arg(tx("Shadow", "阴影")).arg(translatedShadowMode()));
        perf_chip_->setText(QString("%1  %2").arg(tx("Frame", "帧")).arg(formatMs(app_.last_profile.render_total_ms)));
        status_mode_->setText(QString("%1  |  %2")
                                  .arg(translatedLookName())
                                  .arg(translatedShadowMode()));
        status_perf_->setText(QString("clip %1  raster %2")
                                  .arg(formatMs(app_.last_profile.clip_bin_ms))
                                  .arg(formatMs(app_.last_profile.raster_shade_ms)));
        status_hint_->setText(QString::fromLocal8Bit(app_.status_message.c_str()));
    }

    void showStatus(const QString& message, bool isError) {
        app_.status_message = message.toLocal8Bit().constData();
        statusBar()->showMessage(message, 5000);
        if (status_hint_) {
            status_hint_->setText(message);
        }
        if (last_event_label_) {
            last_event_label_->setText(message);
        }
        if (isError) {
            QMessageBox::warning(this, tx("HaoRender Studio", "HaoRender 工作台"), message);
        }
    }

    QJsonObject captureStateObject() const {
        QJsonObject root;
        root["language"] = static_cast<int>(language_);
        root["theme"] = static_cast<int>(theme_);
        root["restoreLastSession"] = restore_last_session_;
        root["modelPath"] = qPath(app_.model_path);
        root["environmentPath"] = qPath(app_.environment_path);
        root["backgroundImagePath"] = qPath(app_.background_image_path);
        root["backgroundMode"] = app_.background_mode;
        root["programmableShaderSource"] = QString::fromUtf8(app_.programmable_shader_source.c_str());
        root["manualRenderResolution"] = app_.manual_render_resolution;
        root["renderResolutionPreset"] = app_.render_resolution_preset;
        root["renderWidth"] = app_.render_width;
        root["renderHeight"] = app_.render_height;
        root["dockTab"] = dock_tabs_ ? dock_tabs_->currentIndex() : preferred_dock_tab_;
        root["materialTab"] = materials_tabs_ ? materials_tabs_->currentIndex() : preferred_material_tab_;

        QJsonArray backgroundBgr;
        backgroundBgr.append(static_cast<int>(app_.background_color[0]));
        backgroundBgr.append(static_cast<int>(app_.background_color[1]));
        backgroundBgr.append(static_cast<int>(app_.background_color[2]));
        root["backgroundBgr"] = backgroundBgr;

        QJsonObject camera;
        camera["targetX"] = app_.camera.target[0];
        camera["targetY"] = app_.camera.target[1];
        camera["targetZ"] = app_.camera.target[2];
        camera["yaw"] = app_.camera.yaw;
        camera["pitch"] = app_.camera.pitch;
        camera["distance"] = app_.camera.distance;
        root["camera"] = camera;

        QJsonObject ui;
        ui["shadow_enabled"] = app_.ui.shadow_enabled;
        ui["shadow_technique"] = app_.ui.shadow_technique;
        ui["backface_culling"] = app_.ui.backface_culling;
        ui["shadow_cascade_enabled"] = app_.ui.shadow_cascade_enabled;
        ui["near_shadow_res_preset"] = app_.ui.near_shadow_res_preset;
        ui["far_shadow_res_preset"] = app_.ui.far_shadow_res_preset;
        ui["shadow_near_extent"] = app_.ui.shadow_near_extent;
        ui["shadow_near_depth"] = app_.ui.shadow_near_depth;
        ui["shadow_far_extent"] = app_.ui.shadow_far_extent;
        ui["shadow_far_depth"] = app_.ui.shadow_far_depth;
        ui["cascade_split"] = app_.ui.cascade_split;
        ui["cascade_blend"] = app_.ui.cascade_blend;
        ui["fov"] = app_.ui.fov;
        ui["exposure"] = app_.ui.exposure;
        ui["normal_strength"] = app_.ui.normal_strength;
        ui["shading_look"] = app_.ui.shading_look;
        ui["ibl_enabled"] = app_.ui.ibl_enabled;
        ui["ibl_diffuse"] = app_.ui.ibl_diffuse;
        ui["ibl_specular"] = app_.ui.ibl_specular;
        ui["sky_light"] = app_.ui.sky_light;
        ui["metallic_channel"] = app_.ui.metallic_channel;
        ui["roughness_channel"] = app_.ui.roughness_channel;
        ui["ao_channel"] = app_.ui.ao_channel;
        ui["emissive_channel"] = app_.ui.emissive_channel;
        ui["phong_hard_specular"] = app_.ui.phong_hard_specular;
        ui["phong_toon_diffuse"] = app_.ui.phong_toon_diffuse;
        ui["phong_use_tonemap"] = app_.ui.phong_use_tonemap;
        ui["phong_primary_light_only"] = app_.ui.phong_primary_light_only;
        ui["phong_secondary_scale"] = app_.ui.phong_secondary_scale;
        ui["phong_ambient"] = app_.ui.phong_ambient;
        ui["phong_specular"] = app_.ui.phong_specular;
        ui["active_lights"] = app_.ui.active_lights;

        QJsonArray lights;
        for (const auto& light : app_.ui.lights) {
            QJsonObject item;
            item["yaw"] = light.yaw;
            item["pitch"] = light.pitch;
            item["intensity"] = light.intensity;
            item["r"] = light.r;
            item["g"] = light.g;
            item["b"] = light.b;
            lights.append(item);
        }
        ui["lights"] = lights;
        root["ui"] = ui;
        return root;
    }

    void restoreStateObject(const QJsonObject& root, bool loadAssets) {
        auto readInt = [](const QJsonObject& object, const char* key, int fallback) {
            return object.contains(key) ? object.value(key).toInt(fallback) : fallback;
        };
        auto readDouble = [](const QJsonObject& object, const char* key, double fallback) {
            return object.contains(key) ? object.value(key).toDouble(fallback) : fallback;
        };
        auto readBool = [](const QJsonObject& object, const char* key, bool fallback) {
            return object.contains(key) ? object.value(key).toBool(fallback) : fallback;
        };
        auto readString = [](const QJsonObject& object, const char* key, const QString& fallback = QString()) {
            return object.contains(key) ? object.value(key).toString(fallback) : fallback;
        };

        language_ = readInt(root, "language", static_cast<int>(language_)) == 1 ? UiLanguage::Chinese : UiLanguage::English;
        theme_ = static_cast<UiTheme>(std::clamp(readInt(root, "theme", static_cast<int>(theme_)), 0, 2));
        restore_last_session_ = readBool(root, "restoreLastSession", restore_last_session_);
        app_.manual_render_resolution = readBool(root, "manualRenderResolution", app_.manual_render_resolution);
        app_.render_resolution_preset = std::clamp(readInt(root, "renderResolutionPreset", app_.render_resolution_preset), 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1);
        app_.render_width = std::max(320, readInt(root, "renderWidth", app_.render_width));
        app_.render_height = std::max(240, readInt(root, "renderHeight", app_.render_height));
        preferred_dock_tab_ = std::max(0, readInt(root, "dockTab", preferred_dock_tab_));
        preferred_material_tab_ = std::max(0, readInt(root, "materialTab", preferred_material_tab_));

        if (root.contains("backgroundBgr") && root.value("backgroundBgr").isArray()) {
            const QJsonArray backgroundBgr = root.value("backgroundBgr").toArray();
            if (backgroundBgr.size() >= 3) {
                app_.background_color = Scalar(
                    backgroundBgr.at(0).toInt(static_cast<int>(app_.background_color[0])),
                    backgroundBgr.at(1).toInt(static_cast<int>(app_.background_color[1])),
                    backgroundBgr.at(2).toInt(static_cast<int>(app_.background_color[2])));
            }
        }
        app_.background_mode = std::clamp(readInt(root, "backgroundMode", app_.background_mode), 0, 1);
        app_.background_image_path = nativePathString(readString(root, "backgroundImagePath", qPath(app_.background_image_path)));
        app_.programmable_shader_source = readString(root, "programmableShaderSource", QString::fromStdString(app_.programmable_shader_source)).toStdString();

        const QJsonObject ui = root.value("ui").toObject();
        app_.ui.shadow_enabled = readInt(ui, "shadow_enabled", app_.ui.shadow_enabled);
        app_.ui.shadow_technique = readInt(ui, "shadow_technique", app_.ui.shadow_technique);
        app_.ui.backface_culling = readInt(ui, "backface_culling", app_.ui.backface_culling);
        app_.ui.shadow_cascade_enabled = readInt(ui, "shadow_cascade_enabled", app_.ui.shadow_cascade_enabled);
        app_.ui.near_shadow_res_preset = readInt(ui, "near_shadow_res_preset", app_.ui.near_shadow_res_preset);
        app_.ui.far_shadow_res_preset = readInt(ui, "far_shadow_res_preset", app_.ui.far_shadow_res_preset);
        app_.ui.shadow_near_extent = readInt(ui, "shadow_near_extent", app_.ui.shadow_near_extent);
        app_.ui.shadow_near_depth = readInt(ui, "shadow_near_depth", app_.ui.shadow_near_depth);
        app_.ui.shadow_far_extent = readInt(ui, "shadow_far_extent", app_.ui.shadow_far_extent);
        app_.ui.shadow_far_depth = readInt(ui, "shadow_far_depth", app_.ui.shadow_far_depth);
        app_.ui.cascade_split = readInt(ui, "cascade_split", app_.ui.cascade_split);
        app_.ui.cascade_blend = readInt(ui, "cascade_blend", app_.ui.cascade_blend);
        app_.ui.fov = readInt(ui, "fov", app_.ui.fov);
        app_.ui.exposure = readInt(ui, "exposure", app_.ui.exposure);
        app_.ui.normal_strength = readInt(ui, "normal_strength", app_.ui.normal_strength);
        app_.ui.shading_look = readInt(ui, "shading_look", app_.ui.shading_look);
        app_.ui.ibl_enabled = readInt(ui, "ibl_enabled", app_.ui.ibl_enabled);
        app_.ui.ibl_diffuse = readInt(ui, "ibl_diffuse", app_.ui.ibl_diffuse);
        app_.ui.ibl_specular = readInt(ui, "ibl_specular", app_.ui.ibl_specular);
        app_.ui.sky_light = readInt(ui, "sky_light", app_.ui.sky_light);
        app_.ui.metallic_channel = readInt(ui, "metallic_channel", app_.ui.metallic_channel);
        app_.ui.roughness_channel = readInt(ui, "roughness_channel", app_.ui.roughness_channel);
        app_.ui.ao_channel = readInt(ui, "ao_channel", app_.ui.ao_channel);
        app_.ui.emissive_channel = readInt(ui, "emissive_channel", app_.ui.emissive_channel);
        app_.ui.phong_hard_specular = readInt(ui, "phong_hard_specular", app_.ui.phong_hard_specular);
        app_.ui.phong_toon_diffuse = readInt(ui, "phong_toon_diffuse", app_.ui.phong_toon_diffuse);
        app_.ui.phong_use_tonemap = readInt(ui, "phong_use_tonemap", app_.ui.phong_use_tonemap);
        app_.ui.phong_primary_light_only = readInt(ui, "phong_primary_light_only", app_.ui.phong_primary_light_only);
        app_.ui.phong_secondary_scale = readInt(ui, "phong_secondary_scale", app_.ui.phong_secondary_scale);
        app_.ui.phong_ambient = readInt(ui, "phong_ambient", app_.ui.phong_ambient);
        app_.ui.phong_specular = readInt(ui, "phong_specular", app_.ui.phong_specular);
        app_.ui.active_lights = readInt(ui, "active_lights", app_.ui.active_lights);

        if (ui.contains("lights") && ui.value("lights").isArray()) {
            const QJsonArray lights = ui.value("lights").toArray();
            for (int i = 0; i < std::min<int>(3, lights.size()); ++i) {
                const QJsonObject item = lights.at(i).toObject();
                app_.ui.lights[i].yaw = readInt(item, "yaw", app_.ui.lights[i].yaw);
                app_.ui.lights[i].pitch = readInt(item, "pitch", app_.ui.lights[i].pitch);
                app_.ui.lights[i].intensity = readInt(item, "intensity", app_.ui.lights[i].intensity);
                app_.ui.lights[i].r = readInt(item, "r", app_.ui.lights[i].r);
                app_.ui.lights[i].g = readInt(item, "g", app_.ui.lights[i].g);
                app_.ui.lights[i].b = readInt(item, "b", app_.ui.lights[i].b);
            }
        }
        session_recovered_ = sanitizeUiState(app_.ui) || session_recovered_;

        const QJsonObject camera = root.value("camera").toObject();
        const bool hasCamera = !camera.isEmpty();
        const Vector3f cameraTarget(
            static_cast<float>(readDouble(camera, "targetX", app_.camera.target[0])),
            static_cast<float>(readDouble(camera, "targetY", app_.camera.target[1])),
            static_cast<float>(readDouble(camera, "targetZ", app_.camera.target[2])));
        const float cameraYaw = static_cast<float>(readDouble(camera, "yaw", app_.camera.yaw));
        const float cameraPitch = clampPitch(static_cast<float>(readDouble(camera, "pitch", app_.camera.pitch)));
        const float cameraDistance = std::max(0.3f, static_cast<float>(readDouble(camera, "distance", app_.camera.distance)));

        const string modelPath = nativePathString(readString(root, "modelPath", qPath(app_.model_path)));
        const string environmentPath = nativePathString(readString(root, "environmentPath", qPath(app_.environment_path)));

        if (loadAssets) {
            if (!modelPath.empty() && std::filesystem::exists(modelPath)) {
                string errorMessage;
                if (!loadModelFromPath(app_, modelPath, &errorMessage)) {
                    showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
                }
            }
            else if (!modelPath.empty()) {
                showStatus(tx("Preset model file is missing", "预设中的模型文件不存在"), true);
            }

            if (!environmentPath.empty() && std::filesystem::exists(environmentPath)) {
                string errorMessage;
                if (!loadEnvironmentFromPath(app_, environmentPath, &errorMessage)) {
                    showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
                }
            }
            else if (root.contains("environmentPath") && environmentPath.empty()) {
                clearEnvironment(app_);
            }
        }
        else {
            app_.model_path = modelPath;
            app_.environment_path = environmentPath;
        }

        if (hasCamera) {
            app_.camera.target = cameraTarget;
            app_.camera.yaw = cameraYaw;
            app_.camera.pitch = cameraPitch;
            app_.camera.distance = cameraDistance;
            updateCameraPose(app_);
        }

        syncRenderBackground(app_);
        if (app_.manual_render_resolution) {
            const int presetIndex = std::clamp(app_.render_resolution_preset, 0, static_cast<int>(std::size(kRenderResolutionPresets)) - 1);
            resizeRenderBuffers(app_, kRenderResolutionPresets[presetIndex].width, kRenderResolutionPresets[presetIndex].height);
        }
        else {
            resizeRenderBuffers(app_, app_.render_width, app_.render_height);
        }
        app_.ui_applied = false;
    }

    void saveSession() {
        QSettings settings("HaoRender", "HaoRenderStudio");
        QJsonObject preferences;
        preferences["language"] = static_cast<int>(language_);
        preferences["theme"] = static_cast<int>(theme_);
        preferences["restoreLastSession"] = restore_last_session_;
        preferences["dockTab"] = dock_tabs_ ? dock_tabs_->currentIndex() : preferred_dock_tab_;
        preferences["materialTab"] = materials_tabs_ ? materials_tabs_->currentIndex() : preferred_material_tab_;
        settings.setValue("preferences", QString::fromUtf8(QJsonDocument(preferences).toJson(QJsonDocument::Compact)));
        settings.setValue("lastSession", QString::fromUtf8(QJsonDocument(captureStateObject()).toJson(QJsonDocument::Compact)));
        settings.setValue("geometry", saveGeometry());
    }

    void loadSession() {
        QSettings settings("HaoRender", "HaoRenderStudio");
        pending_geometry_ = settings.value("geometry").toByteArray();

        auto parseStoredObject = [](const QString& json) {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                return QJsonObject();
            }
            return document.object();
        };

        const QJsonObject preferences = parseStoredObject(settings.value("preferences").toString());
        if (!preferences.isEmpty()) {
            language_ = preferences.value("language").toInt(static_cast<int>(language_)) == 1 ? UiLanguage::Chinese : UiLanguage::English;
            theme_ = static_cast<UiTheme>(std::clamp(preferences.value("theme").toInt(static_cast<int>(theme_)), 0, 2));
            restore_last_session_ = preferences.value("restoreLastSession").toBool(restore_last_session_);
            preferred_dock_tab_ = std::max(0, preferences.value("dockTab").toInt(preferred_dock_tab_));
            preferred_material_tab_ = std::max(0, preferences.value("materialTab").toInt(preferred_material_tab_));
        }

        if (restore_last_session_) {
            const QJsonObject lastSession = parseStoredObject(settings.value("lastSession").toString());
            if (!lastSession.isEmpty()) {
                restoreStateObject(lastSession, false);
            }
        }
    }

    void rebuildUi() {
        preferred_dock_tab_ = dock_tabs_ ? dock_tabs_->currentIndex() : preferred_dock_tab_;
        preferred_material_tab_ = materials_tabs_ ? materials_tabs_->currentIndex() : preferred_material_tab_;
        pending_geometry_ = saveGeometry();

        applyThemeToApplication();

        if (QWidget* oldCentral = takeCentralWidget()) {
            oldCentral->deleteLater();
        }
        for (QToolBar* toolbar : findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly)) {
            removeToolBar(toolbar);
            toolbar->deleteLater();
        }
        for (QDockWidget* dock : findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            removeDockWidget(dock);
            dock->deleteLater();
        }
        setStatusBar(new QStatusBar(this));

        buildUi();
        connectControls();
        syncWidgetsFromUi();
        compileProgrammableShader(false);
        updateLightSwatches();
        updateLightTabState();
        updateMaterialsTabs();
        updateInspector();
        updateStatusChips();
        setWindowTitle(tx("HaoRender Studio", "HaoRender 工作台"));
        if (!pending_geometry_.isEmpty()) {
            restoreGeometry(pending_geometry_);
        }
        requestRender();
    }

    void updateBackgroundPreview() {
        if (background_color_preview_) {
            const QColor color = currentBackgroundColorQ();
            const QString textColor = color.lightness() < 128 ? "#f8fbfc" : "#10161a";
            background_color_preview_->setText(QString("%1  (%2, %3, %4)")
                                                   .arg(color.name().toUpper())
                                                   .arg(color.red())
                                                   .arg(color.green())
                                                   .arg(color.blue()));
            background_color_preview_->setStyleSheet(
                QString("background: rgb(%1, %2, %3); color:%4; border:1px solid %5; border-radius:8px; padding:6px 10px;")
                    .arg(color.red())
                    .arg(color.green())
                    .arg(color.blue())
                    .arg(textColor, paletteColors().border));
        }
        if (background_image_label_) {
            background_image_label_->setText(
                app_.background_image_path.empty() ? tx("<none>", "<无>")
                                                   : qPath(app_.background_image_path));
        }
        if (background_image_clear_button_) {
            background_image_clear_button_->setEnabled(!app_.background_image_path.empty());
        }
    }

    void pickBackgroundColor() {
        const QColor color = QColorDialog::getColor(currentBackgroundColorQ(), this, tx("Background Color", "背景颜色"));
        if (!color.isValid()) {
            return;
        }
        app_.background_color = Scalar(color.blue(), color.green(), color.red());
        app_.background_mode = 0;
        syncRenderBackground(app_);
        syncWidgetsFromUi();
        showStatus(tx("Background color updated", "背景颜色已更新"), false);
        saveSession();
        requestRender();
    }

    void pickBackgroundImage() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tx("Open Background Image", "打开背景图片"),
            qPath(app_.background_image_path),
            tx("Images (*.png *.jpg *.jpeg *.bmp *.tga);;All Files (*.*)",
               "图片 (*.png *.jpg *.jpeg *.bmp *.tga);;所有文件 (*.*)"));
        if (path.isEmpty()) {
            return;
        }
        Mat image = imread(nativePathString(path), IMREAD_UNCHANGED);
        if (image.empty()) {
            showStatus(tx("Failed to load background image", "背景图片加载失败"), true);
            return;
        }
        app_.background_image_path = nativePathString(path);
        app_.background_mode = 1;
        syncRenderBackground(app_);
        syncWidgetsFromUi();
        showStatus(tx("Background image updated", "背景图片已更新"), false);
        saveSession();
        requestRender();
    }

    void savePresetDialog() {
        std::filesystem::create_directories("Presets");
        const QString path = QFileDialog::getSaveFileName(
            this,
            tx("Save Preset", "保存预设"),
            qPath((std::filesystem::path("Presets") / "haorender_preset.json").string()),
            tx("HaoRender Preset (*.json)", "HaoRender 预设 (*.json)"));
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            showStatus(tx("Failed to save preset", "预设保存失败"), true);
            return;
        }
        file.write(QJsonDocument(captureStateObject()).toJson(QJsonDocument::Indented));
        file.close();
        showStatus(tx("Preset saved", "预设已保存"), false);
        saveSession();
    }

    void loadPresetDialog() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tx("Load Preset", "读取预设"),
            qPath("Presets"),
            tx("HaoRender Preset (*.json);;All Files (*.*)", "HaoRender 预设 (*.json);;所有文件 (*.*)"));
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            showStatus(tx("Failed to open preset", "预设打开失败"), true);
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            showStatus(tx("Preset file is invalid", "预设文件无效"), true);
            return;
        }

        restoreStateObject(document.object(), true);
        syncWidgetsFromUi();
        updateLightSwatches();
        updateLightTabState();
        updateMaterialsTabs();
        updateInspector();
        updateStatusChips();
        showStatus(tx("Preset loaded", "预设已读取"), false);
        saveSession();
        requestRender();
    }

    void updateMaterialsTabs() {
        if (!materials_tabs_) {
            return;
        }

        while (materials_tabs_->count() > 0) {
            QWidget* page = materials_tabs_->widget(0);
            materials_tabs_->removeTab(0);
            if (page) {
                page->deleteLater();
            }
        }

        if (app_.model.meshes.empty()) {
            auto* emptyPage = new QWidget(materials_tabs_);
            auto* emptyLayout = new QVBoxLayout(emptyPage);
            auto* label = new QLabel(tx("Load a model to inspect material channels and texture bindings.",
                                        "先加载模型，材质页签才会显示贴图绑定和通道信息。"), emptyPage);
            label->setWordWrap(true);
            label->setStyleSheet(QString("color:%1;").arg(paletteColors().muted));
            emptyLayout->addWidget(label);
            emptyLayout->addStretch();
            materials_tabs_->addTab(emptyPage, tx("Empty", "空"));
            preferred_material_tab_ = 0;
            return;
        }

        const auto makeValueLabel = [this](const QString& text, QWidget* parent) {
            auto* label = new QLabel(text, parent);
            label->setWordWrap(true);
            label->setStyleSheet(QString("color:%1;").arg(paletteColors().text));
            return label;
        };

        for (size_t meshIndex = 0; meshIndex < app_.model.meshes.size(); ++meshIndex) {
            const Mesh& mesh = app_.model.meshes[meshIndex];
            auto* page = new QWidget(materials_tabs_);
            auto* layout = new QVBoxLayout(page);
            layout->setContentsMargins(12, 12, 12, 16);
            layout->setSpacing(12);

            auto* summarySection = makeSection(tx("Mesh Summary", "网格摘要"));
            auto* summaryForm = new QFormLayout(summarySection);
            summaryForm->addRow(tx("Vertices", "顶点"), makeValueLabel(QString::number(static_cast<qulonglong>(mesh.vertexCount())), summarySection));
            summaryForm->addRow(tx("Triangles", "三角形"), makeValueLabel(QString::number(static_cast<qulonglong>(mesh.indices.size() / 3)), summarySection));
            summaryForm->addRow(tx("Texture Count", "纹理数"), makeValueLabel(QString::number(static_cast<qulonglong>(mesh.textures.size())), summarySection));
            summaryForm->addRow(tx("UV", "UV"), makeValueLabel(mesh.vertexCount() > 0 && mesh.hasTextureCoords(0) ? tx("Present", "存在") : tx("Missing", "缺失"), summarySection));

            auto* pbrSection = makeSection(tx("PBR Channel Mapping", "PBR 通道映射"));
            auto* pbrForm = new QFormLayout(pbrSection);
            pbrForm->addRow(tx("Metallic", "金属度"), makeValueLabel(channelLabel(mesh.pbr_channel_map.metallic_channel), pbrSection));
            pbrForm->addRow(tx("Roughness", "粗糙度"), makeValueLabel(channelLabel(mesh.pbr_channel_map.roughness_channel), pbrSection));
            pbrForm->addRow(tx("AO", "AO"), makeValueLabel(channelLabel(mesh.pbr_channel_map.ao_channel), pbrSection));
            pbrForm->addRow(tx("Emissive", "自发光"), makeValueLabel(channelLabel(mesh.pbr_channel_map.emissive_channel), pbrSection));

            auto* texturesSection = makeSection(tx("Texture Bindings", "纹理绑定"));
            auto* texturesLayout = new QVBoxLayout(texturesSection);
            if (mesh.textures.empty()) {
                texturesLayout->addWidget(makeValueLabel(tx("No textures recorded on this mesh.", "这个网格没有记录到贴图。"), texturesSection));
            }
            else {
                for (const auto& texture : mesh.textures) {
                    auto* card = new QFrame(texturesSection);
                    card->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:8px; }")
                                            .arg(paletteColors().elevated, paletteColors().border));
                    auto* cardLayout = new QVBoxLayout(card);
                    cardLayout->setContentsMargins(10, 8, 10, 8);
                    cardLayout->setSpacing(4);
                    auto* typeLabel = makeValueLabel(QString::fromLocal8Bit(texture.type.c_str()), card);
                    typeLabel->setStyleSheet(QString("font-weight:600; color:%1;").arg(paletteColors().text));
                    auto* pathLabel = makeValueLabel(texture.path.empty() ? tx("<embedded or unresolved>", "<嵌入式或未解析>")
                                                                        : QString::fromLocal8Bit(texture.path.c_str()), card);
                    pathLabel->setStyleSheet(QString("color:%1;").arg(paletteColors().muted));
                    cardLayout->addWidget(typeLabel);
                    cardLayout->addWidget(pathLabel);
                    texturesLayout->addWidget(card);
                }
            }

            layout->addWidget(summarySection);
            layout->addWidget(pbrSection);
            layout->addWidget(texturesSection);
            layout->addStretch();
            materials_tabs_->addTab(page, tx("Material ", "材质 ") + QString::number(static_cast<int>(meshIndex) + 1));
        }

        materials_tabs_->setCurrentIndex(std::clamp(preferred_material_tab_, 0, materials_tabs_->count() - 1));
    }

    void closeEvent(QCloseEvent* event) override {
        saveSession();
        QMainWindow::closeEvent(event);
    }

    void openModelDialog() {
        QString path = QFileDialog::getOpenFileName(
            this,
            tx("Open Model", "导入模型"),
            qPath(app_.model_path.empty() ? findDefaultModelPath() : app_.model_path),
            tx("Model Files (*.obj *.fbx *.gltf *.glb *.pmx *.dae *.3ds *.stl *.ply *.x);;All Files (*.*)",
               "模型文件 (*.obj *.fbx *.gltf *.glb *.pmx *.dae *.3ds *.stl *.ply *.x);;所有文件 (*.*)"));
        if (path.isEmpty()) {
            return;
        }
        string errorMessage;
        if (!loadModelFromPath(app_, nativePathString(path), &errorMessage)) {
            showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
            return;
        }
        updateMaterialsTabs();
        updateInspector();
        updateStatusChips();
        showStatus(QString::fromLocal8Bit(app_.status_message.c_str()), false);
        saveSession();
        requestRender();
    }

    void openEnvironmentDialog() {
        QString path = QFileDialog::getOpenFileName(
            this,
            tx("Open Environment", "导入环境图"),
            qPath(app_.environment_path.empty() ? findDefaultEnvironmentMap() : app_.environment_path),
            tx("Environment Files (*.hdr *.png *.jpg *.jpeg *.bmp *.tga);;All Files (*.*)",
               "环境图文件 (*.hdr *.png *.jpg *.jpeg *.bmp *.tga);;所有文件 (*.*)"));
        if (path.isEmpty()) {
            return;
        }
        string errorMessage;
        if (!loadEnvironmentFromPath(app_, nativePathString(path), &errorMessage)) {
            showStatus(QString::fromLocal8Bit(errorMessage.c_str()), true);
            return;
        }
        updateInspector();
        updateStatusChips();
        showStatus(QString::fromLocal8Bit(app_.status_message.c_str()), false);
        saveSession();
        requestRender();
    }

    void saveScreenshot() {
        if (app_.render_image.empty()) {
            showStatus("Nothing to save yet.", true);
            return;
        }
        std::filesystem::create_directories("Screenshots");
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        std::filesystem::path path = std::filesystem::path("Screenshots") / QString("haorender_%1.png").arg(timestamp).toStdString();
        if (!imwrite(path.string(), app_.render_image)) {
            showStatus("Failed to save snapshot.", true);
            return;
        }
        QString message = QString("Snapshot saved to %1").arg(qPath(path.string()));
        setStatus(app_, nativePathString(message));
        updateInspector();
        updateStatusChips();
        showStatus(message, false);
    }

private:
    AppState app_;
    RenderViewport* viewport_ = nullptr;
    QTimer* render_timer_ = nullptr;
    bool render_requested_ = false;

    QLabel* look_chip_ = nullptr;
    QLabel* shadow_chip_ = nullptr;
    QLabel* perf_chip_ = nullptr;
    QLabel* status_hint_ = nullptr;
    QLabel* status_mode_ = nullptr;
    QLabel* status_perf_ = nullptr;

    IntSliderField* fov_spin_ = nullptr;
    DoubleSliderField* exposure_spin_ = nullptr;
    DoubleSliderField* normal_strength_spin_ = nullptr;
    QCheckBox* manual_resolution_check_ = nullptr;
    QComboBox* render_resolution_combo_ = nullptr;
    QCheckBox* backface_culling_check_ = nullptr;
    QCheckBox* shadow_enabled_check_ = nullptr;
    QComboBox* shadow_technique_combo_ = nullptr;
    QCheckBox* shadow_cascade_check_ = nullptr;
    QComboBox* near_shadow_res_combo_ = nullptr;
    QComboBox* far_shadow_res_combo_ = nullptr;
    DoubleSliderField* shadow_near_extent_spin_ = nullptr;
    DoubleSliderField* shadow_near_depth_spin_ = nullptr;
    DoubleSliderField* shadow_far_extent_spin_ = nullptr;
    DoubleSliderField* shadow_far_depth_spin_ = nullptr;
    DoubleSliderField* cascade_split_spin_ = nullptr;
    DoubleSliderField* cascade_blend_spin_ = nullptr;

    QComboBox* shading_look_combo_ = nullptr;
    QCheckBox* ibl_enabled_check_ = nullptr;
    DoubleSliderField* ibl_diffuse_spin_ = nullptr;
    DoubleSliderField* ibl_specular_spin_ = nullptr;
    DoubleSliderField* sky_light_spin_ = nullptr;
    QComboBox* metallic_channel_combo_ = nullptr;
    QComboBox* roughness_channel_combo_ = nullptr;
    QComboBox* ao_channel_combo_ = nullptr;
    QComboBox* emissive_channel_combo_ = nullptr;
    QCheckBox* phong_hard_spec_check_ = nullptr;
    QCheckBox* phong_toon_diffuse_check_ = nullptr;
    QCheckBox* phong_tonemap_check_ = nullptr;
    QCheckBox* phong_primary_only_check_ = nullptr;
    DoubleSliderField* phong_secondary_spin_ = nullptr;
    DoubleSliderField* phong_ambient_spin_ = nullptr;
    DoubleSliderField* phong_specular_spin_ = nullptr;
    QStackedWidget* shading_mode_stack_ = nullptr;
    CollapsibleGroupBox* pbr_lighting_section_ = nullptr;
    CollapsibleGroupBox* pbr_section_ = nullptr;
    CollapsibleGroupBox* phong_section_ = nullptr;
    CollapsibleGroupBox* programmable_section_ = nullptr;
    QComboBox* programmable_shader_preset_combo_ = nullptr;
    QPlainTextEdit* programmable_shader_guide_ = nullptr;
    QPlainTextEdit* programmable_shader_editor_ = nullptr;
    QPushButton* programmable_shader_apply_preset_button_ = nullptr;
    QPushButton* programmable_shader_compile_button_ = nullptr;
    QPushButton* programmable_shader_template_button_ = nullptr;
    QLabel* programmable_shader_status_label_ = nullptr;

    QSpinBox* active_lights_spin_ = nullptr;
    array<QWidget*, 3> light_cards_ {};
    array<IntSliderField*, 3> light_yaw_spin_ {};
    array<IntSliderField*, 3> light_pitch_spin_ {};
    array<DoubleSliderField*, 3> light_intensity_spin_ {};
    array<QSpinBox*, 3> light_r_spin_ {};
    array<QSpinBox*, 3> light_g_spin_ {};
    array<QSpinBox*, 3> light_b_spin_ {};
    array<QPushButton*, 3> light_color_button_ {};

    QLabel* model_path_label_ = nullptr;
    QLabel* environment_path_label_ = nullptr;
    QLabel* mesh_count_label_ = nullptr;
    QLabel* triangle_count_label_ = nullptr;
    QLabel* vertex_count_label_ = nullptr;
    QLabel* resolution_label_ = nullptr;
    QLabel* embree_label_ = nullptr;
    QLabel* camera_label_ = nullptr;
    QLabel* profile_clear_label_ = nullptr;
    QLabel* profile_shadow_near_label_ = nullptr;
    QLabel* profile_shadow_far_label_ = nullptr;
    QLabel* profile_vertex_label_ = nullptr;
    QLabel* profile_clip_bin_label_ = nullptr;
    QLabel* profile_raster_label_ = nullptr;
    QLabel* profile_total_label_ = nullptr;
    QLabel* last_event_label_ = nullptr;
    QLabel* background_mode_label_ = nullptr;
    QLabel* background_color_preview_ = nullptr;
    QLabel* background_image_label_ = nullptr;
    QLabel* ui_theme_label_ = nullptr;
    QLabel* language_label_ = nullptr;
    QLabel* restore_session_label_ = nullptr;
    QComboBox* background_mode_combo_ = nullptr;
    QComboBox* ui_theme_combo_ = nullptr;
    QComboBox* language_combo_ = nullptr;
    QCheckBox* restore_session_check_ = nullptr;
    QPushButton* background_color_button_ = nullptr;
    QPushButton* background_image_button_ = nullptr;
    QPushButton* background_image_clear_button_ = nullptr;
    QPushButton* save_preset_button_ = nullptr;
    QPushButton* load_preset_button_ = nullptr;
    QTabWidget* materials_tabs_ = nullptr;
    QTabWidget* dock_tabs_ = nullptr;
    QAction* open_model_action_ = nullptr;
    QAction* open_env_action_ = nullptr;
    QAction* clear_env_action_ = nullptr;
    QAction* reset_camera_action_ = nullptr;
    QAction* save_snapshot_action_ = nullptr;
    QLabel* header_title_label_ = nullptr;
    QLabel* header_subtitle_label_ = nullptr;
    QLabel* toolbar_hint_label_ = nullptr;
    UiLanguage language_ = UiLanguage::Chinese;
    UiTheme theme_ = UiTheme::Ocean;
    bool restore_last_session_ = true;
    int preferred_dock_tab_ = 0;
    int preferred_material_tab_ = 0;
    QByteArray pending_geometry_;
    bool syncing_widgets_ = false;
    bool session_recovered_ = false;
};
}

int main(int argc, char** argv) {
    enableFastFpModes();
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    applyApplicationTheme(app, UiTheme::Ocean);
    app.setFont(QFont("Segoe UI", 10));

    string startupModelPath = findDefaultModelPath();
    string startupEnvironmentPath = findDefaultEnvironmentMap();
    int startupShadowTechnique = 0;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        static const string shadowTechniquePrefix = "--shadow-technique=";
        if (arg.rfind(shadowTechniquePrefix, 0) == 0) {
            string mode = arg.substr(shadowTechniquePrefix.size());
            startupShadowTechnique = (mode == "embree" || mode == "raster-embree") ? 1 : 0;
        }
        else if (!arg.empty() && arg[0] != '-') {
            startupModelPath = arg;
        }
    }

    HaoRenderWindow window(startupModelPath, startupEnvironmentPath, startupShadowTechnique);
    window.show();
    return app.exec();
}
