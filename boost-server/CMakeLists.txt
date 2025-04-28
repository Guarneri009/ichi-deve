cmake_minimum_required(VERSION 3.19.0)
message(STATUS "CMake version: ${CMAKE_VERSION}")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(main CXX)

file(GLOB cpu_source_files "${CMAKE_SOURCE_DIR}/*.cpp")
file(GLOB gpu_source_files "${CMAKE_SOURCE_DIR}/*.cu")

find_package(Boost REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost version: ${Boost_VERSION}")
    message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Boost not found")
endif()

# OpenMPを有効にするかどうかを選択するオプション (デフォルトではOFF)
option(ENABLchE_OPENMP "Enable OpenMP support" OFF)

# For OpenMP
if (ENABLE_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_FOUND)
        message(STATUS "OpenMP found: ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    else()
        message(WARNING "OpenMP not found, but ENABLE_OPENMP was ON")
    endif()
else()
    message(STATUS "OpenMP support disabled.")
endif()


find_package(Boost REQUIRED COMPONENTS system url)


find_package(CUDAToolkit  QUIET)
if(CUDA_FOUND)
    message("Cuda version: " ${CUDA_VERSION})
    message(STATUS "CUDA detected: ${CMAKE_CUDA_COMPILER}")
    enable_language(CUDA)
    add_executable(${PROJECT_NAME} ${cpu_source_files} ${gpu_source_files})
else()
    add_executable(${PROJECT_NAME} ${cpu_source_files})
endif()

# コンパイル時にCUDAが定義されるように設定
if(CUDA_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC __CUDACC__)
endif()

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME}
    PRIVATE Threads::Threads
    Boost::system Boost::url
)


# For CUDA
#if(POLICY CMP0146)
#    cmake_policy(SET CMP0146 OLD)
#endif()

# CUDA固有のコンパイル設定 GTX1070のアーキテクチャを指定
# https://developer.nvidia.com/cuda-gpus
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_ARCHITECTURES "61")
# C++20とCUDA20を両方サポートするためのコンパイルオプション
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20 cuda_std_20)
