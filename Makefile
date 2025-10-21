CC := clang
OBJDUMP := objdump
AR := ar

CFLAGS  = -march=rv64gcv -mabi=lp64d
CFLAGS += -I./
CFLAGS += -Itcm/
CFLAGS += -Iudma/

TCM_SRC = tcm/tcm.c
DMA_SRC = udma/udma.c
AIMM_SRC = aimm.c

CSRC ?= multicore_matmul.c

TARGET = app.elf
LIBAIMM = libaimm.a

all:
	@echo "Building with test source: $(CSRC)"
	@$(CC) -o tcm.o -c $(TCM_SRC) $(CFLAGS)
	@$(CC) -o udma.o -c $(DMA_SRC) $(CFLAGS)
	@$(CC) -o aimm.o -c $(AIMM_SRC) $(CFLAGS)
	@$(AR) rcs $(LIBAIMM) aimm.o udma.o tcm.o
	@$(CC) -g -Og -o $(TARGET) $(CSRC) $(CFLAGS) -static -lpthread -L. -laimm

clean:
	rm -f *.o *.out *.elf *.i *.s *.a *.so
