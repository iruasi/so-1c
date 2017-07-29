#ifndef OPERACIONESFS_H_
#define OPERACIONESFS_H_

#ifndef MAX_AB
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif
#ifndef MIN_AB
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif


int open2(char *path);

int read2(char *path, char **buf, size_t size, off_t offset);

int write2(char * path, char * buf, size_t size, off_t offset);

int unlink2 (char *path);

int validarArchivo(char* path);

int escribirInfoEnBloques(char **bloques, char *info, size_t size, off_t off);
int escribirBloque(char *sblk, off_t off, char *info, int wr_size);
void iniciarBloques(int cant, char* path);
char** obtenerBloquesDisponibles(int cant);
void agregarBloquesSobreBloques(char ***bloques, char **blq_add);
void marcarBloquesOcupados(char **bloques);
int obtenerCantidadBloquesLibres(void);



#endif /* OPERACIONESFS_H_ */
