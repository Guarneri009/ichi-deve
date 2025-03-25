cmake_minimum_required(VERSION 3.19.0)
message(STATUS "CMake version: ${CMAKE_VERSION}")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(main CXX)

file(GLOB cpu_source_files "${CMAKE_SOURCE_DIR}/*.cpp")
file(GLOB gpu_source_files "${CMAKE_SOURCE_DIR}/*.cu")

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

# For CUDA
#if(POLICY CMP0146)
#    cmake_policy(SET CMP0146 OLD)
#endif()

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

# CUDA固有のコンパイル設定
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_ARCHITECTURES "61")
# C++17とCUDA17を両方サポートするためのコンパイルオプション
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17 cuda_std_17)
