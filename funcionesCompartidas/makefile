RM=rm -rf
CC=gcc

C_SRCS=$(shell find . -iname "*.c" | tr '\n' ' ')
H_SRCS=$(shell find . -iname "*.h" | tr '\n' ' ')
OBJS=$(C_SRCS:./%.c=build/%.o)

# Clean and compile .so
all: build/libfuncionesCompartidas.so

create-dirs:
	mkdir -p build/funcionesCompartidas

build/libfuncionesCompartidas.so: create-dirs $(OBJS)
	$(CC)  -shared -Wall -o "build/libfuncionesCompartidas.so" $(OBJS)

build/funcionesCompartidas/%.o: funcionesCompartidas/%.c
	$(CC)  -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"

# Clean build files
clean:
	$(RM) build

install: all
	cp -u build/libfuncionesCompartidas.so /usr/lib
	cp --parents -u $(H_SRCS) /usr/include

uninstall:
	rm -f /usr/lib/libfuncionesCompartidas.so
	rm -rf /usr/include/funcionesCompartidas

.PHONY: all create-dirs clean install uninstall
