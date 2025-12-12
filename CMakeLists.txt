cmake_minimum_required(VERSION 4.0)
project(ProyectoFinal_CGA)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)

if (FIX_ASSIMP_MEMCALL)
    set(CMAKE_CXX_FLAGS "-Wno-nontrivial-memcall")
endif ()

if (NOT UNIX)
    message(STATUS "Compiling Windows version")
    add_compile_definitions(-DWIN32)
endif ()

if (VCPKG_TOOLCHAIN)
    find_package(nlohmann_json CONFIG REQUIRED)
else ()
    find_package(nlohmann_json REQUIRED)
endif ()

add_subdirectory(AzxEngineGL)

add_compile_options(-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-unknown-pragmas -Wuninitialized)
add_link_options(-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-unknown-pragmas -Wuninitialized)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Compiling Debug version.")
    add_compile_options(-O0 -gfull -DDEBUG -fno-inline -fstack-protector-all)
    add_link_options(-O0 -gfull -DDEBUG -fno-inline -fstack-protector-all)
else ()
    add_compile_options(-O3)
endif ()

add_executable(ProyectoFinal_CGA src/main.cpp
        src/GameSettings.h
        src/DebugSettings.h
        src/Components/RunnerComponent.h
        src/Systems/RunnerSystem.cpp
        src/Systems/RunnerSystem.h
        src/Components/FloorComponent.h
        src/Systems/CoinSystem.cpp
        src/Systems/CoinSystem.h
        src/Components/CoinComponent.h
        src/Components/PathComponent.h
        src/Components/ObstacleComponent.h
)

if (NOT USE_DEBUG_ASSETS)
    message(STATUS "Copying shader files.")
    file(COPY shaders DESTINATION ${PROJECT_BINARY_DIR})
    message(STATUS "Copying assets files.")
    file(COPY assets DESTINATION ${PROJECT_BINARY_DIR})
else ()
    message(STATUS "Using debug assets.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_DEBUG_ASSETS")
endif ()

target_link_libraries(ProyectoFinal_CGA PUBLIC AzxEngineGL)
target_link_libraries(ProyectoFinal_CGA PUBLIC nlohmann_json::nlohmann_json)
