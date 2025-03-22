# 测试CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# 添加GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
)
# 不覆盖gtest_force_shared_crt
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 启用CTest
include(CTest)
enable_testing()

# 开启调试
set(CMAKE_BUILD_TYPE Debug)


# 设置包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/../include
    ${LLAMA_INCLUDE_DIR}
    ${CPPJIEBA_DEP_DIR}
    ${CPPJIEBA_INCLUDE_DIR}
    ${GGML_INCLUDE_DIR}
)

# 添加测试文件
add_executable(
    test_chinese_tokenizer
    test_chinese_tokenizer.cpp
)
target_link_libraries(
    test_chinese_tokenizer
    GTest::gtest_main
    kb_preprocessor
)

add_executable(
    test_text_cleaner
    test_text_cleaner.cpp
)
target_link_libraries(
    test_text_cleaner
    GTest::gtest_main
    kb_preprocessor
)

add_executable(
    test_text_splitter
    test_text_splitter.cpp
)
target_link_libraries(
    test_text_splitter
    GTest::gtest_main
    kb_preprocessor
)

add_executable(
    test_metadata_extractor
    test_metadata_extractor.cpp
)
target_link_libraries(
    test_metadata_extractor
    GTest::gtest_main
    kb_preprocessor
)

add_executable(
    test_preprocessing_pipeline
    test_preprocessing_pipeline.cpp
)
target_link_libraries(
    test_preprocessing_pipeline
    GTest::gtest_main
    kb_preprocessor
)

add_executable(test_ml_extractor
    test_ml_extractor.cpp
)

target_link_libraries(test_ml_extractor
    PRIVATE
    kb_extractor
    kb_common
    GTest::gtest
    GTest::gtest_main
)

# 添加存储模块测试
add_executable(test_storage_manager
    test_storage_manager.cpp
)

target_link_libraries(test_storage_manager
    PRIVATE
    kb_storage
    kb_knowledge
    kb_common
    GTest::gtest
    GTest::gtest_main
)

# 添加MySQL存储测试
add_executable(test_mysql_storage
    test_mysql_storage.cpp
)

target_link_libraries(test_mysql_storage
    PRIVATE
    kb_storage
    kb_knowledge
    kb_common
    GTest::gtest
    GTest::gtest_main
)


add_executable(test_model
    model_test.cpp
)

target_link_libraries(test_model
    PRIVATE
    kb_engine
    kb_common
    llama
   
)
add_executable(test_query
    test_query.cpp
)

target_link_libraries(test_query
    PRIVATE
    kb_query
    kb_common
    kb_knowledge
    kb_engine
    kb_storage
    
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME test_ml_extractor COMMAND test_ml_extractor)
add_test(NAME test_storage_manager COMMAND test_storage_manager)
add_test(NAME test_mysql_storage COMMAND test_mysql_storage)

# 注册测试
include(GoogleTest)
gtest_discover_tests(test_chinese_tokenizer)
gtest_discover_tests(test_text_cleaner)
gtest_discover_tests(test_text_splitter)
gtest_discover_tests(test_metadata_extractor)
gtest_discover_tests(test_preprocessing_pipeline)
gtest_discover_tests(test_storage_manager)
gtest_discover_tests(test_mysql_storage)