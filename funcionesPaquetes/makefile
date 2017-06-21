RM=rm -rf
CC=gcc

C_SRCS=$(shell find . -iname "*.c" | tr '\n' ' ')
H_SRCS=$(shell find . -iname "*.h" | tr '\n' ' ')
OBJS=$(C_SRCS:./%.c=build/%.o)

# Clean and compile .so
all: build/libfuncionesPaquetes.so

create-dirs:
	mkdir -p build/funcionesPaquetes

build/libfuncionesPaquetes.so: create-dirs $(OBJS)
	$(CC)  -shared -Wextra -Wall -o "build/libfuncionesPaquetes.so" $(OBJS)

build/funcionesPaquetes/%.o: funcionesPaquetes/%.c
	$(CC)  -c -Wall -Wextra -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"

# Clean build files
clean:
	$(RM) build

install: all
	cp -u build/libfuncionesPaquetes.so /usr/lib
	cp --parents -u $(H_SRCS) /usr/include

uninstall:
	rm -f /usr/lib/libfuncionesPaquetes.so
	rm -rf /usr/include/funcionesPaquetes

.PHONY: all create-dirs clean install uninstall
