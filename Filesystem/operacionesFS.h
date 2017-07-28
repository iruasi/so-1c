#ifndef OPERACIONESFS_H_
#define OPERACIONESFS_H_

int open2(char *path);

int read2(char *path, char **buf, size_t size, off_t offset);

int write2(char * path, char * buf, size_t size, off_t offset);

int unlink2 (char *path);

int validarArchivo(char* path);

void iniciarBloques(int cant, char* path);
char** obtenerBloquesDisponibles(int cant);
void agregarBloquesSobreBloques(char ***bloques, char **blq_add);
void marcarBloquesOcupados(char **bloques);
int obtenerCantidadBloquesLibres(void);



#endif /* OPERACIONESFS_H_ */
