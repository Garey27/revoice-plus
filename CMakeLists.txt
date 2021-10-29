cmake_minimum_required(VERSION 3.15)
project(revoice_plus)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake-version4git)

PROJECT_VERSION_FROM_GIT()
add_definitions(${PROJECT_VERSION4GIT_CFLAGS})

set(CMAKE_CXX_STANDARD 17)
set(BUILD_UTILS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(dep/opus)
add_subdirectory(dep/silk)
add_subdirectory(dep/speex)
add_subdirectory(dep/soxr)


if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC") 
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set( CMAKE_SHARED_LINKER_FLAGS "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8" )
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fpermissive -w -O3")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -w -O3")
endif()


set(REVP_INCLUDE_DIRS
	dep/rehlsdk/common 
	dep/rehlsdk/engine 
	dep/rehlsdk/dlls
	dep/rehlsdk/public
	dep/metamod
	revoice/public
	revoice/src
	dep/silk
	dep/opus/include
	dep/speex/speex	
	dep/soxr/src
	)

set(REVP_SOURCES
	revoice/src/dllapi.cpp
	revoice/src/engine_api.cpp
	revoice/src/h_export.cpp
	revoice/src/meta_api.cpp
	revoice/src/precompiled.cpp
	revoice/src/public_amalgamation.cpp
	revoice/src/revoice_cfg.cpp
	revoice/src/revoice_main.cpp
	revoice/src/revoice_api.cpp
	revoice/src/revoice_player.cpp
	revoice/src/revoice_rehlds_api.cpp
	revoice/src/revoice_reunion_api.cpp
	revoice/src/revoice_utils.cpp
	revoice/src/sdk_util.cpp
	revoice/src/SteamP2PCodec.cpp
	revoice/src/voice_codec_frame.cpp
	revoice/src/VoiceEncoder_Opus.cpp
	revoice/src/VoiceEncoder_Silk.cpp
	revoice/src/VoiceEncoder_Speex.cpp
	revoice/public/utlbuffer.cpp
	)
add_library(revoice_plus SHARED ${REVP_SOURCES})
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC") 
set(CMAKE_CXX_FLAGS_DEBUG "/nologo /Od /Oi- /Ob0 /GS /ZI /await")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/nologo /debug:fastlink /incremental /editandcontinue")
target_link_options(revoice_plus PRIVATE /MACHINE:X86 /EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8)
endif()

target_link_libraries(revoice_plus PRIVATE Opus SILK Speex soxr 	
 	-static-libstdc++ 
	-static-intel -no-intel-extensions   
	-ldl -lm)
target_include_directories(revoice_plus PRIVATE ${REVP_INCLUDE_DIRS})
		
set_target_properties(revoice_plus PROPERTIES PREFIX "")
set_target_properties(revoice_plus PROPERTIES OUTPUT_NAME "revoice_plus_mm")
