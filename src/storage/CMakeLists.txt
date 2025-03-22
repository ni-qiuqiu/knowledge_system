# 存储模块的 CMakeLists.txt 文件

# 存储模块配置
# 寻找MySQL库
find_package(MySQL QUIET)

# 如果没找到MySQL，尝试手动定义
if(NOT MySQL_FOUND)
    # 尝试找到mysql.h
    find_path(MySQL_INCLUDE_DIR
        NAMES mysql.h
        PATHS /usr/include/mysql /usr/local/include/mysql /usr/mysql/include/mysql
    )
    
    # 尝试找到libmysqlclient
    find_library(MySQL_LIBRARY
        NAMES mysqlclient
        PATHS /usr/lib /usr/local/lib /usr/mysql/lib
    )
    
    # 如果找到了头文件和库文件
    if(MySQL_INCLUDE_DIR AND MySQL_LIBRARY)
        set(MySQL_FOUND 1)
        set(MySQL_LIBRARIES ${MySQL_LIBRARY})
        set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIR})
        
        # 为了与find_package保持一致，创建导入目标
        add_library(MySQL::MySQL UNKNOWN IMPORTED)
        set_target_properties(MySQL::MySQL PROPERTIES
            IMPORTED_LOCATION "${MySQL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MySQL_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "未找到MySQL库。请安装MySQL开发包或指定MySQL库的位置。")
    endif()
endif()

# 存储库
add_library(kb_storage STATIC
    storage_manager.cpp
    mysql_storage.cpp
    storage_factory.cpp
    mysql_connection_pool.cpp
    storage_cache.cpp
    batch_operations.cpp
    version_control.cpp
)

target_include_directories(kb_storage
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${MySQL_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(kb_storage
    PUBLIC
    kb_common
    kb_knowledge
    ${MySQL_LIBRARIES}
)

# 编译选项
target_compile_options(kb_storage PRIVATE -Wall -Wextra)

# 为mysql_storage.cpp文件特别添加MySQL头文件包含路径
set_source_files_properties(mysql_storage.cpp PROPERTIES
    COMPILE_FLAGS "-I${MySQL_INCLUDE_DIRS}"
)

# 安装规则
install(TARGETS kb_storage
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/storage/
    DESTINATION include/storage
    FILES_MATCHING PATTERN "*.h"
)