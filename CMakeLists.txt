cmake_minimum_required(VERSION 2.8)
set(CMAKE_VERBOSE_MAKEFILE ON)

project(SfMLocalization)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

MESSAGE("Build type: " ${CMAKE_BUILD_TYPE})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wall  -O3 -march=native ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall   -O3 -march=native")

# Check C++11 or C++0x support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
   add_definitions(-DCOMPILEDWITHC11)
   message(STATUS "Using flag -std=c++11.")
elseif(COMPILER_SUPPORTS_CXX0X)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
   add_definitions(-DCOMPILEDWITHC0X)
   message(STATUS "Using flag -std=c++0x.")
else()
   message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)

# use OpenCV 3.0, Eigen3, Ceres, OpenMVG
SET("OpenCV_DIR" "/opt/opencv-3.0.0/share/OpenCV")
SET("Ceres_DIR" "/usr/local/share/Ceres")
SET("OpenMVG_DIR" "/usr/local/share/openMVG/cmake")
find_package(OpenCV 3.0.0 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenMVG REQUIRED)
find_package(Ceres 1.10.0 REQUIRED)

# use OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

include_directories(
${PROJECT_SOURCE_DIR}/BoWCommon/src
${PROJECT_SOURCE_DIR}/VisionLocalizeCommon/src
${PROJECT_SOURCE_DIR}/VisionLocalizeBeaconCommon/src
${OpenCV_INCLUDE_DIRS}
${EIGEN3_INCLUDE_DIR}
${CERES_INCLUDE_DIRS}
${OPENMVG_INCLUDE_DIRS}
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${PROJECT_NAME} SHARED
BoWCommon/src/BoFSpatialPyramids.cpp
BoWCommon/src/BoFUtils.cpp
BoWCommon/src/DenseFeatureDetector.cpp
BoWCommon/src/DenseLocalFeatureWrapper.cpp
VisionLocalizeCommon/src/AKAZEOpenCV.cpp
VisionLocalizeCommon/src/AKAZEOption.cpp
VisionLocalizeCommon/src/FileUtils.cpp
VisionLocalizeCommon/src/MatUtils.cpp
VisionLocalizeCommon/src/MatchUtils.cpp
VisionLocalizeCommon/src/PcaWrapper.cpp
VisionLocalizeCommon/src/SfMDataUtils.cpp
VisionLocalizeCommon/src/StringUtils.cpp
VisionLocalizeBeaconCommon/src/BeaconUtils.cpp
)

target_link_libraries(${PROJECT_NAME}
${OpenCV_LIBS}
${EIGEN3_LIBS}
${CERES_LIBRARIES}
${OPENMVG_LIBRARIES}
)

# Build applications
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/ExtFeatAndMatch/Release)

add_executable(ExtFeatAndMatch
ExtFeatAndMatch/src/computeFeaturesAndMatches.cpp)
target_link_libraries(ExtFeatAndMatch ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/ExtractBoW/Release)

add_executable(ExtractBoW
ExtractBoW/src/ExtractBoW.cpp)
target_link_libraries(ExtractBoW ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/OpenMVGLocalization_AKAZE/Release)

add_executable(OpenMVGLocalization_AKAZE
OpenMVGLocalization_AKAZE/src/localization.cpp)
target_link_libraries(OpenMVGLocalization_AKAZE ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/OpenMVGLocalization_AKAZE_BeaconMatch/Release)

add_executable(OpenMVGLocalization_AKAZE_BeaconMatch
OpenMVGLocalization_AKAZE_BeaconMatch/src/localization.cpp)
target_link_libraries(OpenMVGLocalization_AKAZE_BeaconMatch ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/OpenMVG_BA/Release)

add_executable(OpenMVG_BA
OpenMVG_BA/src/adjust_sfm_data.cpp)
target_link_libraries(OpenMVG_BA ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/ReadBinaryMat/Release)

add_executable(ReadBinaryMat
ReadBinaryMat/src/ReadBinaryMat.cpp)
target_link_libraries(ReadBinaryMat ${PROJECT_NAME})

#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/TrainBoW/Release)

add_executable(TrainBoW
TrainBoW/src/TrainBoW.cpp)
target_link_libraries(TrainBoW ${PROJECT_NAME})