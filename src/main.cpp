#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
// region Global Include (Must be the first!)
#define STB_IMAGE_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include "GlobalDefines.h"
// endregion Global Include

#include "Camera.h"
#include "Components/CoinComponent.h"
#include "Components/FloorComponent.h"
#include "Components/ObstacleComponent.h"
#include "Components/PathComponent.h"
#include "Components/RunnerComponent.h"
#include "DebugSettings.h"
#include "ECS/Components/Collider.h"
#include "ECS/Components/MeshRenderer.h"
#include "ECS/Components/PlayerController.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SystemManager.h"
#include "ECS/Systems/CollisionSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "FontType.h"
#include "Input/Keyboard.h"
#include "Lights/PointLight.h"
#include "Model.h"
#include "Primitives/Cube.h"
#include "Primitives/Plane.h"
#include "Resources/ResourceManager.h"
#include "Shader.h"
#include "SkinnedAnimation.h"
#include "SkinnedAnimator.h"
#include "Skybox.h"
#include "StorageBufferDynamicArray.h"
#include "Systems/CoinSystem.h"
#include "Systems/RunnerSystem.h"
#include "Window.h"
#include "imgui.h"

#include <AL/alut.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <random>

#define RGBCOLOR(r, g, b) glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f)

enum GameScene
{
    MAINMENU,
    INGAME,
    GAMEOVER
};

enum MenuOptions
{
    START,
    EXIT
};

struct SingleKeyPress
{
    bool clicked = true;
    bool event = true;
};

SingleKeyPress debugMode;
SingleKeyPress menuUp;
SingleKeyPress menuDown;
SingleKeyPress cameraChange;
GameScene gameScene = MAINMENU;
uint32_t currentOption = 0;
std::array menuOptions = {START, EXIT};

const std::string debugSettingsPath = "./debug_settings.json";

float deltaTime, lastTime;
float red = 0.0f;
bool showDebugGui = false;
bool enableCursorEvent = true;
bool fullscreenEvent;
bool enableCursor = true;
bool enableGrid = false;
bool enableSkybox = true;
bool enablePixelate = true;
bool polygonMode = false;
int pixelFbResolution = 320;
int lastPixelFbResolution = 320;
bool mainGameStarted = false;
bool useFreeCamera = false;

float fpsCounter = 0;
float fpsCount = 0;
float fps = 0;

// region Models Section
Model oxxoStore(
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
    "."
#endif
    "./assets/models/OxxoStore/OxxoStore.obj");
Model pathChunk01(
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
    "."
#endif
    "./assets/models/Path/Path.obj");
Model tsuruCar(
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
    "."
#endif
    "./assets/models/Tsuru/Tsuru.obj");
Model iceCreamCart(
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
    "."
#endif
    "./assets/models/IceCreamCart/IceCreamCart.fbx");

Model lowPolyManModel(
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
    "."
#endif
    "./assets/models/LowPolyMan/LowPolyMan.fbx");
SkinnedAnimation *playerAnimation;
SkinnedAnimator playerAnimator;

// endregion Models Section

Input::Keyboard &keyboard = *Input::Keyboard::GetInstance();
Input::Mouse &mouse = *Input::Mouse::GetInstance();
Resources::ResourceManager &resources = *Resources::ResourceManager::GetInstance();

GLuint particleVAO;

struct Uniforms
{
    GLint model = 0;
};

enum PATH_PATTERN
{
    CLEAN,
    OBSTACLE,
    POWERUP
};

constexpr std::array ObstaclePatterns = {
    std::array{OBSTACLE, CLEAN,    CLEAN   },
    std::array{CLEAN,    OBSTACLE, CLEAN   },
    std::array{CLEAN,    CLEAN,    OBSTACLE},
    std::array{OBSTACLE, OBSTACLE, CLEAN   },
    std::array{OBSTACLE, OBSTACLE, OBSTACLE},
    std::array{OBSTACLE, CLEAN,    OBSTACLE},
};

std::random_device randomDevice;
std::mt19937 generator(randomDevice());
std::uniform_int_distribution<int> obstaclePatternGenerator(0, ObstaclePatterns.size() - 1);

Uniforms uniforms{};

Primitives::Plane plane;
Primitives::Cube cube;

ECS::Registry registry;
ECS::SystemManager systemManager;

ECS::Entity player;
ECS::Entity floorEntity;
ECS::Entity debugDummy;
ECS::Entity oxxoStoreEntity;

ECS::Entity lastPath;

DebugSettings debugSettings;

Camera *mainCamera;
Camera *previousUsedCamera;

// region Game Variables
float metersRunned = 0.0f;
int pathsGenerated = 0;
bool obstaclesCanSpawn = false;
constexpr int generatorSpaceInterval = 8;

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

void LoadInGameEntities(Shader &shader)
{
    // region Entities
    player = registry.CreateEntity();
    registry
        .AddComponent(player, ECS::Components::Transform{
                                  .translation = {0.0f, 2.0f, 0.0f}
    })
        .AddComponent(player, RunnerComponent{})
        .AddComponent(player, ECS::Components::AABBCollider{.min = {-0.5f, -1.0f, -0.5f}, .max = {0.5f, 1.0f, 0.5f}});

    floorEntity = registry.CreateEntity();
    registry
        .AddComponent(floorEntity, ECS::Components::Transform{
                                       .translation = {0.0f, 0.0f, 0.0f},
                                       .scale = glm::vec3(0.1f)
    })
        .AddComponent(floorEntity, ECS::Components::AABBCollider{.min = {-10.0f, -0.5f, -25.31f}, .max = {10.0f, 0.5f, 25.31f}})
        .AddComponent(floorEntity, FloorComponent{});

    debugDummy = registry.CreateEntity();
    registry.AddComponent(debugDummy, ECS::Components::Transform{
                                          .translation = {0.0f, 2.0f, 0.0f}
    })
        .AddComponent(debugDummy, ECS::Components::AABBCollider{.min = {-0.5f, -1.0f, -0.5f}, .max = {0.5f, 1.0f, 0.5f}});
    // !Only enable when is required
    registry.DestroyEntity(debugDummy);

    oxxoStoreEntity = registry.CreateEntity();
    registry.AddComponent(oxxoStoreEntity, ECS::Components::Transform{
                                               .translation = {5.0f, 0.0f, -8.5f},
                                               .scale = glm::vec3(0.25f)
    })
        .AddComponent(oxxoStoreEntity, ECS::Components::MeshRenderer{.model = &oxxoStore, .shader = &shader});

    const auto tsuruEntity = registry.CreateEntity();
    registry.AddComponent(tsuruEntity, ECS::Components::Transform{
                                           .translation = {15.0f, 0.0f, -8.0f}
    })
        .AddComponent(tsuruEntity, ECS::Components::MeshRenderer{.model = &tsuruCar, .shader = &shader});

    for (int i = 0; i < 2; i++)
    {
        const ECS::Entity e = registry.CreateEntity();
        const float diffX = (2.0f * static_cast<float>(i)) - 5.0f;
        registry
            .AddComponent(e, ECS::Components::Transform{
                                 .translation = {diffX, 0.0f, 0.0f},
                                 .scale = glm::vec3(0.1f)
        })
            .AddComponent(e, PathComponent{})
            .AddComponent(e, ECS::Components::MeshRenderer{.model = &pathChunk01, .shader = &shader});
        lastPath = e;
    }
    // endregion Entities
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
    registry.RegisterComponent<RunnerComponent>();
    registry.RegisterComponent<FloorComponent>();
    registry.RegisterComponent<PathComponent>();
    registry.RegisterComponent<ObstacleComponent>();
    registry.RegisterComponent<CoinComponent>();
    systemManager.RegisterSystem<ECS::Systems::CollisionSystem>();
    systemManager.RegisterSystem<ECS::Systems::RenderSystem>();
    systemManager.RegisterSystem<RunnerSystem>();
    systemManager.RegisterSystem<CoinSystem>();

    auto runnerSystem = systemManager.GetSystem<RunnerSystem>();
    runnerSystem->SetEnabled(false);

    resources.ScanResources();
    Resources::ResourceManager::InitDefaultResources();

    oxxoStore.Load();
    pathChunk01.Load();
    tsuruCar.Load();
    iceCreamCart.Load();
    lowPolyManModel.Load();

    playerAnimation = lowPolyManModel.GetAnimation(2);
    if (playerAnimation)
    {
        playerAnimator.PlayAnimation(playerAnimation);
    }

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

    Camera freeCamera({2.0f, 2.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    Camera menuCamera({3.0f, 1.0f, 2.8f}, {0.0f, 1.0f, 0.0f},
                      -58.85f, 2.30f, 0, 0);
    Camera gameCamera({-7.5f, 7.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                      0.31f, -26.45f, 0, 0);
    gameCamera.Lock();
    menuCamera.Lock();

    mainCamera = &menuCamera;
    previousUsedCamera = &menuCamera;

    freeCamera.SetInput(Input::Keyboard::GetInstance(), Input::Mouse::GetInstance());
    gameCamera.SetInput(Input::Keyboard::GetInstance(), Input::Mouse::GetInstance());

    freeCamera.SetMoveSpeed(debugSettings.cameraMoveSpeed);
    freeCamera.SetTurnSpeed(debugSettings.cameraTurnSpeed);

    keyboard.AddCallback(GLFW_KEY_P, [&freeCamera]() -> void
                         {
                             if (cameraChange.event) return;
                             cameraChange.event = true;
                             useFreeCamera = !useFreeCamera;
                             if (useFreeCamera)
                             {
                                 previousUsedCamera = mainCamera;
                                 mainCamera = &freeCamera;
                             }
                             else
                                 mainCamera = previousUsedCamera;
                         });

    // mouse.ToggleMouse(enableCursor);
    window.SetMouseStatus(enableCursor);

    StorageBufferDynamicArray<Lights::PointLight> pointLights(3);
    pointLights.Add({
        .position = {2.0f, 2.0f, 2.0f, 0.0f},
        .ambient = {0.1f, 0.1f, 0.1f, 0.0f},
        .diffuse = {0.9f, 0.7f, 0.7f, 0.0f},
        .specular = {1.0f, 1.0f, 1.0f, 0.0f},
        .constant = 1.0f,
        .linear = 0.09f,
        .quadratic = 0.032f,
        .isTurnedOn = true
    });

    FontType fontBearDays(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()), "../fonts/BearDays.ttf", 1.2f);
    fontBearDays.Init();
    FontType fontArial(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()), "../fonts/arial.ttf", 0.30f);
    fontArial.Init();

    ConfigureKeys(window);

    plane.Init();
    cube.Init();

    glm::mat4 view;
    glm::mat4 projection;

    // * ===================================================================== *
    // *                             GAME LOOP                                 *
    // * ===================================================================== *
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

        mainCamera->Move(deltaTime);

        glm::mat4 model(1.0f);
        view = mainCamera->GetLookAt();
        projection = glm::perspective(glm::radians(45.0f), pixelFrameBuffer.GetAspect(), 0.1f, 100.0f);
        std::vector<glm::mat4> finalBones;

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
        shader.Set<3>("fogColor", glm::vec3(0.0f));

        shader.Set<3>("directionalLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.Set<3>("directionalLight.ambient", glm::vec3(0.08f, 0.08f, 0.08f));
        shader.Set<3>("directionalLight.diffuse", glm::vec3(0.08f, 0.08f, 0.08f));
        shader.Set<3>("directionalLight.specular", glm::vec3(0.08f, 0.08f, 0.08f));

        systemManager.UpdateAll(registry, deltaTime);

        switch (gameScene)
        {
        case MAINMENU:
        {
            // region MainMenuScene
            // draw paths
            for (unsigned int i = 0; i < 10; i++)
            {
                model = glm::mat4(1.0f);
                model = glm::translate(model, {2.0f * static_cast<float>(i), 0.0f, 0.0f});
                model = glm::scale(model, glm::vec3(0.1f));
                shader.Set<4, 4>(uniforms.model, model);
                pathChunk01.Render(shader);
            }

            // Render OXXO
            model = glm::translate(glm::mat4(1.0f), {5.0f, 0.0f, -9.0f});
            model = glm::scale(model, glm::vec3(0.30f));
            shader.Set<4, 4>(uniforms.model, model);
            oxxoStore.Render(shader);

            // Render Ice Cream Cart
            model = glm::translate(glm::mat4(1.0f), {5.2f, 0.65f, -1.45f});
            model = glm::rotate(model, glm::radians(120.0f), {0, 1, 0});
            model = glm::rotate(model, glm::radians(-90.0f), {1, 0, 0});
            model = glm::scale(model, glm::vec3(0.8f));
            shader.Set<4, 4>(uniforms.model, model);
            iceCreamCart.Render(shader);

            // TODO : Tsuru model
            model = glm::translate(glm::mat4(1.0f), {8.0f, 0.10f, -1.6f});
            model = glm::rotate(model, glm::radians(90.0f), {0, 1, 0});
            model = glm::scale(model, glm::vec3(0.5f));
            shader.Set<4, 4>(uniforms.model, model);
            tsuruCar.Render(shader);

            playerAnimator.UpdateAnimation(deltaTime);
            finalBones = playerAnimator.GetFinalBoneMatrices();
            model = glm::translate(glm::mat4(1.0f), {4.0f, 0.0f, -0.5f});
            model = glm::scale(model, glm::vec3(0.15f));
            shader.Set<4, 4>(uniforms.model, model);
            for (unsigned int i = 0; i < finalBones.size(); i++)
                shader.Set<4, 4>(std::format("bones[{}]", i).c_str(), finalBones[i]);
            lowPolyManModel.Render(shader);

            for (unsigned int i = 0; i < MAX_BONES; i++)
                shader.Set<4, 4>(std::format("bones[{}]", i).c_str(), glm::mat4(1.0f));

            // endregion MainMenuScene

            fontBearDays.SetScale(1.2f, static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()))
                .SetColor(currentOption == START ? glm::vec4(1.0f) : glm::vec4(0.8f, 0.8f, 0.8f, 1.0f))
                .Render(0.3f, 0.2f, "Start");

            fontBearDays
                .SetColor(currentOption == EXIT ? glm::vec4(1.0f) : glm::vec4(0.8f, 0.8f, 0.8f, 1.0f))
                .Render(0.3f, -0.2f, "Exit");

            fontBearDays.SetScale(1.8f, static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()))
                .SetColor(glm::vec4(1.0f))
                .Render(-0.9f, -0.7f, "City Escape");

            fontBearDays.SetScale(0.60f, static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()))
                .SetColor(glm::vec4(1.0f))
                .Render(-0.9f, -0.9f, "An Advanced Computer Graphics Project");

            if (keyboard.GetKeyPress(GLFW_KEY_ENTER))
            {
                switch (menuOptions[currentOption])
                {
                case START:
                    LoadInGameEntities(shader);
                    mainGameStarted = true;
                    runnerSystem->SetEnabled(true);
                    mainCamera = &gameCamera;

                    playerAnimation = lowPolyManModel.GetAnimation(7);
                    if (playerAnimation)
                    {
                        playerAnimator.PlayAnimation(playerAnimation);
                    }

                    gameScene = INGAME;
                    break;
                case EXIT:
                    window.SetShouldClose(true);
                    break;
                }
            }

            if (!menuUp.event && keyboard.GetKeyPress(GLFW_KEY_UP))
                menuUp.event = true;

            if (!menuDown.event && keyboard.GetKeyPress(GLFW_KEY_DOWN))
                menuDown.event = true;

            if (menuUp.event && !keyboard.GetKeyPress(GLFW_KEY_UP))
            {
                currentOption = (currentOption == 0) ? menuOptions.size() - 1 : currentOption - 1;
                menuUp.event = false;
            }

            if (menuDown.event && !keyboard.GetKeyPress(GLFW_KEY_DOWN))
            {
                currentOption = (currentOption == menuOptions.size() - 1) ? 0 : currentOption + 1;
                menuDown.event = false;
            }

            break;
        }
        case INGAME:
        {
            // region Game Logic
            metersRunned += debugSettings.pathVelocity * deltaTime;
            // * ================================================================= *
            // * Path Generation                                                   *
            // * ================================================================= *
            // 1.1 Path movement update ====================================================================================
            for (ECS::Entity pathEntity : registry.View<PathComponent, ECS::Components::Transform>())
            {
                auto &transform = registry.GetComponent<ECS::Components::Transform>(pathEntity);
                transform.translation.x = transform.translation.x - debugSettings.pathVelocity * deltaTime;

                // Remove out of view paths
                if (transform.translation.x <= -5.0f)
                    registry.DestroyEntity(pathEntity);
            }

            // 1.2 Create required new paths ===============================================================================
            if (const auto &lastPathTransform = registry.GetComponent<ECS::Components::Transform>(lastPath);
                lastPathTransform.translation.x <= 100.0f)
            {
                const ECS::Entity e = registry.CreateEntity();
                registry.AddComponent(e, ECS::Components::Transform{
                                             .translation = {lastPathTransform.translation.x + 2.0f, 0.0f, 0.0f},
                                             .scale = glm::vec3(0.1f)
                })
                    .AddComponent(e, PathComponent{})
                    .AddComponent(e, ECS::Components::MeshRenderer{.model = &pathChunk01, .shader = &shader});
                lastPath = e;
                pathsGenerated = (pathsGenerated + 1) % generatorSpaceInterval;
                obstaclesCanSpawn = true;
            }

            // * ================================================================= *
            // * Obstacles generation                                              *
            // * ================================================================= *
            // 2.1 Update obstacles ================================================
            for (const ECS::Entity obstacle : registry.View<ObstacleComponent, ECS::Components::Transform>())
            {
                auto &transform = registry.GetComponent<ECS::Components::Transform>(obstacle);
                transform.translation.x = transform.translation.x - debugSettings.pathVelocity * deltaTime;

                if (transform.translation.x <= -5.0f)
                    registry.DestroyEntity(obstacle);
            }

            // 2.2 Create new obstacles ============================================
            if (pathsGenerated == 0 && obstaclesCanSpawn)
            {
                const auto &lastPathTransform = registry.GetComponent<ECS::Components::Transform>(lastPath);

                const int genIdx = obstaclePatternGenerator(generator);
                auto pattern = ObstaclePatterns[genIdx];

                for (size_t i = 0; i < pattern.size(); i++)
                {
                    if (pattern[i] == CLEAN) continue;

                    constexpr float laneWidth = 2.0f;
                    float obstaclePos = (static_cast<float>(i) * laneWidth) - laneWidth;
                    ECS::Entity obstacle = registry.CreateEntity();
                    registry
                        .AddComponent(obstacle, ECS::Components::Transform{
                                                    .translation = {lastPathTransform.translation.x, 1.0f, obstaclePos}
                    })
                        .AddComponent(obstacle, ObstacleComponent{})
                        .AddComponent(obstacle, ECS::Components::AABBCollider{.min = glm::vec3(-0.5f), .max = glm::vec3(0.5f)});

                    // obstaclesEntities.push_front(obstacle);
                    obstaclesCanSpawn = false;
                }
            }

            // * ================================================================= *
            // * Coins generation                                                  *
            // * ================================================================= *
            // 3.1 Update coins ====================================================
            for (const ECS::Entity coin : registry.View<CoinComponent, ECS::Components::Transform>())
            {
                auto &transform = registry.GetComponent<ECS::Components::Transform>(coin);
                transform.translation.x = transform.translation.x - debugSettings.pathVelocity * deltaTime;

                if (transform.translation.x <= -5.0f)
                    registry.DestroyEntity(coin);
            }

            // 3.3 Create new coins ================================================
            if (pathsGenerated == 0 && obstaclesCanSpawn)
            {

                // ! NOT WORKING, REMOVE LATER
                // const auto &lastPathTransform = registry.GetComponent<ECS::Components::Transform>(lastPath);
                //
                // for (size_t i = 0; i < 3; i++)
                // {
                //     constexpr float laneWidth = 2.0f;
                //     float coinPos = (static_cast<float>(i) * laneWidth) - laneWidth;
                //     ECS::Entity coin = registry.CreateEntity();
                //     registry
                //         .AddComponent(coin, ECS::Components::Transform{
                //                                 .translation = {lastPathTransform.translation.x, 1.0f, coinPos}
                //     })
                //         .AddComponent(coin, ECS::Components::AABBCollider{.min = glm::vec3(-0.5f), .max = glm::vec3(0.5f)})
                //         .AddComponent(coin, CoinComponent{10});
                // }
            }

            playerAnimator.UpdateAnimation(deltaTime);
            finalBones = playerAnimator.GetFinalBoneMatrices();

            auto playerTransform = registry.GetComponent<ECS::Components::Transform>(player);
            model = glm::translate(glm::mat4(1.0f), playerTransform.translation) * glm::mat4_cast(playerTransform.rotation) * glm::scale(glm::mat4(1.0f), playerTransform.scale);
            model = glm::translate(model, {0.0f, -0.80f, 0.0f});
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.20f));
            shader.Set<4, 4>(uniforms.model, model);
            for (unsigned int i = 0; i < finalBones.size(); i++)
                shader.Set<4, 4>(std::format("bones[{}]", i).c_str(), finalBones[i]);
            lowPolyManModel.Render(shader);

            for (unsigned int i = 0; i < MAX_BONES; i++)
                shader.Set<4, 4>(std::format("bones[{}]", i).c_str(), glm::mat4(1.0f));

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (enableGrid)
            {
                gridShader.Use();
                gridShader.Set<4, 4>("uVP", projection * view);
                gridShader.Set<3>("cameraPosition", mainCamera->GetPosition());
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

                    // Only to differentiate floor&player grounding from other colliders
                    if ((registry.HasComponent<RunnerComponent>(colliderEntity)
                         && collider.collidingEntities.size() == 1
                         && std::ranges::find(collider.collidingEntities, floorEntity) != collider.collidingEntities.end())
                        || (registry.HasComponent<FloorComponent>(colliderEntity)
                            && std::ranges::find(collider.collidingEntities, player) != collider.collidingEntities.end()))
                        debugShader.Set<3>(debugColor, glm::vec3(0.0f, 1.0f, 1.0f));
                    else
                        debugShader.Set<3>(debugColor, collider.isColliding
                                                           ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                           : glm::vec3(1.0f, 1.0f, 0.0f));

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

                    debugShader.Set<3>(debugColor, collider.isColliding ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(1.0f, 1.0f, 0.0f));

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

            fontBearDays.SetScale(1.2f, static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()))
                .SetColor({1.0f, 0.0f, 0.0f, 0.5f})
                .Render(-0.95f, 0.80f, std::format("DISTANCE: {:.0f}", metersRunned));

            auto &playerComponent = registry.GetComponent<RunnerComponent>(player);
            fontBearDays.SetScale(1.2f, static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()))
                .SetColor({1.0f, 1.0f, 0.0f, 0.5f})
                .Render(-0.95f, 0.70f, std::format("SCORE: {}", playerComponent.score));
            break;
        }
            // endregion Game Logic
        default:;
        }

        if (enablePixelate)
        {
            window.EnableWindowViewport();
            Framebuffer::EnableMainFramebuffer();
            pixelFrameBuffer.RenderQuad();
        }

        fontArial.SetColor(glm::vec4(1.0f))
            .Render(0.75f, -0.95f, "PreAlpha 1.0.0");

        keyboard.HandleKeyLoop();

        // region gui
        if (showDebugGui)
        {
            ImGui::Begin("Camera info");
            auto camPos = mainCamera->GetPosition();
            auto camDir = mainCamera->GetDirection();
            ImGui::Text("Position: x=%f y=%f z=%f", static_cast<double>(camPos.x), static_cast<double>(camPos.y), static_cast<double>(camPos.z));
            ImGui::Text("Direction: x=%f y=%f z=%f", static_cast<double>(camDir.x), static_cast<double>(camDir.y), static_cast<double>(camDir.z));
            ImGui::Text("Yaw = %f | Pitch = %f", static_cast<double>(mainCamera->GetYaw()), static_cast<double>(mainCamera->GetPitch()));
            if (ImGui::Button("Switch camera"))
            {
                useFreeCamera = !useFreeCamera;
                if (useFreeCamera)
                {
                    previousUsedCamera = mainCamera;
                    mainCamera = &freeCamera;
                }
                else
                    mainCamera = previousUsedCamera;
            }
            ImGui::End();

            ImGui::Begin("Player info");
            const auto &playerTransform = registry.GetComponent<ECS::Components::Transform>(player);
            ImGui::Text("Position: x=%f y=%f z=%f", static_cast<double>(playerTransform.translation.x),
                        static_cast<double>(playerTransform.translation.y),
                        static_cast<double>(playerTransform.translation.z));
            ImGui::Text("Meters runned: %.2f", static_cast<double>(metersRunned));
            ImGui::End();

            ImGui::Begin("Engine Info and Settings");
            ImGui::Text("Game Time: %.2f", glfwGetTime());
            ImGui::Text("Delta time = %f", static_cast<double>(deltaTime));
            ImGui::Text("FPS = %f", static_cast<double>(fps));
            ImGui::Text("Paths generated: %d", pathsGenerated);
            ImGui::Text("Entities in scene: %lu", registry.GetEntityCount());
            if (ImGui::Checkbox("Vsync", &debugSettings.enableVsync))
                window.EnableVsync(debugSettings.enableVsync);

            if (gameScene == MAINMENU && !mainGameStarted)
            {
                if (ImGui::Button("Start Game"))
                {
                    LoadInGameEntities(shader);
                    mainGameStarted = true;
                    runnerSystem->SetEnabled(true);
                    gameScene = INGAME;
                }
            }

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
                mainCamera->SetMoveSpeed(debugSettings.cameraMoveSpeed);

            if (ImGui::DragFloat("Camera turn speed", &debugSettings.cameraTurnSpeed))
                mainCamera->SetTurnSpeed(debugSettings.cameraTurnSpeed);

            ImGui::DragFloat("Road speed", &debugSettings.pathVelocity, 0.001f, 0.0f);

            ImGui::End();

            ImGui::Begin("Lights control");

            if (ImGui::Button("Add light"))
            {
                pointLights.Add({
                    .position = {0.0f, 0.0f, 2.0f, 0.0f},
                    .ambient = {0.1f, 0.1f, 0.1f, 0.0f},
                    .diffuse = {1.0f, 1.0f, 1.0f, 0.0f},
                    .specular = {1.0f, 1.0f, 1.0f, 0.0f},
                    .constant = 1.0f,
                    .linear = 0.09f,
                    .quadratic = 0.032f,
                    .isTurnedOn = true
                });
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
        if (!keyboard.GetKeyPress(GLFW_KEY_P)) cameraChange.event = false;
        // endregion

        window.EndGui();
        window.EndRenderPass();
    }

    SaveSettings();

    alutExit();

    return 0;
}

#pragma clang diagnostic pop
