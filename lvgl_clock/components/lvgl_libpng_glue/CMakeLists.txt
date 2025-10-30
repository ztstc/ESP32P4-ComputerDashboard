# 这个 glue 组件不编译任何功能代码，只负责把 libpng 作为 PUBLIC 依赖链接到 lvgl__lvgl，
# 使 lvgl__lvgl 的编译阶段继承 libpng 的 include 路径和编译选项
idf_component_register(
    SRCS "dummy.c"
    REQUIRES libpng lvgl__lvgl
)

# 找到 lvgl__lvgl 的目标并把 libpng 公开链接上去（传播 include 路径给 lvgl__lvgl 的编译）
idf_component_get_property(LVGL_TGT lvgl__lvgl COMPONENT_LIB)
if(LVGL_TGT)
    target_link_libraries(${LVGL_TGT} PUBLIC ${COMPONENT_LIB})
endif()