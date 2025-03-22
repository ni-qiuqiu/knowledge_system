# 创建预处理器库
add_library(kb_preprocessor STATIC
    document_loader.cpp
    text_cleaner.cpp
    chinese_tokenizer.cpp
    text_splitter.cpp
    metadata_extractor.cpp
    preprocessing_pipeline.cpp
    # 将来可以添加更多源文件
)

# 设置包含目录
target_include_directories(kb_preprocessor
    PUBLIC
    ${CPPJIEBA_DEP_DIR}
    ${CPPJIEBA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 设置链接库
target_link_libraries(kb_preprocessor
    PUBLIC
    kb_common
    uuid
    boost_filesystem
    boost_system
)

# 可以为预处理器设置特定编译选项
target_compile_options(kb_preprocessor
    PRIVATE
    -Wall -Wextra
) 