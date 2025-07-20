idf_component_register(SRCS "main.cpp"
                            "board.cpp"
                            "app.cpp"
                            "blufi.cpp"
                            "http_handlers.cpp"
                            "utils.cpp"
                    INCLUDE_DIRS 
                            "."
                    EMBED_TXTFILES www/dist/index.html.gz
                            )

                            
add_custom_target(www DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/dist/index.html.gz)

add_dependencies(${COMPONENT_LIB} www)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 20)

# fixes the issue with GCC type mismatch error even their size is the same:
# https://github.com/espressif/esp-idf/issues/9511
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security;-Wformat=0" APPEND)
