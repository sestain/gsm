.SILENT:

PACKAGE = GSM
EE_BIN = $(PACKAGE).ELF
EE_OBJS = main.o gsm_engine.o gsm_api.o dve_reg.o

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include -Iinclude -I.
EE_CFLAGS = -D_EE -Os -G0 -Wall $(EE_INCS)

EE_LDFLAGS = -nostartfiles -nostdlib -Tlinkfile -L$(PS2SDK)/ee/lib -s

all: $(EE_BIN)
	 rm -f 'uncompressed $(PACKAGE).ELF'
	 mv $(PACKAGE).ELF 'uncompressed $(PACKAGE).ELF'
	 ee-strip 'uncompressed $(PACKAGE).ELF'
	 ps2-packer 'uncompressed $(PACKAGE).ELF' $(PACKAGE).ELF > /dev/null

clean:
	rm -f *.ELF *.o *.a *.s *.i *.map

rebuild:clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
