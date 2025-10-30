# 编译当前目录里所有 libpng 源文件，公开头文件目录给其它组件
file(GLOB LIBPNG_SRCS "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${LIBPNG_SRCS}
    INCLUDE_DIRS .          # 对外公开 png.h
    PRIV_INCLUDE_DIRS .     # 自身内部也需要 pngpriv.h 等
    REQUIRES zlib           # 依赖 ESP-IDF 自带 zlib 组件
)