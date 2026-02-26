set(PROJECT_NAME CSC8503)

################################################################################
# Source groups
################################################################################
#set(Header_Files
#    "GameTechRenderer.h"
#    "NetworkedGame.h"
#    "NetworkPlayer.h"
#    "StateGameObject.h"
#    "TutorialGame.h"
#)
file(GLOB Header_Files *.h)
source_group("Header Files" FILES ${Header_Files})



#set(Source_Files
#    "GameTechRenderer.cpp"
#    "Main.cpp"
#    "NetworkedGame.cpp"
#    "NetworkPlayer.cpp"
#    "StateGameObject.cpp"
#    "TutorialGame.cpp"
#)
file(GLOB Source_Files *.cpp)

# Collect ECS source files from Source directory
file(GLOB_RECURSE ECS_Header_Files ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
file(GLOB_RECURSE ECS_Source_Files ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp)
source_group("Source\\Headers" FILES ${ECS_Header_Files})
source_group("Source\\Sources" FILES ${ECS_Source_Files})

if(USE_VULKAN)
	file(GLOB SHADER_FILES ${ASSET_ROOT}/Shaders/VK/*.*)

    list(REMOVE_ITEM Header_Files ${CMAKE_CURRENT_SOURCE_DIR}/GameTechRenderer.h)
	list(REMOVE_ITEM Source_Files ${CMAKE_CURRENT_SOURCE_DIR}/GameTechRenderer.cpp)

else()
	list(REMOVE_ITEM Header_Files ${CMAKE_CURRENT_SOURCE_DIR}/GameTechVulkanRenderer.h)
	list(REMOVE_ITEM Source_Files ${CMAKE_CURRENT_SOURCE_DIR}/GameTechVulkanRenderer.cpp)
endif()

source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
    ${ECS_Header_Files}
    ${ECS_Source_Files}
)

if(USE_VULKAN)
    set(ALL_FILES 
        ${Header_Files}
        ${Source_Files}
        ${SHADER_FILES}
            Source/Game/Components/Res_Capsule.h
            Source/Game/Systems/Sys_EnemyAI.h
			Source/Game/Systems/Sys_EnemyAI.cpp
			Source/Game/Prefabs/PREFAB_ENEMY_TYPE01.cpp
			Source/Game/Prefabs/PREFAB_ENEMY_TYPE01.h
			Source/Game/Components/C_T_Enemy.h
			Source/Game/Components/C_D_AIState.h
			Source/Game/Components/C_D_AIPreception.h
			Source/Game/Components/Res_EnemyEnums.h
			Source/Game/Scenes/Scene_NavTest.cpp
			Source/Game/Scenes/Scene_NavTest.h
            Source/Game/Components/Res_NavTestState.h
    )

foreach (file ${SHADER_FILES})
    get_filename_component(file_name ${file} NAME)
	get_filename_component(file_ext ${file} EXT)
	
	if(file_ext STREQUAL ".h" OR file_ext STREQUAL ".cpp")
		continue()
	endif()
		
	if( file_ext STREQUAL  ".vert" OR
		file_ext STREQUAL  ".frag" OR
		file_ext STREQUAL  ".comp" OR
		file_ext STREQUAL  ".geom" OR
		file_ext STREQUAL  ".tesc" OR
		file_ext STREQUAL  ".tese" OR
		file_ext STREQUAL  ".rgen" OR
		file_ext STREQUAL  ".rint" OR
		file_ext STREQUAL  ".rahit" OR
		file_ext STREQUAL  ".rchit" OR
		file_ext STREQUAL  ".rmiss" OR
		file_ext STREQUAL  ".rcall" OR
		file_ext STREQUAL  ".task" OR
		file_ext STREQUAL  ".mesh"
	)
		message("Adding custom command to ${file}")
		get_filename_component(file_dir ${file} ABSOLUTE)
        set(SPIRV_OUTPUT ${file_name}.spv)
		set(SPIRV_ABS_INPUT ${file_dir})
		set(SPIRV_ABS_OUTPUT ${file_dir}.spv)
		
		add_custom_command(
			OUTPUT ${SPIRV_ABS_OUTPUT}
			
			COMMENT "Compiling GLSL shader:"
			COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V  ${SPIRV_ABS_INPUT} -o ${SPIRV_ABS_OUTPUT}
            DEPENDS ${file}
			VERBATIM
		)
        list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUTPUT})   
	endif()
endforeach()

endif()

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME}  ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE CSC8503)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
)

################################################################################
# Compile definitions
################################################################################
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "UNICODE;"
        "_UNICODE" 
        "WIN32_LEAN_AND_MEAN"
        "_WINSOCKAPI_"   
        "_WINSOCK2API_"
        "_WINSOCK_DEPRECATED_NO_WARNINGS"
    )
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE
    <vector>
    <map>
    <stack>
    <list>
	<set>
	<string>
    <thread>
    <atomic>
    <functional>
    <iostream>
	<chrono>
	<sstream>

	"../NCLCoreClasses/Vector.h"
    "../NCLCoreClasses/Quaternion.h"
    "../NCLCoreClasses/Plane.h"
    "../NCLCoreClasses/Matrix.h"
    "../NCLCoreClasses/GameTimer.h"

    <Jolt/Jolt.h>
    <nlohmann/json.hpp>
)

# 禁用 TestECS.cpp 的预编译头（避免与 LOG_INFO 宏冲突）
set_source_files_properties(TestECS.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
 
if(USE_VULKAN)
target_precompile_headers(${PROJECT_NAME} PRIVATE
    <vulkan/vulkan.hpp>
    <SmartTypes.h>
    <VulkanRenderer.h>
    #<VulkanShader.h>
    #<VulkanShaderBuilder.h>
    <VulkanTexture.h>
    <VulkanMesh.h>
    <VulkanPipelineBuilder.h>
    <VulkanDynamicRenderBuilder.h>

    <VulkanTextureBuilder.h>

    <VulkanDescriptorSetLayoutBuilder.h>
    <VulkanRenderPassBuilder.h>
    #<VulkanCompute.h>
    <VulkanComputePipelineBuilder.h>
	<VulkanBufferBuilder.h>
)
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
		/utf-8;
        $<$<CONFIG:Release>:
            /Oi;
            /Gy
        >
        /permissive-;
        /std:c++latest;
        /sdl;
        /W3;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF
        >
    )
endif()

################################################################################
# Dependencies
################################################################################
if(MSVC)
    target_link_libraries(${PROJECT_NAME} LINK_PUBLIC  "Winmm.lib")
endif()

include_directories("../NCLCoreClasses/")
include_directories("../CSC8503CoreClasses/")

# ECS Source directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC NCLCoreClasses)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC CSC8503CoreClasses)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ENet)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Jolt)

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC GLTFLoader)

# Link Assimp
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC assimp)
include_directories(${CMAKE_SOURCE_DIR}/External/assimp/include)

# Link ImGui
if(USE_IMGUI)
    target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ImGui)
    include_directories(${CMAKE_SOURCE_DIR}/External/imgui)
    include_directories(${CMAKE_SOURCE_DIR}/External/imgui/backends)
endif()

if(USE_OPENGL)
include_directories("../OpenGLRendering/")
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC OpenGLRendering)
endif()

if(USE_VULKAN)
    include_directories("../VulkanRendering/")
    target_link_libraries(${PROJECT_NAME} LINK_PUBLIC VulkanRendering)

    add_compile_definitions(VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

    add_compile_definitions(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
    add_compile_definitions(VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=1)

    add_compile_definitions(VK_NO_PROTOTYPES)

    add_custom_target(
        SHADER_FILES
        DEPENDS ON ${SPIRV_BINARY_FILES}
    )

    add_dependencies(${PROJECT_NAME} SHADER_FILES)
endif()