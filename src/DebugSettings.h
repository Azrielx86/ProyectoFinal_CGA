#ifndef PROYECTOFINAL_CGA_DEBUGSETTINGS_H
#define PROYECTOFINAL_CGA_DEBUGSETTINGS_H

#include <nlohmann/json.hpp>

struct DebugSettings
{
    float pathVelocity = 0.01f;
    float cameraMoveSpeed = 150.0f;
    float cameraTurnSpeed = 75.0f;
    bool enablePixelate = true;
    int pixelateResolution = 320;
};

inline void from_json(const nlohmann::json &j, DebugSettings &settings)
{
    if (j.contains("path_velocity")) j.at("path_velocity").get_to(settings.pathVelocity);
    if (j.contains("camera_move_speed")) j.at("camera_move_speed").get_to(settings.cameraMoveSpeed);
    if (j.contains("camera_turn_speed")) j.at("camera_turn_speed").get_to(settings.cameraTurnSpeed);
    if (j.contains("enable_pixelate")) j.at("enable_pixelate").get_to(settings.enablePixelate);
    if (j.contains("pixelate_resolution")) j.at("pixelate_resolution").get_to(settings.pixelateResolution);
}

inline void to_json(nlohmann::json &j, const DebugSettings &settings)
{
    j = nlohmann::json{
        {"path_velocity", settings.pathVelocity},
        {"camera_move_speed", settings.cameraMoveSpeed},
        {"camera_turn_speed", settings.cameraTurnSpeed},
        {"enable_pixelate", settings.enablePixelate},
        {"pixelate_resolution", settings.pixelateResolution},
    };
}

#endif // PROYECTOFINAL_CGA_DEBUGSETTINGS_H
