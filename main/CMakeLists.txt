idf_component_register(SRCS "main.c")
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
esptool_merge_bin(${CONFIG_IDF_TARGET})
