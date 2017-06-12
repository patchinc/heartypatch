deps_config := \
	/Users/ashwin/esp/esp-idf/components/bt/Kconfig \
	/Users/ashwin/esp/esp-idf/components/esp32/Kconfig \
	/Users/ashwin/esp/esp-idf/components/ethernet/Kconfig \
	/Users/ashwin/esp/esp-idf/components/freertos/Kconfig \
	/Users/ashwin/esp/esp-idf/components/log/Kconfig \
	/Users/ashwin/esp/esp-idf/components/lwip/Kconfig \
	/Users/ashwin/esp/esp-idf/components/mbedtls/Kconfig \
	/Users/ashwin/esp/esp-idf/components/openssl/Kconfig \
	/Users/ashwin/esp/esp-idf/components/spi_flash/Kconfig \
	/Users/ashwin/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/ashwin/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/ashwin/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/ashwin/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
