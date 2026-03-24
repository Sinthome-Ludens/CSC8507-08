cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(CSC8503)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################
set(CMAKE_CXX_STANDARD 20)

################################################################################
# Nuget packages function stub.
################################################################################
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Extra Defines
################################################################################
option(USE_VULKAN BOOL OFF)
option(USE_OPENGL BOOL ON)
option(USE_IMGUI "Enable ImGui" ON)
if(USE_IMGUI)
    add_compile_definitions("USE_IMGUI")
endif()

option(GAME_SHIPPING "Shipping build: disable debug panels" OFF)
if(GAME_SHIPPING)
    add_compile_definitions("GAME_SHIPPING")
endif()

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Platform Configuration
################################################################################
if(USE_VULKAN)
	find_package(Vulkan)
endif() 
if(GAME_SHIPPING)
    set(ASSET_ROOT "RUNTIME_RESOLVE" CACHE STRING "Asset root directory" FORCE)
    add_compile_definitions(ASSETROOTLOCATION="RUNTIME_RESOLVE")
    add_compile_definitions(ASSET_PATH_RUNTIME_RESOLVE)
else()
    set(ASSET_ROOT "${CMAKE_SOURCE_DIR}/Assets/" CACHE STRING "Asset root directory")
    add_compile_definitions(ASSETROOTLOCATION="${ASSET_ROOT}")
endif() 

if(USE_VULKAN)
    add_compile_definitions("USEVULKAN") 
	add_compile_definitions("VK_USE_PLATFORM_WIN32_KHR") 
endif() 

if(USE_OPENGL)
    add_compile_definitions("USEOPENGL") 
endif() 

if(MSVC)
    add_compile_definitions("NOMINMAX")
    add_compile_definitions("WIN32_LEAN_AND_MEAN")  
endif()

################################################################################
# External Libraries
################################################################################
# nlohmann_json (header-only)
include_directories(${CMAKE_SOURCE_DIR}/External/nlohmann_json/single_include)

# Jolt Physics - configure to use dynamic MSVC runtime to match project
set(USE_ASSERTS OFF CACHE BOOL "Enable asserts in Jolt" FORCE)
set(DOUBLE_PRECISION OFF CACHE BOOL "Use double precision in Jolt" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Enable LTO in Jolt" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "Cross platform deterministic in Jolt" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Override CXX flags in Jolt" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Enable all warnings in Jolt" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build Jolt Unit Tests" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "Build Jolt HelloWorld" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Build Jolt PerformanceTest" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "Build Jolt Samples" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "Build JoltViewer" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "Enable Jolt installation" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use static MSVC runtime in Jolt" FORCE)
add_subdirectory(External/JoltPhysics/Build)

# Assimp - configure to disable unnecessary features
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable Assimp tests" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable Assimp tools" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Disable Assimp samples" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp installation" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all importers by default" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all exporters" FORCE)

# Enable only needed importers
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "Enable OBJ importer" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "Enable FBX importer" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "Enable GLTF importer" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "Enable Collada importer" FORCE)
set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "Enable Blender importer" FORCE)

# Disable unnecessary Assimp features
set(ASSIMP_NO_EXPORT ON CACHE BOOL "Disable export functionality" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build zlib with Assimp" FORCE)

add_subdirectory(External/assimp)

# ImGui - Docking Branch
if(USE_IMGUI)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/External/imgui)
    file(GLOB IMGUI_SOURCES
        ${IMGUI_DIR}/*.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    add_library(ImGui STATIC ${IMGUI_SOURCES})
    target_include_directories(ImGui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(ImGui PRIVATE opengl32)
    target_compile_definitions(ImGui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/External/enet/CMakeLists.txt)
    add_subdirectory(External/enet)
    # The official enet CMakeLists.txt doesn't export include directories via target_include_directories
    target_include_directories(enet INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/enet/include)
    # Provide an alias 'ENet' to keep compatibility with existing CSC8503 targets if needed
    add_library(ENet ALIAS enet)
else()
    message(WARNING "External/enet not found. Please run the git clone command from README.md to fetch it.")
endif()

################################################################################
# Sub-projects
################################################################################
add_subdirectory(NCLCoreClasses)
add_subdirectory(CSC8503CoreClasses)
add_subdirectory(CSC8503)
add_subdirectory(GLTFLoader)

if(USE_VULKAN)
    add_subdirectory(VulkanRendering)
endif()
if(USE_OPENGL)
    add_subdirectory(OpenGLRendering)
endif() 

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CSC8503)
