cmake_minimum_required(VERSION 3.16)

# Sets project name
project(CSCI-1230-FINAL-PROJECT LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Sets C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Specifies required Qt components
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS OpenGL)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
find_package(Qt6 REQUIRED COMPONENTS Xml)

# Allows you to include files from within those directories, without prefixing their filepaths
include_directories(src)

# Specifies .cpp and .h files to be passed to the compiler
add_executable(${PROJECT_NAME}
    src/main.cpp

    src/realtime.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/utils/scenefilereader.cpp
    src/utils/sceneparser.cpp
    src/shapes/Cone.cpp
    src/shapes/Cube.cpp
    src/shapes/Sphere.cpp
    src/shapes/Cylinder.cpp

    src/mainwindow.h
    src/realtime.h
    src/settings.h
    src/lut_utils.h
    src/utils/scenedata.h
    src/utils/scenefilereader.h
    src/utils/sceneparser.h
    src/utils/shaderloader.h
    src/utils/bezier.h
    src/utils/camera_path.h
    src/shapes/Cube.h
    src/utils/aspectratiowidget/aspectratiowidget.hpp
    src/shapes/Cone.h
    src/shapes/Cube.h
    src/shapes/Sphere.h
    src/shapes/Cylinder.h
    src/camera.cpp
    src/camera.h
    src/utils/gl_mesh.h
    src/terrain/voxel_chunk.cpp src/terrain/voxel_chunk.h
    src/particles/particle.h
    src/particles/particlesystem.cpp
    src/particles/particlesystem.h

    # Sky textures - Rainy
    resources/textures/sky/Rainy/back.jpg
    resources/textures/sky/Rainy/bottom.jpg
    resources/textures/sky/Rainy/front.jpg
    resources/textures/sky/Rainy/left.jpg
    resources/textures/sky/Rainy/right.jpg
    resources/textures/sky/Rainy/top.jpg

    # Sky textures - Sunny
    resources/textures/sky/Sunny/Back.bmp
    resources/textures/sky/Sunny/Bottom.bmp
    resources/textures/sky/Sunny/Front.bmp
    resources/textures/sky/Sunny/Left.bmp
    resources/textures/sky/Sunny/Right.bmp
    resources/textures/sky/Sunny/Top.bmp

    # # ====== terrian / postprocessing ======
    # src/terrain/voxel_chunk.h
    # src/terrain/voxel_chunk.cpp
    src/terrain/terraingenerator.h src/terrain/terraingenerator.cpp
    src/vegetation/lsystem_tree.h src/vegetation/lsystem_tree.cpp
    src/particles/particle.h
    src/particles/particlesystem.h
    src/particles/particlesystem.cpp
    README.md
    resources/shaders/default.frag resources/shaders/default.vert resources/shaders/forest.frag resources/shaders/forest.vert resources/shaders/post.frag resources/shaders/post.vert resources/shaders/sky.frag resources/shaders/sky.vert resources/shaders/terrain.frag resources/shaders/terrain.vert resources/shaders/water.frag resources/shaders/water.vert resources/textures/terrain/beach/albedo.jpg resources/textures/terrain/beach/ao.jpg resources/textures/terrain/beach/displacement.jpg resources/textures/terrain/beach/normal.jpg resources/textures/terrain/beach/roughness.jpg resources/textures/terrain/beach/Sand_Fine_tdsmeeko_surface_Preview.png resources/textures/terrain/beach/tdsmeeko_2K_Displacement.exr resources/textures/terrain/grass/albedo.jpg resources/textures/terrain/grass/ao.jpg resources/textures/terrain/grass/displacement.jpg resources/textures/terrain/grass/normal.jpg resources/textures/terrain/grass/roughness.jpg resources/textures/terrain/grass/vb2mdatlw_2K_Displacement.exr resources/textures/terrain/rock/albedo.jpg resources/textures/terrain/rock/displacement.jpg resources/textures/terrain/rock/normal.jpg resources/textures/terrain/rock/roughness.jpg resources/textures/terrain/rock/vdyoaif_2K_AO.jpg resources/textures/terrain/rock/vdyoaif_2K_Displacement.exr resources/textures/terrain/rock_beach/albedo.jpg resources/textures/terrain/rock_beach/ao.jpg resources/textures/terrain/rock_beach/displacement.jpg resources/textures/terrain/rock_beach/normal.jpg resources/textures/terrain/rock_beach/roughness.jpg resources/textures/terrain/rock_beach/ulmiccvlw_2K_Displacement.exr resources/textures/terrain/snow/albedo.jpg resources/textures/terrain/snow/ao.jpg resources/textures/terrain/snow/displacement.jpg resources/textures/terrain/snow/normal.jpg resources/textures/terrain/snow/roughness.jpg resources/textures/terrain/snow/Snow_Mixed_vcqnfdk_surface_Preview.png resources/textures/terrain/snow/vcqnfdk_2K_Displacement.exr resources/textures/terrain/snow/vcqnfdk_2K_Transmission.jpg resources/textures/water_normal_tile.jpg

    # src/terrain/terrainsystem.cpp
    # src/terrain/terrainsystem.h

    # src/post/postprocess.cpp
    # src/post/postprocess.h

)

# GLM: this creates its library and allows you to `#include "glm/..."`
add_subdirectory(glm)

# GLEW: this creates its library and allows you to `#include "GL/glew.h"`
add_library(StaticGLEW STATIC glew/src/glew.c

)
include_directories(${PROJECT_NAME} PRIVATE glew/include)

# Specifies libraries to be linked (Qt components, glew, etc)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Core
    Qt::Gui
    Qt::OpenGL
    Qt::OpenGLWidgets
    Qt::Xml
    StaticGLEW
)

# Specifies other files
qt6_add_resources(${PROJECT_NAME} "Resources"
    PREFIX
        "/"
    FILES
        resources/shaders/default.frag
        resources/shaders/default.vert

        resources/textures/terrain/grass/albedo.jpg
        resources/textures/terrain/rock_beach/albedo.jpg
        resources/textures/terrain/beach/albedo.jpg
        resources/textures/terrain/rock/albedo.jpg
        resources/textures/terrain/snow/albedo.jpg

        resources/textures/terrain/grass/normal.jpg
        resources/textures/terrain/rock_beach/normal.jpg
        resources/textures/terrain/beach/normal.jpg
        resources/textures/terrain/rock/normal.jpg
        resources/textures/terrain/snow/normal.jpg
        resources/textures/water_normal_tile.jpg
        resources/textures/waterDUDV.png
        resources/textures/normalMap.png

        resources/textures/terrain/grass/roughness.jpg
        resources/textures/terrain/rock_beach/roughness.jpg
        resources/textures/terrain/beach/roughness.jpg
        resources/textures/terrain/rock/roughness.jpg
        resources/textures/terrain/snow/roughness.jpg
        resources/textures/terrain/rock_beach/displacement.jpg

        resources/shaders/terrain.frag
        resources/shaders/terrain.vert

        resources/shaders/forest.frag
        resources/shaders/forest.vert

        resources/shaders/water.frag
        resources/shaders/water.vert

        resources/shaders/sky.frag
        resources/shaders/sky.vert

        resources/shaders/post.frag
        resources/shaders/post.vert

        resources/shaders/particle.frag
        resources/shaders/particle.vert

        # Sky textures - Rainy
        resources/textures/sky/Rainy/back.jpg
        resources/textures/sky/Rainy/bottom.jpg
        resources/textures/sky/Rainy/front.jpg
        resources/textures/sky/Rainy/left.jpg
        resources/textures/sky/Rainy/right.jpg
        resources/textures/sky/Rainy/top.jpg

        # Sky textures - Sunny
        resources/textures/sky/Sunny/Back.bmp
        resources/textures/sky/Sunny/Bottom.bmp
        resources/textures/sky/Sunny/Front.bmp
        resources/textures/sky/Sunny/Left.bmp
        resources/textures/sky/Sunny/Right.bmp
        resources/textures/sky/Sunny/Top.bmp
)
# qt_add_resources(${PROJECT_NAME} "res"
#   PREFIX
#   "/"
#   FILES
#   resources/shaders/default.vert
#   resources/shaders/default.frag
# )

# GLEW: this provides support for Windows (including 64-bit)
if (WIN32)
  add_compile_definitions(GLEW_STATIC)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    opengl32
    glu32
  )
endif()

# Set this flag to silence warnings on Windows
if (MSVC OR MSYS OR MINGW)
  set(CMAKE_CXX_FLAGS "-Wno-volatile")
endif()
# Set this flag to silence warnings on MacOS
if (APPLE)
  set(CMAKE_CXX_FLAGS "-Wno-deprecated-volatile")
endif()
