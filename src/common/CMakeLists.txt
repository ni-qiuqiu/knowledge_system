
# 公共实用函数库
add_library(kb_common STATIC
    config.cpp
    logging.cpp
    utils.cpp
)

target_include_directories(kb_common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 添加OpenSSL库依赖（用于SHA256）
target_link_libraries(kb_common
    PUBLIC
    crypto
)