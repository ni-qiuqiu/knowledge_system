#
# 添加模型引擎库
add_library(kb_engine STATIC
    model_interface.cpp
    llama_model.cpp
    engine_integration.cpp
)



# 设置包含目录
target_include_directories(kb_engine
    PUBLIC
    ${LLAMA_INCLUDE_DIR}
    ${GGML_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include

)

# 链接依赖库
target_link_libraries(kb_engine
    PUBLIC
    kb_common
)


# # 添加模型测试程序
# add_executable(model_test
#     model_test.cpp
# )

# 链接模型引擎库
# target_link_libraries(model_test
#     PRIVATE
#     kb_engine
# )

# # 拷贝测试程序到主目录
# set_target_properties(model_test
#     PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
# )