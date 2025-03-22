
# 查询模块 
set(QUERY_SOURCES
    query_parser.cpp
    graph_searcher.cpp
    answer_generator.cpp
    dialogue_manager.cpp
    query_manager.cpp
)

# 添加库
add_library(kb_query STATIC 
    ${QUERY_SOURCES}
    )

# 包含路径
target_include_directories(kb_query PUBLIC
    ${CMAKE_SOURCE_DIR}/../../include
    ${EIGEN3_INCLUDE_DIR}
)

# 链接依赖
target_link_libraries(kb_query PUBLIC
    kb_common
    kb_knowledge
    kb_engine
    kb_storage
    Eigen3::Eigen
)


