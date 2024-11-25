idf_component_register(SRCS "main.c")

# config storage image version
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/../storage_version.ota.in ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs/version.ota)
#spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
spiffs_create_partition_image(storage ../spiffs)

# Add  CONFIG_IDF_TARGET and PROJECT_VER to binary file name
esptool_package_firmware("${CMAKE_PROJECT_NAME}_firmware_${CONFIG_IDF_TARGET}_${PROJECT_VER}")
esptool_package_storage("${CMAKE_PROJECT_NAME}_storage_${CONFIG_IDF_TARGET}_${PROJECT_VER}")

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_NAME=\"${CMAKE_PROJECT_NAME}\"")
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_VERSION=\"${PROJECT_VER}\"")
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DPROJECT_TARGET=\"${CONFIG_IDF_TARGET}\"")

# merge all binary files
esptool_merge_bin("${CMAKE_PROJECT_NAME}_merged_${CONFIG_IDF_TARGET}_${PROJECT_VER}")