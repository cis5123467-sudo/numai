CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
VERSION ?= 0.1.3-r1
BUILD = build/$(VERSION)
RELEASE_DIR = releases
APP = $(RELEASE_DIR)/numai-$(VERSION).nwa
SOURCES = \
	src/app/main.c \
	src/calculation/cas.c \
	src/courses/topics.c \
	src/science/physics.c \
	src/science/chemistry.c \
	src/science/units.c \
	src/calculation/solvers.c \
	src/ui/mathview.c \
	src/platform/scriptstore.c
OBJECTS = $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/icon.o
DEVICE_FLAGS = -mthumb -mfloat-abi=hard -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -DPLATFORM_DEVICE=1

.PHONY: build check clean
build: $(APP)

$(BUILD):
	mkdir -p $@

$(RELEASE_DIR):
	mkdir -p $@

$(BUILD)/eadk.h: | $(BUILD)
	VISUAL=cat $(NWLINK) view-eadk-h > $@.tmp
	mv $@.tmp $@

$(BUILD)/%.o: src/%.c Makefile $(BUILD)/eadk.h
	mkdir -p $(@D)
	$(CC) $(DEVICE_FLAGS) -Isrc -I$(BUILD) -DNUMAI_VERSION='"$(VERSION)"' -std=c99 -Os -Wall -Wextra -Werror -fdata-sections -ffunction-sections -MMD -MP -c $< -o $@

$(BUILD)/icon.o: src/assets/icon.png | $(BUILD)
	$(NWLINK) png-icon-o $< $@

$(APP): $(OBJECTS) | $(RELEASE_DIR)
	$(CC) $(DEVICE_FLAGS) -Wl,--relocatable -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,-e,main,--gc-sections -Wl,-u,eadk_app_name,-u,eadk_app_icon,-u,eadk_api_level,-u,_printf_float $^ -lm -o $@.tmp
	mv $@.tmp $@

check: $(APP)
	$(NWLINK) nwa-elf $< $(BUILD)/numai.elf

clean:
	rm -rf build

-include $(OBJECTS:.o=.d)
