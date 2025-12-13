#ifndef PROYECTOFINAL_CGA_DEBUGSETTINGS_H
#define PROYECTOFINAL_CGA_DEBUGSETTINGS_H

#include <nlohmann/json.hpp>

struct DebugSettings
{
    float pathVelocity = 28.0f;
    float cameraMoveSpeed = 5.0f;
    float cameraTurnSpeed = 75.0f;
    bool enablePixelate = true;
    int pixelateResolution = 520;
    bool enableVsync = true;
    bool showHitboxes = false;
};

inline void from_json(const nlohmann::json &j, DebugSettings &settings)
{
    // if (j.contains("path_velocity")) j.at("path_velocity").get_to(settings.pathVelocity);
    if (j.contains("camera_move_speed")) j.at("camera_move_speed").get_to(settings.cameraMoveSpeed);
    if (j.contains("camera_turn_speed")) j.at("camera_turn_speed").get_to(settings.cameraTurnSpeed);
    if (j.contains("enable_pixelate")) j.at("enable_pixelate").get_to(settings.enablePixelate);
    if (j.contains("pixelate_resolution")) j.at("pixelate_resolution").get_to(settings.pixelateResolution);
    if (j.contains("enable_vsync")) j.at("enable_vsync").get_to(settings.enableVsync);
    if (j.contains("show_hitboxes")) j.at("show_hitboxes").get_to(settings.showHitboxes);
}

inline void to_json(nlohmann::json &j, const DebugSettings &settings)
{
    j = nlohmann::json{
        // {"path_velocity", settings.pathVelocity},
        {"camera_move_speed", settings.cameraMoveSpeed},
        {"camera_turn_speed", settings.cameraTurnSpeed},
        {"enable_pixelate", settings.enablePixelate},
        {"pixelate_resolution", settings.pixelateResolution},
        {"enable_vsync", settings.enableVsync},
        {"show_hitboxes", settings.showHitboxes},
    };
}

#endif // PROYECTOFINAL_CGA_DEBUGSETTINGS_H
