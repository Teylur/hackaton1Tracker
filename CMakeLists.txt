cmake_minimum_required(VERSION 3.18)
project(yolo_image_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Исполняемый файл всегда в build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ====================== GUI: ImGui + GLFW ======================
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")

add_library(ImGui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ImGui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# GLFW
if(WIN32)
    # Для Windows — положи GLFW в third_party/glfw
    set(GLFW_DIR "${CMAKE_SOURCE_DIR}/third_party/glfw")
    if(EXISTS ${GLFW_DIR})
        target_include_directories(ImGui PRIVATE ${GLFW_DIR}/include)
        link_directories(${GLFW_DIR}/lib-vc2022)   # или lib-vc2019 / lib-vc2017
        set(GLFW_LIB glfw3)
    else()
        message(WARNING "GLFW не найден в third_party/glfw. Скачай и положи туда.")
    endif()
else()
    # Для Linux
    find_package(glfw3 REQUIRED)
    set(GLFW_LIB glfw)
endif()

# ====================== OpenCV + ONNX (твоя оригинальная часть) ======================
set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH
    "ONNX Runtime root (include/ and lib/)")

# ---------------- OpenCV ----------------
if(WIN32)
    find_package(OpenCV REQUIRED)
    set(YOLO_OPENCV_INCLUDES ${OpenCV_INCLUDE_DIRS})
    set(YOLO_OPENCV_LIBS ${OpenCV_LIBS})
else()
    find_package(PkgConfig QUIET)
    set(OPENCV_FOUND FALSE)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPENCV opencv4)
    endif()
    if(OPENCV_FOUND)
        set(YOLO_OPENCV_INCLUDES ${OPENCV_INCLUDE_DIRS})
        set(YOLO_OPENCV_LIBS ${OPENCV_LIBRARIES})
        set(YOLO_OPENCV_LIB_DIRS ${OPENCV_LIBRARY_DIRS})
    else()
        find_package(OpenCV REQUIRED)
        set(YOLO_OPENCV_INCLUDES ${OpenCV_INCLUDE_DIRS})
        set(YOLO_OPENCV_LIBS ${OpenCV_LIBS})
    endif()
endif()

# ---------------- ONNX Runtime ----------------
if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found in ${ONNXRUNTIME_ROOT}/include")
endif()

if(WIN32)
    find_library(ONNXRUNTIME_LIB NAMES onnxruntime PATHS "${ONNXRUNTIME_ROOT}/lib" NO_DEFAULT_PATH)
    if(NOT ONNXRUNTIME_LIB)
        message(FATAL_ERROR "onnxruntime.lib not found...")
    endif()
else()
    file(GLOB ONNXRUNTIME_LIB_CANDIDATES "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so*")
    list(FILTER ONNXRUNTIME_LIB_CANDIDATES EXCLUDE REGEX "providers")
    if(NOT ONNXRUNTIME_LIB_CANDIDATES)
        message(FATAL_ERROR "libonnxruntime.so not found...")
    endif()
    list(GET ONNXRUNTIME_LIB_CANDIDATES 0 ONNXRUNTIME_LIB)
endif()

# ====================== Target ======================
add_executable(yolo_image_test 
    src/main.cpp
    # Добавляй сюда новые файлы по мере создания:
    # src/gui/ImGuiManager.cpp
    # src/gui/AlertSystem.cpp
    # src/gui/VideoPlayer.cpp
)

target_include_directories(yolo_image_test PRIVATE
    ${YOLO_OPENCV_INCLUDES}
    "${ONNXRUNTIME_ROOT}/include"
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(yolo_image_test PRIVATE
    ${YOLO_OPENCV_LIBS}
    ${ONNXRUNTIME_LIB}
    ImGui
    ${GLFW_LIB}
)

if(YOLO_OPENCV_LIB_DIRS)
    target_link_directories(yolo_image_test PRIVATE ${YOLO_OPENCV_LIB_DIRS})
endif()

if(NOT WIN32)
    target_link_options(yolo_image_test PRIVATE "-Wl,-rpath,${ONNXRUNTIME_ROOT}/lib")
endif()

# std::filesystem
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <filesystem>
    int main() { std::filesystem::path p(\"test\"); return 0; }
" HAS_FILESYSTEM)
if(NOT HAS_FILESYSTEM AND NOT MSVC)
    target_link_libraries(yolo_image_test PRIVATE stdc++fs)
endif()

# Копирование DLL (Windows)
if(WIN32)
    file(GLOB ORT_RUNTIME_DLLS "${ONNXRUNTIME_ROOT}/lib/*.dll")
    foreach(_dll ${ORT_RUNTIME_DLLS})
        add_custom_command(TARGET yolo_image_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:yolo_image_test>"
        )
    endforeach()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ONNX Runtime: ${ONNXRUNTIME_ROOT}")