deps_config := \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/app_trace/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/aws_iot/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/bt/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/driver/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/esp32/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/ethernet/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/fatfs/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/freertos/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/heap/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/libsodium/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/log/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/lwip/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/mbedtls/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/openssl/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/pthread/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/spi_flash/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/spiffs/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/wear_levelling/Kconfig \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/examples/RF24-demo/main/Kconfig.projbuild \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/mnt/0e474266-d3ad-4927-9269-607231afb799/GoogleDrive/InProg/Freelancing/Development/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
