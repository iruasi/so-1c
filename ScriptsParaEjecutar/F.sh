cd tp-2017-1c-Flanders-chip-y-asociados/Filesystem/ ; gcc -g3 -Wall -DFUSE_USE_VERSION=27 -D_FILE_OFFSET_BITS=64 fileSystem.c fileSystemConfigurators.c manejadorSadica.c operacionesFS.c -lfuse -lfuncionesPaquetes -lfuncionesCompartidas -lcommons -lm -lpthread -o ~/fs.out ; cd
