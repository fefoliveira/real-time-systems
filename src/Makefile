export MDK := $(realpath ../mdk)
export ARCH := esp32c3
export PORT := /dev/ttyACM0

PROG        ?= nke
ARCH        ?= esp32c3
CFLAGS      ?= -W -Wall -Wextra -Wundef -Wshadow -pedantic \
               -Wdouble-promotion -fno-common -Wconversion \
               -march=rv32imc -mabi=ilp32 \
               -Os -ffunction-sections -fdata-sections -fno-builtin-printf\
               -I. -I$(MDK)/$(ARCH) -I$(MDK)/../include $(EXTRA_CFLAGS)
LINKFLAGS ?= -T$(MDK)/$(ARCH)/link.ld -nostdlib -lgcc -nostartfiles -Wl,--gc-sections,--undefined=isr1 $(EXTRA_LINKFLAGS)
SOURCES = NKE.c
CWD := $(realpath .)

DOCKER := sudo docker run -it --rm \
	-v $(CWD):$(CWD) \
	-v $(MDK):$(MDK) \
	-v $(MDK)/../include:$(MDK)/../include \
	-w $(CWD) \
	mdashnet/riscv

include $(MDK)/$(ARCH)/build.mk

#OBS: Se alterar *.h, faça make clean e depois make para recompilar tudo. (Isso é necessário porque o Makefile não tem dependências de arquivos .h)