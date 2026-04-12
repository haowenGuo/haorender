cmake_minimum_required(VERSION 3.10)
project(myrender)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(OpenMP)
find_package(assimp CONFIG QUIET)
find_path(EIGEN3_INCLUDE_DIR NAMES Eigen/Core)
set(ASSIMP_INCLUDE_DIR "" CACHE PATH "Path to a complete Assimp include directory")
option(HAO_RENDER_DEPTH_HALF "Use Eigen::half for z-buffer and shadow depth buffers" OFF)
option(HAO_RENDER_VERTEX_HALF "Store loaded mesh vertex attributes as Eigen::half" OFF)

if(NOT (ASSIMP_INCLUDE_DIR
	AND EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/config.h"
	AND EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/Importer.hpp"))
	find_path(ASSIMP_IMPORTER_INCLUDE_DIR NAMES assimp/Importer.hpp)
	find_path(ASSIMP_CONFIG_INCLUDE_DIR NAMES assimp/config.h)
	if(ASSIMP_IMPORTER_INCLUDE_DIR AND EXISTS "${ASSIMP_IMPORTER_INCLUDE_DIR}/assimp/config.h")
		set(ASSIMP_INCLUDE_DIR "${ASSIMP_IMPORTER_INCLUDE_DIR}" CACHE PATH "Path to a complete Assimp include directory" FORCE)
	elseif(ASSIMP_CONFIG_INCLUDE_DIR AND EXISTS "${ASSIMP_CONFIG_INCLUDE_DIR}/assimp/Importer.hpp")
		set(ASSIMP_INCLUDE_DIR "${ASSIMP_CONFIG_INCLUDE_DIR}" CACHE PATH "Path to a complete Assimp include directory" FORCE)
	endif()
endif()

if(NOT EIGEN3_INCLUDE_DIR)
	message(FATAL_ERROR "Complete Eigen headers were not found. Pass -DEIGEN3_INCLUDE_DIR=/path/to/eigen3 or install Eigen.")
endif()

if(NOT ASSIMP_INCLUDE_DIR)
	message(FATAL_ERROR "Complete Assimp headers were not found. Pass -DASSIMP_INCLUDE_DIR=/path/to/assimp/include or install Assimp.")
endif()

add_executable(myrender main.cpp tgaimage.cpp Drawer.cpp shader.cpp ppp.cpp render.cpp model.cpp)
target_include_directories(myrender PRIVATE
	${EIGEN3_INCLUDE_DIR}
	${ASSIMP_INCLUDE_DIR}
	${CMAKE_SOURCE_DIR}/include)
target_link_libraries(myrender PRIVATE ${OpenCV_LIBRARIES})

if(TARGET assimp::assimp)
	target_link_libraries(myrender PRIVATE assimp::assimp)
else()
	find_library(ASSIMP_LIBRARY NAMES assimp assimp-vc143-mtd)
	if(ASSIMP_LIBRARY)
		target_link_libraries(myrender PRIVATE ${ASSIMP_LIBRARY})
	else()
		message(FATAL_ERROR "Assimp import library was not found. Install Assimp or pass -Dassimp_DIR=/path/to/assimp/cmake.")
	endif()
endif()

if(OpenMP_CXX_FOUND)
	target_link_libraries(myrender PRIVATE OpenMP::OpenMP_CXX)
endif()

if(HAO_RENDER_DEPTH_HALF)
	target_compile_definitions(myrender PRIVATE HAO_RENDER_DEPTH_HALF)
endif()

if(HAO_RENDER_VERTEX_HALF)
	target_compile_definitions(myrender PRIVATE HAO_RENDER_VERTEX_HALF)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assimp-vc143-mtd.dll")
	add_custom_command(TARGET myrender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_SOURCE_DIR}/assimp-vc143-mtd.dll"
			"$<TARGET_FILE_DIR:myrender>/assimp-vc143-mtd.dll")
endif()

file(GLOB OPENCV_RUNTIME_DLLS
	"${OpenCV_DIR}/x64/vc*/bin/opencv_world*.dll"
	"${OpenCV_DIR}/../x64/vc*/bin/opencv_world*.dll")
foreach(OPENCV_RUNTIME_DLL IN LISTS OPENCV_RUNTIME_DLLS)
	add_custom_command(TARGET myrender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${OPENCV_RUNTIME_DLL}"
			"$<TARGET_FILE_DIR:myrender>")
endforeach()
