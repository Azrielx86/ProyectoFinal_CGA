#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
// region Global Include (Must be the first!)
#define STB_IMAGE_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include "GlobalDefines.h"
// endregion Global Include

#include "Camera.h"
#include "DebugSettings.h"
#include "ECS/Components/Collider.h"
#include "ECS/Components/MeshRenderer.h"
#include "ECS/Components/PlayerController.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SystemManager.h"
#include "ECS/Systems/CollisionSystem.h"
#include "ECS/Systems/PlayerControlSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "Input/Keyboard.h"
#include "Lights/PointLight.h"
#include "Model.h"
#include "Primitives/AbstractPrimitive.h"
#include "Primitives/Cube.h"
#include "Primitives/Plane.h"
#include "Resources/ResourceManager.h"
#include "Shader.h"
#include "SkinnedAnimation.h"
#include "Skybox.h"
#include "StorageBufferDynamicArray.h"
#include "Window.h"
#include "imgui.h"
#include <AL/alut.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <fstream>
#include <iostream>

#define RGBCOLOR(r, g, b) glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f)

struct SingleKeyPress
{
    bool clicked = true;
    bool event = true;
};

SingleKeyPress debugMode;

const std::string debugSettingsPath = "./debug_settings.json";

float deltaTime, lastTime;
float red = 0.0f;
bool showDebugGui = true;
bool enableCursorEvent = true;
bool fullscreenEvent;
bool enableCursor = true;
bool enableGrid = true;
bool enableSkybox = false;
bool enablePixelate = true;
bool polygonMode = false;
int pixelFbResolution = 320;
int lastPixelFbResolution = 320;

float fpsCounter = 0;
float fpsCount = 0;
float fps = 0;

// region Models Section
Model oxxoStore("./assets/models/OxxoStore/OxxoStore.obj");
Model pathChunk01("./assets/models/Path/Path.obj");
// endregion Models Section

Input::Keyboard &keyboard = *Input::Keyboard::GetInstance();
Input::Mouse &mouse = *Input::Mouse::GetInstance();
Resources::ResourceManager &resources = *Resources::ResourceManager::GetInstance();

GLuint particleVAO;

struct Uniforms
{
    GLint model = 0;
};

Uniforms uniforms{};

Primitives::Plane plane;
Primitives::Cube cube;

ECS::Registry registry;
ECS::SystemManager systemManager;

DebugSettings debugSettings;

// region Game Variables
std::deque<ECS::Entity> pathEntities;
float pathVelocity = 0.01f;

// endregion Game Variables

void ConfigureKeys(Window &window)
{
    keyboard
        .AddCallback(GLFW_KEY_ESCAPE, [&window]() -> void
                     {
                         window.SetShouldClose(true);
                     })
        .AddCallback(GLFW_KEY_T, [&window]() -> void
                     {
                         if (enableCursorEvent) return;
                         enableCursorEvent = true;
                         enableCursor = !enableCursor;
                         mouse.ToggleMouse(enableCursor);
                         window.SetMouseStatus(enableCursor);
                     })
        .AddCallback(GLFW_KEY_F11, [&window]() -> void
                     {
                         if (fullscreenEvent) return;
                         fullscreenEvent = true;
                         window.ToggleFullscreen();
                     })
        .AddCallback(GLFW_KEY_F3, []() -> void
                     {
                         if (debugMode.event) return;
                         debugMode.event = true;
                         showDebugGui = !showDebugGui;
                     });
}

void LoadSettings()
{
    if (std::ifstream debugSettingsStream(debugSettingsPath); debugSettingsStream.is_open())
        debugSettings = nlohmann::json::parse(debugSettingsStream);
}

void SaveSettings()
{
    if (std::ofstream debugSettingsStream(debugSettingsPath); debugSettingsStream.is_open())
    {
        const nlohmann::json data = debugSettings;
        debugSettingsStream << data.dump();
    }
}

int main(int argc, char **argv)
{
    Window window(1280, 720, "Proyecto Final CGA");

    alutInit(&argc, argv);

    if (!window.Init())
    {
        std::cerr << "Cannot initialize window.\n";
        return 1;
    }

    LoadSettings();

    registry.RegisterComponent<ECS::Components::Transform>();
    registry.RegisterComponent<ECS::Components::MeshRenderer>();
    registry.RegisterComponent<ECS::Components::PlayerController>();
    registry.RegisterComponent<ECS::Components::AABBCollider>();
    registry.RegisterComponent<ECS::Components::SBBCollider>();
    registry.RegisterComponent<ECS::Components::OBBCollider>();
    systemManager.RegisterSystem<ECS::Systems::CollisionSystem>();
    systemManager.RegisterSystem<ECS::Systems::RenderSystem>();
    systemManager.RegisterSystem<ECS::Systems::PlayerControlSystem>();

    resources.ScanResources();
    Resources::ResourceManager::InitDefaultResources();

    oxxoStore.Load();
    pathChunk01.Load();

    // clang-format off
	Skybox skybox({
	    "./assets/textures/skybox/sky_cubemap/px.png",
	    "./assets/textures/skybox/sky_cubemap/nx.png",
	    "./assets/textures/skybox/sky_cubemap/py.png",
	    "./assets/textures/skybox/sky_cubemap/ny.png",
	    "./assets/textures/skybox/sky_cubemap/pz.png",
	    "./assets/textures/skybox/sky_cubemap/nz.png"
	});
    // clang-format on
    skybox.Load();

    Shader shader = *resources.GetShader("base");
    Shader skyboxShader = *resources.GetShader("skybox_shader");
    Shader gridShader = *resources.GetShader("infinite_grid");
    Shader fbPixelShader = *resources.GetShader("fb_pixel");
    Shader debugShader = *resources.GetShader("debug");

    Framebuffer pixelFrameBuffer(fbPixelShader, window.GetWidth(), window.GetHeight());
    pixelFrameBuffer.SetMaxResolution(WIDTH, pixelFbResolution);
    pixelFrameBuffer.SetRenderFilter(GL_NEAREST);
    pixelFrameBuffer.CreateFramebuffer(window.GetWidth(), window.GetHeight());

    window.AddFramebuffer(&pixelFrameBuffer);

    Camera camera({2.0f, 2.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    camera.SetInput(Input::Keyboard::GetInstance(), Input::Mouse::GetInstance());

    camera.SetMoveSpeed(debugSettings.cameraMoveSpeed);
    camera.SetTurnSpeed(debugSettings.cameraTurnSpeed);

    mouse.ToggleMouse(enableCursor);
    window.SetMouseStatus(enableCursor);

    StorageBufferDynamicArray<Lights::PointLight> pointLights(3);
    pointLights.Add({.position = {2.0f, 2.0f, 2.0f, 0.0f},
                     .ambient = {0.1f, 0.1f, 0.1f, 0.0f},
                     .diffuse = {0.9f, 0.7f, 0.7f, 0.0f},
                     .specular = {1.0f, 1.0f, 1.0f, 0.0f},
                     .constant = 1.0f,
                     .linear = 0.09f,
                     .quadratic = 0.032f,
                     .isTurnedOn = true});

    ConfigureKeys(window);

    plane.Init();
    cube.Init();

    // region Entities
    ECS::Entity player = registry.CreateEntity();
    registry.AddComponent(player, ECS::Components::Transform{
                                      .translation = glm::vec3(0.0f)})
        .AddComponent(player, ECS::Components::AABBCollider{.min = glm::vec3(-1.0f), .max = glm::vec3(1.0f)});

    ECS::Entity oxxoStoreEntity = registry.CreateEntity();
    registry.AddComponent(oxxoStoreEntity, ECS::Components::Transform{
                                               .translation = {-5.0f, 0.0f, 0.0f},
                                               .scale = {0.1f, 0.1f, 0.1f}})
        .AddComponent(oxxoStoreEntity, ECS::Components::MeshRenderer{.model = &oxxoStore, .shader = &shader});

    for (int i = 0; i < 2; i++)
    {
        const ECS::Entity e = registry.CreateEntity();
        const float diffX = (2.0f * static_cast<float>(i)) - 5.0f;
        registry
            .AddComponent(e, ECS::Components::Transform{
                                 .translation = {diffX, 0.0f, 0.0f},
                                 .scale = glm::vec3(0.1f)})
            .AddComponent(e, ECS::Components::MeshRenderer{.model = &pathChunk01, .shader = &shader});
        pathEntities.push_front(e);
    }
    // endregion Entities

    glm::mat4 view;
    glm::mat4 projection;

    while (!window.ShouldClose())
    {
        auto now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastTime;
        lastTime = now;

        if (fpsCounter <= 1)
        {
            fpsCounter += deltaTime;
            fpsCount++;
        }
        else
        {
            fps = fpsCount;
            fpsCounter = 0;
            fpsCount = 0;
        }

        if (enablePixelate)
        {
            if (pixelFbResolution != lastPixelFbResolution)
            {
                pixelFrameBuffer.DestroyFramebuffer();
                pixelFrameBuffer.SetMaxResolution(WIDTH, pixelFbResolution);
                pixelFrameBuffer.CreateFramebuffer(window.GetWidth(), window.GetHeight());
                lastPixelFbResolution = pixelFbResolution;
            }

            pixelFrameBuffer.BeginRender();
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
        }

        glPolygonMode(GL_FRONT_AND_BACK, polygonMode ? GL_LINE : GL_FILL);

        window.StartGui();

        camera.Move(deltaTime);

        view = camera.GetLookAt();
        projection = glm::perspective(glm::radians(45.0f), pixelFrameBuffer.GetAspect(), 0.1f, 100.0f);

        if (enableSkybox)
        {
            skybox
                .BeginRender(skyboxShader)
                .SetProjection(projection)
                .SetView(view)
                .Render();
        }

        shader.Use();
        uniforms.model = shader.GetUniformLocation("model");

        shader.Set("pointLightsSize", static_cast<int>(pointLights.Size()));
        shader.Set<4, 4>("view", view);
        shader.Set<4, 4>("projection", projection);
        shader.Set<3>("ambientLightColor", glm::vec3{1.0f, 1.0f, 1.0f});

        shader.Set<3>("directionalLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.Set<3>("directionalLight.ambient", glm::vec3(0.08f, 0.08f, 0.08f));
        shader.Set<3>("directionalLight.diffuse", glm::vec3(0.08f, 0.08f, 0.08f));
        shader.Set<3>("directionalLight.specular", glm::vec3(0.08f, 0.08f, 0.08f));

        systemManager.UpdateAll(registry, deltaTime);

        // region Game Logic

        // 1.0 Path movement update ====================================================================================
        std::vector<ECS::Entity> pathsToRemove;
        for (ECS::Entity pathEntity : pathEntities)
        {
            auto &transform = registry.GetComponent<ECS::Components::Transform>(pathEntity);
            transform.translation.x = transform.translation.x - debugSettings.pathVelocity * deltaTime;

            if (transform.translation.x <= -5.0f)
                pathsToRemove.push_back(pathEntity);
        }

        // 1.1 Remove paths out of view ================================================================================
        for (ECS::Entity e : pathsToRemove)
        {
            pathEntities.pop_back();
            registry.DestroyEntity(e);
        }

        // 1.2 Create required new paths ===============================================================================
        if (const auto &lastPathTransform = registry.GetComponent<ECS::Components::Transform>(pathEntities.front());
            lastPathTransform.translation.x <= 50.0f)
        {
            const ECS::Entity e = registry.CreateEntity();
            registry
                .AddComponent(e, ECS::Components::Transform{
                                     .translation = {lastPathTransform.translation.x + 2.0f, 0.0f, 0.0f},
                                     .scale = glm::vec3(0.1f)})
                .AddComponent(e, ECS::Components::MeshRenderer{.model = &pathChunk01, .shader = &shader});
            pathEntities.push_front(e);
        }

        // endregion Game Logic

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (enableGrid)
        {
            gridShader.Use();
            gridShader.Set<4, 4>("uVP", projection * view);
            gridShader.Set<3>("cameraPosition", camera.GetPosition());
            plane.Render();
            shader.Use();
        }

        if (debugSettings.showHitboxes)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            debugShader.Use();
            const GLint debugProjection = debugShader.GetUniformLocation("projection");
            const GLint debugView = debugShader.GetUniformLocation("view");
            const GLint debugModel = debugShader.GetUniformLocation("model");
            const GLint debugColor = debugShader.GetUniformLocation("color");
            debugShader.Set<3>(debugColor, glm::vec3(1.0f, 1.0f, 0.0f));
            debugShader.Set<4, 4>(debugProjection, projection);
            debugShader.Set<4, 4>(debugView, view);

            // AABB Debug draw
            for (const ECS::Entity colliderEntity : registry.View<ECS::Components::AABBCollider, ECS::Components::Transform>())
            {
                const auto &transform = registry.GetComponent<ECS::Components::Transform>(colliderEntity);
                const auto &collider = registry.GetComponent<ECS::Components::AABBCollider>(colliderEntity);
                const auto worldCollider = collider.GetWorldAABB(transform);

                auto collidersModel = glm::mat4(1.0f);
                collidersModel = glm::translate(collidersModel, worldCollider.min + (worldCollider.max - worldCollider.min) * 0.5f);
                collidersModel = glm::scale(collidersModel, worldCollider.max - worldCollider.min);
                debugShader.Set<4, 4>(debugModel, collidersModel);

                cube.Render();
            }

            // OBB Debug draw
            for (const ECS::Entity obbEntity : registry.View<ECS::Components::OBBCollider, ECS::Components::Transform>())
            {
                const auto &transform = registry.GetComponent<ECS::Components::Transform>(obbEntity);
                const auto &collider = registry.GetComponent<ECS::Components::OBBCollider>(obbEntity);
                const auto worldOBB = collider.GetWorldOBB(transform);

                auto collidersModel = glm::mat4(1.0f);
                collidersModel = glm::translate(collidersModel, worldOBB.center);
                collidersModel *= glm::mat4_cast(worldOBB.rotation);
                collidersModel = glm::scale(collidersModel, worldOBB.halfExtents * 2.0f);
                debugShader.Set<4, 4>(debugModel, collidersModel);

                cube.Render();
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        glDisable(GL_BLEND);

        if (enablePixelate)
        {
            window.EnableWindowViewport();
            Framebuffer::EnableMainFramebuffer();
            pixelFrameBuffer.RenderQuad();
        }

        keyboard.HandleKeyLoop();

        // region gui
        if (showDebugGui)
        {
            ImGui::Begin("Camera info");
            auto camPos = camera.GetPosition();
            auto camDir = camera.GetDirection();
            ImGui::Text("Position: x=%f y=%f z=%f", static_cast<double>(camPos.x), static_cast<double>(camPos.y),
                        static_cast<double>(camPos.z));
            ImGui::Text("Direction: x=%f y=%f z=%f", static_cast<double>(camDir.x), static_cast<double>(camDir.y),
                        static_cast<double>(camDir.z));
            ImGui::Text("Yaw = %f | Pitch = %f", static_cast<double>(camera.GetYaw()),
                        static_cast<double>(camera.GetPitch()));
            ImGui::End();

            ImGui::Begin("Engine Info and Settings");
            ImGui::Text("Delta time = %f", static_cast<double>(deltaTime));
            ImGui::Text("FPS = %f", static_cast<double>(fps));
            if (ImGui::Checkbox("Vsync", &debugSettings.enableVsync))
                window.EnableVsync(debugSettings.enableVsync);
            ImGui::Checkbox("Grid", &enableGrid);
            ImGui::Checkbox("Skybox", &enableSkybox);
            ImGui::Checkbox("Show Hitboxes", &debugSettings.showHitboxes);

            ImGui::SeparatorText("Pixelate effect settings");

            ImGui::Checkbox("Pixelate framebuffer", &enablePixelate);
            ImGui::SliderInt("Resolution (width)", &pixelFbResolution, 64, 2048);

            ImGui::SeparatorText("Shader reload");

            if (ImGui::Button("Reload base shader"))
            {
                shader.ReloadShader();
                std::cout << "Shader reloaded\n";
            }

            ImGui::SeparatorText("Other settings");
            ImGui::Checkbox("Show polygon lines", &polygonMode);

            if (ImGui::DragFloat("Camera move speed", &debugSettings.cameraMoveSpeed))
                camera.SetMoveSpeed(debugSettings.cameraMoveSpeed);

            if (ImGui::DragFloat("Camera turn speed", &debugSettings.cameraTurnSpeed))
                camera.SetTurnSpeed(debugSettings.cameraTurnSpeed);

            ImGui::DragFloat("Road speed", &debugSettings.pathVelocity, 0.001f, 0.0f);

            ImGui::End();

            ImGui::Begin("Lights control");

            if (ImGui::Button("Add light"))
            {
                pointLights.Add({.position = {0.0f, 0.0f, 2.0f, 0.0f},
                                 .ambient = {0.1f, 0.1f, 0.1f, 0.0f},
                                 .diffuse = {1.0f, 1.0f, 1.0f, 0.0f},
                                 .specular = {1.0f, 1.0f, 1.0f, 0.0f},
                                 .constant = 1.0f,
                                 .linear = 0.09f,
                                 .quadratic = 0.032f,
                                 .isTurnedOn = true});
            }

            for (size_t i = 0; i < pointLights.Size(); ++i)
            {
                Lights::PointLight &pLight = pointLights[i];
                ImGui::PushID(std::format("PL{}", i).c_str());
                ImGui::Columns(3, "Position");
                ImGui::DragFloat("X", &pLight.position.x);
                ImGui::NextColumn();
                ImGui::DragFloat("Y", &pLight.position.y);
                ImGui::NextColumn();
                ImGui::DragFloat("Z", &pLight.position.z);
                ImGui::Columns(1);
                ImGui::SeparatorText(std::format("Pointlight {}", i).c_str());
                ImGui::SliderFloat("Constant", &pLight.constant, 0.0, 1.0);
                ImGui::SliderFloat("Linear", &pLight.linear, 0.0, 1.0);
                ImGui::SliderFloat("Quadratic", &pLight.quadratic, 0.0, 1.0);
                ImGui::Checkbox("Is turned on", reinterpret_cast<bool *>(&pLight.isTurnedOn));
                ImGui::ColorEdit3("Color", reinterpret_cast<float *>(&pLight.diffuse), ImGuiColorEditFlags_Float);
                pointLights.UpdateIndex(i);

                if (ImGui::Button(std::format("Delete", i).c_str()))
                    pointLights.Remove(i);

                ImGui::PopID();
            }
            ImGui::End();
        }
        // endregion

        // region Special keys handle
        if (!keyboard.GetKeyPress(GLFW_KEY_T)) enableCursorEvent = false;
        if (!keyboard.GetKeyPress(GLFW_KEY_F11)) fullscreenEvent = false;
        if (!keyboard.GetKeyPress(GLFW_KEY_F3)) debugMode.event = false;
        // endregion

        window.EndGui();
        window.EndRenderPass();
    }

    SaveSettings();

    alutExit();

    return 0;
}

#pragma clang diagnostic pop
