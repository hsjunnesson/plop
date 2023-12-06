cmake_minimum_required(VERSION 3.21)
project(plop
    VERSION 0.0.1
    DESCRIPTION "Plop is a game"
    HOMEPAGE_URL "http://plop.com"
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(LIVE_PP False)
set(SUPERLUMINAL False)

# Find locally installed dependencies. Tip: Use VCPKG for these.

if (SUPERLUMINAL)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "c:/Program Files/Superluminal/Performance/API")
    find_package(SuperluminalAPI REQUIRED)
endif()


# Fetch dependencies from Github

include(FetchContent)

FetchContent_Declare(
    BitSquidFoundation
    GIT_REPOSITORY https://github.com/hsjunnesson/bitsquid-foundation.git
)

if (NOT bitsquidfoundation_POPULATED)
    FetchContent_MakeAvailable(BitSquidFoundation)
endif()

add_subdirectory("${CMAKE_SOURCE_DIR}/chocolate")

set(WWISE_SDK "c:/Program Files (x86)/Audiokinetic/Wwise 2023.1.0.8367/SDK")
find_package(Wwise REQUIRED)


# Main game source

set(SRC_plop
    "src/main.cpp"
    "src/game.h"
    "src/game.cpp"
    "src/wwise.h"
    "src/wwise.cpp"
    "src/rnd.h"
    "src/util.h"
    "src/palette.h"
    "src/palette.cpp"
    "plop_wwise/GeneratedSoundBanks/Wwise_IDs.h"
)

set(SRC_AK
    "src/SoundEngine/Common/AkFileHelpersBase.h"
    "src/SoundEngine/Common/AkFileLocationBase.cpp"
    "src/SoundEngine/Common/AkFileLocationBase.h"
    "src/SoundEngine/Common/AkFilePackage.cpp"
    "src/SoundEngine/Common/AkFilePackage.h"
    "src/SoundEngine/Common/AkFilePackageLowLevelIO.h"
    "src/SoundEngine/Common/AkFilePackageLowLevelIO.inl"
    "src/SoundEngine/Common/AkFilePackageLowLevelIODeferred.h"
    "src/SoundEngine/Common/AkFilePackageLUT.cpp"
    "src/SoundEngine/Common/AkFilePackageLUT.h"
    "src/SoundEngine/Common/AkGeneratedSoundBanksResolver.cpp"
    "src/SoundEngine/Common/AkGeneratedSoundBanksResolver.h"
    "src/SoundEngine/Common/AkMultipleFileLocation.cpp"
    "src/SoundEngine/Common/AkMultipleFileLocation.h"
    "src/SoundEngine/Common/AkJobWorkerMgr.h"
    "src/SoundEngine/Common/AkJobWorkerMgr.cpp"
    "src/SoundEngine/Win32/AkDefaultIOHookDeferred.cpp"
    "src/SoundEngine/Win32/AkDefaultIOHookDeferred.h"
    "src/SoundEngine/Win32/AkFileHelpers.h"
)


# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_plop}
    ${SRC_AK}
)


# Includes

if (SUPERLUMINAL)
    include_directories(SYSTEM ${SuperluminalAPI_INCLUDE_DIRS})
endif()

if (LIVE_PP)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/LivePP/API/x64)
endif()

include_directories(SYSTEM ${Wwise_INCLUDE_DIR})
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/plop_wwise/GeneratedSoundBanks)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/SoundEngine)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/SoundEngine/Win32)


# Linked libraries

target_link_libraries(${PROJECT_NAME} PRIVATE chocolate)

if (SUPERLUMINAL)
    target_link_libraries(${PROJECT_NAME} PRIVATE SuperluminalAPI)
endif()

foreach(lib ${Wwise_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${lib})
endforeach()

# target_link_libraries(${PROJECT_NAME} PRIVATE debug ws2_32)

# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DAK_OPTIMIZED")

include(cmake/CompilerWarnings.cmake)
myproject_set_project_warnings(${PROJECT_NAME})

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(TARGET ${PROJECT_NAME} PROPERTY WIN32_EXECUTABLE TRUE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE=1)

    source_group("wwise" FILES ${SRC_AK})
    set_source_files_properties(${SRC_AK} PROPERTIES COMPILE_FLAGS "/Zc:wchar_t")

    if (LIVE_PP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIVE_PP=1 NOMINMAX=1)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi /Gm- /Gy /Gw)
        target_link_options(${PROJECT_NAME} PRIVATE /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF /DEBUG:FULL)
    endif()

    if (SUPERLUMINAL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE SUPERLUMINAL=1)
    endif()
endif()
