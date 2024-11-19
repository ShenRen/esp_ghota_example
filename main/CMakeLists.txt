idf_component_register(SRCS "main.c")
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

# Add  CONFIG_IDF_TARGET and PROJECT_VER to binary file name
# if(NOT PROJECT_VER)
#     git_describe(pro_version "${CMAKE_CURRENT_LIST_DIR}" "--match=v*.*.*") # "--match=v*.*.*"
#     message(STATUS "PROJECT_VER: ${pro_version}")
# else()
esptool_rename_bin("${CMAKE_PROJECT_NAME}_${CONFIG_IDF_TARGET}_${PROJECT_VER}")

# merge all binary files
esptool_merge_bin("${CMAKE_PROJECT_NAME}_${CONFIG_IDF_TARGET}_${PROJECT_VER}_merged")

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_NAME=\"${CMAKE_PROJECT_NAME}\"")
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_VERSION=\"${PROJECT_VER}\"")
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_TARGET=\"${CONFIG_IDF_TARGET}\"")