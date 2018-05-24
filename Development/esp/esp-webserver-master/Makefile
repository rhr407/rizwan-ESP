IDF_PATH ?= $(abspath $(shell pwd)/../../esp-idf)
PROJECT_NAME := ESP32_IDF
# EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../../components/mongoose
EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../../components/

include $(IDF_PATH)/make/project.mk


SDK_VER=$(shell cat sdk.version)

.PHONY: all

MG_PATH = $(realpath $(PWD)/../../)

# all:
# 	docker run --rm -it -v $(MG_PATH):$(IDF_PATH) \
#                    $(SDK_VER) \
#                    bash -c "cd $(IDF_PATH)/examples/ESP32_IDF && \
#                    make -f Makefile.build defconfig && make -f Makefile.build"

# clean:
# 	docker run --rm -it -v $(MG_PATH):$(IDF_PATH)\
#                    $(SDK_VER) \
#                    bash -c "rm -rf $(IDF_PATH)/examples/ESP32_IDF/build"



