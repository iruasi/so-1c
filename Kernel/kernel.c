#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <parser/parser.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#include "capaMemoria.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"
#include "planificador.h"

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

#define BACKLOG 20

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
void test_iniciarPaginasDeCodigoEnMemoria(int sock_mem, char *code, int size_code, int pags);

int setGlobal(tPackValComp *val_comp); // todo: poner en otro lado
t_valor_variable getGlobal(t_nombre_variable *var, bool *found); // todo: poner en otro lado

void cons_manejador(int sock_mem, int sock_hilo, tMensaje msj);
void cpu_manejador(int sock_cpu, tMensaje msj);
tPackSrcCode *recibir_paqueteSrc(int fd);


tHeapProc *hProcs;
int hProcs_cant;

int MAX_ALLOC_SIZE; // con esta variable se debe comprobar que CPU no pida mas que este size de HEAP
int sock_cpu;
int frames, frame_size; // para guardar datos a recibir de Memoria
tKernel *kernel;
extern t_valor_variable *shared_vals;

t_list *listaProgramas;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	listaProgramas = list_create();

	int stat, ready_fds;
	int fd, new_fd;
	int fd_max = -1;
	int sock_fs, sock_mem;
	int sock_lis_cpu, sock_lis_con;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	setupPlanificador();

	// Se trata de conectar con Memoria
	if ((sock_mem = establecerConexion(kernel->ip_memoria, kernel->puerto_memoria)) < 0){
		fprintf(stderr, "No se pudo conectar con la Memoria! sock_mem: %d\n", sock_mem);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Memoria
	if ((stat = handshakeCon(sock_mem, kernel->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Memoria\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a MEMORIA\n", stat);

	fd_max = MAX(sock_mem, fd_max);

	// Se trata de conectar con Filesystem
	if ((sock_fs = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		fprintf(stderr, "No se pudo conectar con el Filesystem! sock_fs: %d\n", sock_fs);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Filesystem
	if ((stat = handshakeCon(sock_fs, kernel->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Filesystem\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a FILESYSTEM\n", stat);

	fd_max = MAX(sock_fs, fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", sock_lis_cpu);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_cpu, fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_con: %d\n", sock_lis_con);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_con, fd_max);

	// Se agregan memoria, fs, listen_cpu, listen_consola y stdin al set master
	FD_SET(sock_mem, &master_fd);
	FD_SET(sock_fs, &master_fd);
	FD_SET(sock_lis_cpu, &master_fd);
	FD_SET(sock_lis_con, &master_fd);
	FD_SET(0, &master_fd);

	while ((stat = listen(sock_lis_cpu, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	while ((stat = listen(sock_lis_con, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}


	tPackHeader *header_tmp = malloc(HEAD_SIZE); // para almacenar cada recv
	while (1){

		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if(ready_fds == -1){
			perror("Fallo el select(). error");
			return FALLO_SELECT;
		}

		for (fd = 0; fd <= fd_max; ++fd){
		if (FD_ISSET(fd, &read_fd)){

			printf("Hay un socket listo! El fd es: %d\n", fd);

			// Controlamos el listen de CPU o de Consola
			if (fd == sock_lis_cpu){
				sock_cpu = handleNewListened(fd, &master_fd);
				if (sock_cpu < 0){
					perror("Fallo en manejar un listen. error");
					return FALLO_CONEXION;
				}

				fd_max = MAX(sock_cpu, fd_max);
				break;

			} else if (fd == sock_lis_con){

				new_fd = handleNewListened(fd, &master_fd);
				if (new_fd < 0){
					perror("Fallo en manejar un listen. error");
					return FALLO_CONEXION;
				}

				fd_max = MAX(new_fd, fd_max);
				break;
			}

			// Como no es un listen, recibimos el header de lo que llego
			if ((stat = recv(fd, header_tmp, HEAD_SIZE, 0)) == -1){
				perror("Error en recv() de algun socket. error");
				fprintf(stderr, "El socket asociado al fallo es: %d\n", fd);
				break;

			} else if (stat == 0){
				printf("Se desconecto el socket %d\nLo sacamos del set listen...\n", fd);
				clearAndClose(&fd, &master_fd);
				break;
			}

			if (fd == sock_mem){
				printf("Llego algo desde memoria!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);

				if (header_tmp->tipo_de_mensaje != MEMINFO)
					break;


				if((stat = recibirInfoKerMem(fd, &frames, &frame_size)) == -1){
					puts("No se recibio correctamente informacion de Memoria!");
					return FALLO_GRAL;
				}

				MAX_ALLOC_SIZE = frame_size - 2 * SIZEOF_HMD;

				break;

			} else if (fd == sock_fs){
				printf("llego algo desde fs!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;

			} else if (fd == 0){ //socket del stdin
				printf("Ingreso texto por pantalla!\nCerramos Kernel!\n");
				goto limpieza; // nos vamos del ciclo infinito...
			}

			if (header_tmp->tipo_de_proceso == CON){
				printf("Llego algo desde Consola!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				cons_manejador(sock_mem, fd, header_tmp->tipo_de_mensaje);
				break;
			}

			if (header_tmp->tipo_de_proceso == CPU){
				printf("Llego algo desde CPU!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				cpu_manejador(fd, header_tmp->tipo_de_mensaje);
				break;
			}

			puts("Si esta linea se imprime, es porque el header_tmp tiene algun valor rarito...");
			printf("El valor de header_tmp es: proceso %d \t mensaje: %d\n", header_tmp->tipo_de_proceso, header_tmp->tipo_de_mensaje);

		}} // aca terminan el for() y el if(FD_ISSET)
	}

limpieza:
	// Un poco mas de limpieza antes de cerrar

	free(header_tmp);

	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
	close(sock_mem);
	close(sock_fs);
	close(sock_lis_con);
	close(sock_lis_cpu);

	liberarConfiguracionKernel(kernel);
	return stat;
}

void cons_manejador(int sock_mem, int sock_hilo, tMensaje msj){

	switch(msj){
	case SRC_CODE:
		puts("Consola quiere iniciar un programa");

		int src_size;
		tPackSrcCode *entradaPrograma = NULL;
		entradaPrograma = recibir_paqueteSrc(sock_hilo);//Aca voy a recibir el tPackSrcCode
		src_size = strlen((const char *) entradaPrograma->sourceCode) + 1; // strlen no cuenta el '\0'
		printf("El size del paquete %d\n", src_size);

		int cant_pag = (int) ceil((float)src_size / frame_size);
		tPCB *new_pcb = nuevoPCB(entradaPrograma, cant_pag, sock_hilo);  //Toda la lógica de la paginacion la hago a la hora de crear el pcb, si no hay pagina => no hay pcb
		//En nuevoPcb, casteo entradaPrograma para que me de los valores.

		test_iniciarPaginasDeCodigoEnMemoria(sock_mem, entradaPrograma->sourceCode, src_size, cant_pag);

		encolarEnNEWPrograma(new_pcb, sock_hilo);

		puts("Listo!");
		break;

	default:
		break;
	}


}

void cpu_manejador(int sock_cpu, tMensaje msj){
	printf ("El sock cpu manejado es %d y el mensaje %d\n", sock_cpu, msj);

	tPackHeader head = {.tipo_de_proceso = KER};
	tMensaje rta;
	bool found;
	char *buffer;
	char *var = NULL;
	int stat, pack_size;
	tPackBytes *var_name;
	tPackEscribir *escr;
	t_valor_variable val;

	switch(msj){
	case S_WAIT:
		puts("Signal wait a semaforo");
		//planificadorPasarABlock();
		break;
	case S_SIGNAL:
		//planificadorPasarABlock();
		puts("Signal continuar a semaforo");
		break;

	case SET_GLOBAL:
		puts("Se reasigna una variable global");

		if ((buffer = recvGeneric(sock_cpu)) == NULL){
			puts("Fallo recepcion generica");
			break;
		}

		tPackValComp *val_comp;
		if ((val_comp = deserializeValorYVariable(buffer)) == NULL){
			puts("No se pudo deserializar Valor y Variable");
			// todo: abortar programa?
			break;
		}

		if ((stat = setGlobal(val_comp)) != 0){
			puts("No se pudo asignar la variable global");
			// todo: abortar programa?
			break;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &val_comp);
		break;

	case GET_GLOBAL:
		puts("Se pide el valor de una variable global");

		if ((buffer = recvGeneric(sock_cpu)) == NULL){
			puts("Fallo recepcion generica");
			break;
		}

		var_name = deserializeBytes(buffer);
		freeAndNULL((void **) &buffer);

		var = realloc(var, var_name->bytelen);
		memcpy(var, var_name->bytes, var_name->bytelen);

		val = getGlobal(var, &found);
		rta = (found)? GET_GLOBAL : GLOBAL_NOT_FOUND;
		head.tipo_de_mensaje = rta;

		if ((buffer = serializeValorYVariable(head, val, var, &pack_size)) == NULL){
			puts("No se pudo serializar Valor Y Variable");
			return;
		}

		if ((stat = send(sock_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de Valor y Variable. error");
			return;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void**) &var_name->bytes); freeAndNULL((void **) &var_name);
		break;

	case LIBERAR:
		puts("Funcion liberar");
		break;
	case ABRIR:
		break;
	case BORRAR:
		break;
	case CERRAR:
		break;
	case MOVERCURSOR:
		break;
	case ESCRIBIR:

		buffer = recvGeneric(sock_cpu);
		tPackEscribir *escr = deserializeEscribir(buffer);

		printf("Se escriben en fd %d, la info %s\n", escr->fd, (char*) escr->info);
		free(escr->info); free(escr);
		break;

	case LEER:
		break;
	case RESERVAR:
		break;
	case HSHAKE:
		puts("Es solo un handshake");
		break;
	default:
		puts("Funcion no reconocida!");
		break;
	}
}

int setGlobal(tPackValComp *val_comp){

	int i;
	int nlen = strlen(val_comp->nom) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, val_comp->nom, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			shared_vals[i] = val_comp->val;
			free(aux);
			return 0;
		}
	}
	free(aux);
	return GLOBAL_NOT_FOUND;
}

t_valor_variable getGlobal(t_nombre_variable *var, bool* found){

	int i;
	int nlen = strlen(var) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	*found = true;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, var, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			free(aux);
			return shared_vals[i];
		}
	}
	free(aux);
	*found = false;
	return GLOBAL_NOT_FOUND;
}

tPackSrcCode *recibir_paqueteSrc(int fd){ //Esta funcion tiene potencial para recibir otro tipos de paquetes

	int paqueteRecibido;
	int *tamanioMensaje = malloc(sizeof (int));

	paqueteRecibido = cantidadTotalDeBytesRecibidos(fd, (char *) tamanioMensaje, sizeof(int));
	if(paqueteRecibido <= 0 ) return NULL;

	void *mensaje = malloc(*tamanioMensaje);
	paqueteRecibido = cantidadTotalDeBytesRecibidos(fd, mensaje, *tamanioMensaje);
	if(paqueteRecibido <= 0) return NULL;

	tPackSrcCode *pack_src = malloc(sizeof *pack_src);
	pack_src->sourceLen = paqueteRecibido;
	pack_src->sourceCode = malloc(pack_src->sourceLen);
	memcpy(pack_src->sourceCode, mensaje, paqueteRecibido);

	//tPackSrcCode *buffer = deserializeSrcCode(fd);

	free(tamanioMensaje);tamanioMensaje = NULL;
	free(mensaje);mensaje = NULL;

	return pack_src;

}

// todo: remover test cuando ya no sea necesario
void test_iniciarPaginasDeCodigoEnMemoria(int sock_mem, char *code, int size_code, int pags){
	puts("\n\n\t\tEmpieza el test....");

	int stat;
	int pack_size = 0;
	tPackHeader ini = { .tipo_de_proceso = KER, .tipo_de_mensaje = INI_PROG };
	tPackHeader src = { .tipo_de_proceso = KER, .tipo_de_mensaje = ALMAC_BYTES };

	tPackPidPag p;
	p.head = ini;
	p.pid = 0;
	p.pageCount = pags + kernel->stack_size;

	char * pidpag_serial = serializePIDPaginas(&p);
	if (pidpag_serial == NULL){
		puts("fallo serialize PID Paginas");
		return;
	}

	puts("Enviamos inicializacion de progama");
	if ((stat = send(sock_mem, pidpag_serial, 16, 0)) == -1)
		puts("Fallo pedido de inicializacion de prog en Memoria...");

	tPackByteAlmac *pbal = malloc(sizeof *pbal);
	memcpy(&pbal->head, &src, HEAD_SIZE);
	pbal->pid = 0;
	pbal->page = 0;
	pbal->offset = 0;
	pbal->size = size_code;
	pbal->bytes = code;

	char* packBytes;
	if ((packBytes = serializeByteAlmacenamiento(pbal, &pack_size)) == NULL){
		puts("fallo serialize Bytes");
		return;
	}

	puts("Enviamos el srccode");
	if ((stat = send(sock_mem, packBytes, pack_size, 0)) == -1)
		puts("Fallo envio src code a Memoria...");

	printf("se enviaron %d bytes\n", stat);

	freeAndNULL((void **) &pbal);
	sleep(2);
	puts("\n\n\t\tSe completo el test.\n\n");
}
