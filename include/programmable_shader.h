#pragma once

#include <memory>
#include <string>

class ProgrammableShaderProgram {
public:
    struct Inputs {
        float pi = 3.14159265f;
        float uv_u = 0.0f;
        float uv_v = 0.0f;
        float base_r = 1.0f;
        float base_g = 1.0f;
        float base_b = 1.0f;
        float base_luma = 1.0f;
        float n_x = 0.0f;
        float n_y = 0.0f;
        float n_z = 1.0f;
        float v_x = 0.0f;
        float v_y = 0.0f;
        float v_z = 1.0f;
        float l_x = 0.0f;
        float l_y = 0.0f;
        float l_z = 1.0f;
        float light_r = 1.0f;
        float light_g = 1.0f;
        float light_b = 1.0f;
        float ndotl = 1.0f;
        float ndotv = 1.0f;
        float half_lambert = 1.0f;
        float rim = 0.0f;
        float shadow = 1.0f;
        float metallic = 0.0f;
        float roughness = 1.0f;
        float ao = 1.0f;
        float emissive_r = 0.0f;
        float emissive_g = 0.0f;
        float emissive_b = 0.0f;
        float ambient = 0.03f;
        float specular = 0.12f;
        float exposure = 1.0f;
    };

    static std::shared_ptr<ProgrammableShaderProgram> compile(const std::string& source, std::string& error_message);

    bool evaluate(const Inputs& inputs, float& out_r, float& out_g, float& out_b) const;
    const std::string& source() const { return source_; }

    static std::string defaultSource();
    static std::string guideText();

private:
    struct Impl;

    explicit ProgrammableShaderProgram(std::string source);

    std::string source_;
    std::unique_ptr<Impl> impl_;
};
