idf_component_register()

add_subdirectory(activecpp)

target_link_libraries(${COMPONENT_LIB} INTERFACE samiralavi_activecpp::samiralavi_activecpp)
