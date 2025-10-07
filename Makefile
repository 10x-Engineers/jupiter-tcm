CC := clang
OBJDUMP := objdump
AR := ar

CFLAGS  =-march=rv64gcv -mabi=lp64d
CFLAGS += -I./
CFLAGS += -Itcm/
CFLAGS += -Iudma/

TCM_SRC=tcm/tcm.c
DMA_SRC=udma/udma.c
AIMM_SRC=aimm.c

CSRC= main.c

all:
	@echo Build...
	@$(CC) -o tcm.o -c $(TCM_SRC) $(CFLAGS)
	@$(CC) -o udma.o -c $(DMA_SRC) $(CFLAGS)
	@$(CC) -o aimm.o -c $(AIMM_SRC) $(CFLAGS)
	@$(AR) rcs libaimm.a aimm.o udma.o tcm.o

	@$(CC) -g -Og -o app.elf $(CSRC) $(CFLAGS) -static -lpthread -L. -laimm

clean:
	rm -f *.o;
	rm -f *.out;
	rm -f *.elf;
	rm -f *.i;
	rm -f *.s;
	rm -f *.a;
	rm -f *.so;
