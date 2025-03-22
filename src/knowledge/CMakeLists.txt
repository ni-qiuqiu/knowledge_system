# 知识表示模块CMake配置

# 创建知识表示库
add_library(kb_knowledge STATIC
    entity.cpp
    relation.cpp
    triple.cpp
    knowledge_graph.cpp
    # 将来可以添加更多源文件
)

# 设置包含目录
target_include_directories(kb_knowledge
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 设置链接库
target_link_libraries(kb_knowledge
    PUBLIC
    kb_common
    kb_vectorizer
    Eigen3::Eigen
)

# 为知识表示模块设置特定编译选项
target_compile_options(kb_knowledge
    PRIVATE
    -Wall -Wextra
) 