#
# Copyright (c) 2015, Martin Jaros <xjaros32@stud.feec.vutbr.cz>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

#
# Copyright (c) 2018, Piyush Itankar <pitankar@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# Replace the Project name
PROJECT = example

# ICDI port
ICDI    = /dev/ttyACM0

# OpenOCD specific variables
OPENOC_CONFIG_FILE =/usr/local/share/openocd/scripts/board/ek-lm4f120xl.cfg
OPENOC_FILE_UPLOAD ="program $(PROJECT).hex verify reset exit"

# Source code related information
SOURCE  = ./src/
INCLUDE = ./inc/

# ARM compilation tools
ARM_GDB   = arm-none-eabi-gdb
ARM_GCC   = arm-none-eabi-gcc
ARM_LD    = arm-none-eabi-ld
ARM_STRIP = arm-none-eabi-strip

# Compilation specific variables
CFLAGS   ?= -mcpu=cortex-m4 -march=armv7e-m -mthumb -mtune=cortex-m4
CFLAGS   += -mno-thumb-interwork -mfloat-abi=softfp -mfpu=fpv4-sp-d16
CFLAGS   += -ggdb -std=c99 -O1 -I$(INCLUDE) -c -o
LDSCRIPT = ./.msc/scatter.ld
LDFLAGS  ?= --gc-sections -T $(LDSCRIPT) --entry reset_handler -o

.PHONY: all clean

# Triggers generation of the Final .hex file to be uploaded to the board
all: $(PROJECT).hex

# Cleans the project
clean:
	rm -rf ./*/*.d ./*/*.o *.elf *.hex .msc/openocd

# Generates the .elf file to be used for debugging
$(PROJECT)_dbg.elf: $(patsubst %.c, %.o, $(wildcard $(SOURCE)*.c))
	$(ARM_LD) ${LDFLAGS} $(PROJECT)_dbg.elf $^

# Generates the .hex file from .elf file by stripping the debug symbols
$(PROJECT).hex: $(PROJECT)_dbg.elf
	$(ARM_STRIP) -s $(PROJECT)_dbg.elf -o $(PROJECT).hex

-include $(wildcard *.d)

# For every .c file generates a .o file
%.o: %.c
	$(ARM_GCC) ${CFLAGS} $@ $<

# Uploads the .hex file to the board
upload: $(PROJECT).hex
	sudo openocd --file $(OPENOC_CONFIG_FILE) -c $(OPENOC_FILE_UPLOAD)

# Start a OpenOCD debug server
openOCD:
	sudo openocd --file $(OPENOC_CONFIG_FILE)

# Launches the ARM GDB for debugging
debug:
	$(ARM_GDB)

# Setup the openOCD environment used for uploading and debugging
setup:
	.msc/setup_openocd.sh