idf_component_register(
    SRCS "mos_app_messages.c" "mos_app_settings.c" "mos_app_store.c"
    INCLUDE_DIRS "include"
    REQUIRES mission_link mos_shell
)
