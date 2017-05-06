deps_config := \
	/home/circuitects/esp/esp-idf/components/aws_iot/Kconfig \
	/home/circuitects/esp/esp-idf/components/bt/Kconfig \
	/home/circuitects/esp/esp-idf/components/esp32/Kconfig \
	/home/circuitects/esp/esp-idf/components/ethernet/Kconfig \
	/home/circuitects/esp/esp-idf/components/fatfs/Kconfig \
	/home/circuitects/esp/esp-idf/components/freertos/Kconfig \
	/home/circuitects/esp/esp-idf/components/log/Kconfig \
	/home/circuitects/esp/esp-idf/components/lwip/Kconfig \
	/home/circuitects/esp/esp-idf/components/mbedtls/Kconfig \
	/home/circuitects/esp/esp-idf/components/openssl/Kconfig \
	/home/circuitects/esp/esp-idf/components/spi_flash/Kconfig \
	/home/circuitects/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/circuitects/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/circuitects/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/circuitects/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
