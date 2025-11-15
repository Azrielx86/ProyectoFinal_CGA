#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
// region Global Include (Must be the first!)
#define STB_IMAGE_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include "GlobalDefines.h"
// endregion Global Include

#include "Camera.h"
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
#include <AL/al.h>
#include <AL/alut.h>

#include <iostream>

#define RGBCOLOR(r, g, b) glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f)

struct SingleKeyPress
{
    bool clicked = true;
    bool event = true;
};

SingleKeyPress debugMode;

float deltaTime, lastTime;
float red = 0.0f;
bool showDebugGui = true;
bool enableCursorEvent = true;
bool fullscreenEvent;
bool enableCursor = true;
bool enableGrid = true;
bool enableSkybox = false;
bool enablePixelate = true;
bool gridMode = false;
int pixelFbResolution = 320;
int lastPixelFbResolution = 320;

float fpsCounter = 0;
float fpsCount = 0;
float fps = 0;

// region Models Section
Model oxxoStore("./assets/models/OxxoStore/OxxoStore.obj");
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

int main(int argc, char** argv)
{
    Window window(1280, 720, "Proyecto Final CGA");

    alutInit(&argc, argv);

    if (!window.Init())
    {
        std::cerr << "Cannot initialize window.\n";
        return 1;
    }

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

    Framebuffer pixelFrameBuffer(fbPixelShader, window.GetWidth(), window.GetHeight());
    pixelFrameBuffer.SetMaxResolution(WIDTH, pixelFbResolution);
    pixelFrameBuffer.SetRenderFilter(GL_NEAREST);
    pixelFrameBuffer.CreateFramebuffer(window.GetWidth(), window.GetHeight());

    window.AddFramebuffer(&pixelFrameBuffer);

    Camera camera({2.0f, 2.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    camera.SetInput(Input::Keyboard::GetInstance(), Input::Mouse::GetInstance());

    float cameraMoveSpeed = 150.0f;
    float cameraTurnSpeed = 75.0f;

    camera.SetMoveSpeed(cameraMoveSpeed);
    camera.SetTurnSpeed(cameraTurnSpeed);

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
    ECS::Entity oxxoStoreEntity = registry.CreateEntity();
    registry.AddComponent(oxxoStoreEntity, ECS::Components::Transform{
                                               .translation = {0.0f, 0.0f, 0.0f},
                                               .scale = {0.1f, 0.1f, 0.1f}});
    registry.AddComponent(oxxoStoreEntity, ECS::Components::MeshRenderer{
                                               .model = &oxxoStore,
                                               .shader = &shader});
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

        glPolygonMode(GL_FRONT_AND_BACK, gridMode ? GL_LINE : GL_FILL);

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

        systemManager.UpdateAll(registry, deltaTime);

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
            ImGui::Checkbox("Enable Grid", &enableGrid);
            ImGui::Checkbox("Enable skybox", &enableSkybox);

            ImGui::SeparatorText("Pixelate effect settings");
            ImGui::Checkbox("Enable pixelate", &enablePixelate);
            ImGui::SliderInt("Resolution (width)", &pixelFbResolution, 64, 2048);

            ImGui::SeparatorText("Shader reload");

            if (ImGui::Button("Reload base shader"))
            {
                shader.ReloadShader();
                std::cout << "Shader reloaded";
            }

            ImGui::SeparatorText("Other settings");
            if (ImGui::Checkbox("Set grid mode", &gridMode))
                std::cout << std::format("Grid mode: {}\n", gridMode ? "enabled" : "disabled");

            if (ImGui::DragFloat("Camera move speed", &cameraMoveSpeed))
                camera.SetMoveSpeed(cameraMoveSpeed);

            if (ImGui::DragFloat("Camera turn speed", &cameraTurnSpeed))
                camera.SetTurnSpeed(cameraTurnSpeed);

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

    alutExit();

    return 0;
}

#pragma clang diagnostic pop
