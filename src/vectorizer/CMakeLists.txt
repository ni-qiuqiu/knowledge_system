
# 向量化模块CMake配置

# 创建向量化器库
add_library(kb_vectorizer STATIC
    vectorizer.cpp
    tfidf_vectorizer.cpp
    # 将来可以添加更多源文件
)

# 设置包含目录
target_include_directories(kb_vectorizer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 设置链接库
target_link_libraries(kb_vectorizer
    PUBLIC
    kb_common
    Eigen3::Eigen
)

# 为向量化器设置特定编译选项
target_compile_options(kb_vectorizer
    PRIVATE
    -Wall -Wextra
) 

