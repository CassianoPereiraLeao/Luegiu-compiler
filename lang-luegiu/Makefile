# Configurações
CC = gcc
CFLAGS = -g
NASM = nasm
NASM_FLAGS = -f win64
LINKER = gcc
LINKER_FLAGS = -nostartfiles -e __entry 

COMPILADOR = luegiu.exe
FONTE_LUEGIU = ./luegiu/main.luegiu
ASSEMBLY = output.asm
OBJETO = output.obj
EXECUTAVEL_FINAL = programa.exe

all: build run assemble link clean_temp execute

build:
	$(CC) $(CFLAGS) ./*.c -o $(COMPILADOR)

run:
	./$(COMPILADOR) $(FONTE_LUEGIU) $(ASSEMBLY)

assemble:
	$(NASM) $(NASM_FLAGS) $(ASSEMBLY) -o $(OBJETO)

link:
	$(LINKER) $(LINKER_FLAGS) $(OBJETO) -o $(EXECUTAVEL_FINAL)

execute:
	$(EXECUTAVEL_FINAL) big

clean_temp:
	@del -f $(OBJETO) $(ASSEMBLY)

clean:
	@del -f $(COMPILADOR) $(OBJETO) $(ASSEMBLY) $(EXECUTAVEL_FINAL)