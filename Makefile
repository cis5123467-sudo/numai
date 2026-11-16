CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
VERSION ?= 0.0.4
BUILD = build/$(VERSION)
APP = ../numai-$(VERSION).nwa
OBJECTS = $(BUILD)/main.o $(BUILD)/cas.o $(BUILD)/topics.o $(BUILD)/physics.o $(BUILD)/icon.o
DEVICE_FLAGS = -mthumb -mfloat-abi=hard -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -DPLATFORM_DEVICE=1

.PHONY: build check clean
build: $(APP)

$(BUILD):
	mkdir -p $@

$(BUILD)/eadk.h: | $(BUILD)
	VISUAL=cat $(NWLINK) view-eadk-h > $@.tmp
	mv $@.tmp $@

$(BUILD)/%.o: src/%.c src/cas.h src/topics.h src/physics.h $(BUILD)/eadk.h
	$(CC) $(DEVICE_FLAGS) -I$(BUILD) -DNUMAI_VERSION='"$(VERSION)"' -std=c99 -Os -Wall -Wextra -Werror -fdata-sections -ffunction-sections -c $< -o $@

$(BUILD)/topics.o: src/aqa_catalog.inc src/maths_details.inc

$(BUILD)/icon.o: src/icon.png | $(BUILD)
	$(NWLINK) png-icon-o $< $@

$(APP): $(OBJECTS)
	$(CC) $(DEVICE_FLAGS) -Wl,--relocatable -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,-e,main,--gc-sections -Wl,-u,eadk_app_name,-u,eadk_app_icon,-u,eadk_api_level,-u,_printf_float $^ -lm -o $@.tmp
	mv $@.tmp $@

check: $(APP)
	$(NWLINK) nwa-elf $< $(BUILD)/numai.elf

clean:
	rm -rf build
