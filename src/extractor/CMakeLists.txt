# 知识提取模块CMake配置

# 创建知识提取库
add_library(kb_extractor STATIC
    extractor_manager.cpp
    rule_based_extractor.cpp
    ml_extractor.cpp
    # 将来可以添加更多源文件
)

# 设置包含目录
target_include_directories(kb_extractor
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 设置链接库
target_link_libraries(kb_extractor
    PUBLIC
    kb_common
    kb_preprocessor
    kb_vectorizer
    kb_knowledge
)

# 为知识提取模块设置特定编译选项
target_compile_options(kb_extractor
    PRIVATE
    -Wall -Wextra
) 