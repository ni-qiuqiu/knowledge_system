cmake_minimum_required(VERSION 3.15)
project(LLama_Chat VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 设置第三方库路径变量
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
set(CPPJIEBA_INCLUDE_DIR ${THIRD_PARTY_DIR}/cppjieba/include)

set(CPPJIEBA_DEP_DIR ${THIRD_PARTY_DIR}/cppjieba/deps/limonp/include)
set(CPPJIEBA_DICT_DIR ${THIRD_PARTY_DIR}/cppjieba/dict)
set(LLAMA_DIR ${THIRD_PARTY_DIR}/llama)
set(LLAMA_INCLUDE_DIR ${LLAMA_DIR}/include)
set(GGML_INCLUDE_DIR ${LLAMA_DIR}/ggml/include)
set(LLAMA_LIB_DIR ${LLAMA_DIR}/build/bin)


# 外部依赖

find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Eigen3 REQUIRED)

# 添加uuid库依赖
find_library(UUID_LIBRARY uuid REQUIRED)

# 添加 llama 库
add_subdirectory(${LLAMA_DIR} ${CMAKE_BINARY_DIR}/llama)
set(LLAMA_LIBRARY llama)

# 添加子目录
add_subdirectory(src)
# add_subdirectory(lib)

# 设置全局包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLAMA_INCLUDE_DIR}
    ${CPPJIEBA_DEP_DIR}
    ${CPPJIEBA_INCLUDE_DIR}
    ${GGML_INCLUDE_DIR}
    # ${THIRD_PARTY_DIR}
    ${Boost_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    # ${MYSQL_INCLUDE_DIR}
    # ${OPENSSL_INCLUDE_DIR}
)

# 设置输出目录
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/outputs/bin)

# # 设置库输出目录
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/outputs/lib)

# # 设置头文件输出目录
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/outputs/lib)

# 创建主可执行文件
add_executable(kb_system src/main.cpp)

# 链接库
target_link_libraries(kb_system
    PRIVATE
    kb_common
    kb_engine
    Threads::Threads
    OpenMP::OpenMP_CXX
    kb_preprocessor
    kb_vectorizer
    kb_knowledge
    kb_storage
    kb_query
    ${LLAMA_LIBRARY}
    # kb_optimization
    # kb_parallel
    # kb_api
    ${Boost_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# 查找外部依赖
# find_package(OpenSSL REQUIRED)
# find_package(CURL REQUIRED)
# find_package(nlohmann_json REQUIRED)

# 配置测试程序
add_executable(config_test tests/config_test.cpp)
target_link_libraries(config_test
    PRIVATE
    kb_common
    # nlohmann_json::nlohmann_json
)

# 安装配置测试程序
# install(TARGETS config_test
#     RUNTIME DESTINATION bin
# )

# 打印配置信息
message(STATUS "配置 LLama_Chat 项目 v${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}") 