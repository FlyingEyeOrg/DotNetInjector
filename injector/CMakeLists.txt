cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(injector LANGUAGES C CXX)

# 包含所有源代码 CPP 文件
file(GLOB_RECURSE CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# 打印文件列表（调试用）
message(STATUS "Found CPP files: ${CPP_FILES}")

add_executable(${PROJECT_NAME} ${CPP_FILES})

# 包含所有资源文件
file(GLOB_RECURSE RC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*.rc")
file(GLOB_RECURSE ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*.*")

# 资源文件变更，触发主函数重新编译
set_source_files_properties(src/main.cpp PROPERTIES
    OBJECT_DEPENDS "${ASSET_FILES}"
)

# 添加资源依赖，将会设置 exe 图标
target_sources(${PROJECT_NAME} PRIVATE ${RC_FILES})

# 链接必要的 Windows 库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    comctl32      # 通用控件库
    user32        # 用户界面函数
    gdi32         # 图形设备接口
    kernel32      # 核心系统函数
)

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)