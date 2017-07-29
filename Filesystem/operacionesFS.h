#ifndef OPERACIONESFS_H_
#define OPERACIONESFS_H_

#ifndef MAX_AB
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif
#ifndef MIN_AB
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif


int open2(char *path);

char *read2(char *path, size_t size, off_t offset, int *bytes_read);

int write2(char * path, char * buf, size_t size, off_t offset);

int unlink2 (char *path);

int validarArchivo(char* path);

char *leerInfoDeBloques(char **bloques, size_t size, off_t off, int *ioff);
int leerBloque(char *sblk, off_t off, char **info, int rd_size);
int escribirInfoEnBloques(char **bloques, char *info, size_t size, off_t off);
int escribirBloque(char *sblk, off_t off, char *info, int wr_size);
void iniciarBloques(int cant, char* path);
char** obtenerBloquesDisponibles(int cant);
int agregarBloquesSobreBloques(char ***bloques, char **blq_add, int prev, int add);
void marcarBloquesOcupados(char **bloques, int cant);
int obtenerCantidadBloquesLibres(void);



#endif /* OPERACIONESFS_H_ */
