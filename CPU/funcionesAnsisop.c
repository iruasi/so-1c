#include <stdbool.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/string.h>
#include <commons/log.h>

#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "funcionesAnsisop.h"

extern bool termino;
extern AnSISOP_funciones functions;
extern int pag_size, stack_size;
extern t_log*logger;

extern int err_exec;
extern sem_t sem_fallo_exec;

void setupCPUFunciones(void){
	functions.AnSISOP_definirVariable		= definirVariable;
	functions.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable;
	functions.AnSISOP_finalizar 				= finalizar;
	functions.AnSISOP_dereferenciar			= dereferenciar;
	functions.AnSISOP_asignar				= asignar;
	functions.AnSISOP_asignarValorCompartida = asignarValorCompartida;
	functions.AnSISOP_irAlLabel				= irAlLabel;
	functions.AnSISOP_llamarSinRetorno		= llamarSinRetorno;
	functions.AnSISOP_llamarConRetorno		= llamarConRetorno;
	functions.AnSISOP_retornar				= retornar;
	functions.AnSISOP_obtenerValorCompartida = obtenerValorCompartida;
}

void setupCPUFuncionesKernel(void){
	kernel_functions.AnSISOP_wait 					= wait;
	kernel_functions.AnSISOP_signal					= signal_so;
	kernel_functions.AnSISOP_abrir					= abrir;
	kernel_functions.AnSISOP_borrar					= borrar;
	kernel_functions.AnSISOP_cerrar					= cerrar;
	kernel_functions.AnSISOP_escribir				= escribir;
	kernel_functions.AnSISOP_leer					= leer;
	kernel_functions.AnSISOP_liberar				= liberar;
	kernel_functions.AnSISOP_moverCursor			= moverCursor;
	kernel_functions.AnSISOP_reservar				= reservar;
}

t_puntero punteroAContexto(t_list *stack_ind, int ctxt){
	int i, abs_size;
	indiceStack *stack;

	for (i = abs_size = 0; i < ctxt; ++i){
		stack = list_get(stack_ind, i);
		abs_size += sizeof(t_valor_variable) * (list_size(stack->args) + list_size(stack->vars));
	}
	return abs_size;
}

void obtenerUltimoEnContexto(t_list *stack, int *pag, int *off, int *size){

	*pag = *off = *size = 0;
	indiceStack* ultimoStack = list_get(stack, pcb->contextoActual);

	if (!list_size(stack)) // es decir que es el StackVacio()
		return;

	posicionMemoria*   ultimoArg = list_get (ultimoStack->args, list_size(ultimoStack->args)-1);
	posicionMemoriaId* ultimaVar = list_get (ultimoStack->vars, list_size(ultimoStack->vars)-1);

	if (ultimoArg == NULL && ultimaVar == NULL)
		return;

	if (ultimoArg == NULL){
		SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
		return;

	} else if (ultimaVar == NULL){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;

	} else if(ultimoArg->pag > ultimaVar->pos.pag){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;

	} else if(ultimoArg->pag < ultimaVar->pos.pag){
		SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
		return;

	} else if(ultimoArg->offset > ultimaVar ->pos.offset){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;
	}

	SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
}

void obtenerVariable(t_nombre_variable variable, posicionMemoria* pm, indiceStack* stack){
	int i;
	posicionMemoriaId* var;
	for(i=0; i < list_size(stack->vars); i++){
		var = list_get(stack->vars, i);
		if(var->id == variable){
			pm->offset = var->pos.offset;
			pm->pag    = var->pos.pag;
			pm->size   = var->pos.size;
			return;
		}
	}
	pm = NULL;
}

//FUNCIONES DE ANSISOP
t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);

	indiceStack* ult_stack;
	int pag, off, size, stack_ptr;
	obtenerUltimoEnContexto(pcb->indiceDeStack, &pag, &off, &size);
	stack_ptr = punteroAContexto(pcb->indiceDeStack, pcb->contextoActual);

	posicionMemoriaId* var = malloc(sizeof(posicionMemoriaId));
	var->id         = variable;
	var->pos.offset = (off + size) % pag_size;
	var->pos.pag    = pag + (off + size) / pag_size;
	var->pos.size   = sizeof (t_valor_variable);

	if (stack_ptr / pag_size >= stack_size){ // nos pasamos del limite
		printf("No se pudo definir la variable %c: Stack del proceso lleno\n", variable);
		err_exec = MEM_OVERALLOC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	printf("La variable '%c' se define en (p,o,s) %d, %d, %d\n", variable, var->pos.pag, var->pos.offset, var->pos.size);
	printf("Contexto: %d\n", pcb->contextoActual);
	if (list_size(pcb->indiceDeStack) == 0){
		ult_stack = crearStackVacio();
		list_add(ult_stack->vars, var);
		list_add(pcb->indiceDeStack, ult_stack);

	} else {
		ult_stack = list_get(pcb->indiceDeStack, pcb->contextoActual);
		list_add(ult_stack->vars, var);
	}

	return stack_ptr + pcb->paginasDeCodigo * pag_size;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable){
	printf("Obtener posicion de %c\n", variable);

	int i, stack_ptr;
	indiceStack* stack = list_get(pcb->indiceDeStack, pcb->contextoActual);
	posicionMemoria pm;
	posicionMemoriaId* var;

	for(i = 0; i < list_size(stack->vars); i++){
		var = list_get(stack->vars, i);
		if(var->id == variable){
			pm.offset = var->pos.offset;
			pm.pag    = var->pos.pag + pcb->paginasDeCodigo;
			pm.size   = var->pos.size;
			break;
		}
	}

	if (var->id != variable){
		err_exec = FALLO_INSTR;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	stack_ptr = punteroAContexto(pcb->indiceDeStack, pcb->contextoActual);
	return stack_ptr + pm.pag * pag_size + pm.offset;
}

void finalizar(void){
	printf("Finalizar\n");
	if(pcb->contextoActual==0){
		termino = true;
		return;
	}
	indiceStack* stackActual = list_get(pcb->indiceDeStack, pcb->contextoActual);
	int i;
	for(i=0 ; list_size(stackActual->args) ; i++){
		posicionMemoria* arg = list_get(stackActual->args, i);
		free(arg); // se liberan los argumentos
	}

	for(i=0 ; list_size(stackActual->vars) ; i++){
		posicionMemoriaId* var = list_get(stackActual->vars, i);
		free(var);//se liberan las variables
	}
	free(stackActual);
	list_remove(pcb->indiceDeStack, pcb->contextoActual);
	pcb->contextoActual--;
	indiceStack* nuevoContexto=list_get(pcb->indiceDeStack,pcb->contextoActual);
	pcb->pc=nuevoContexto->retPos;
}

t_valor_variable dereferenciar(t_puntero puntero) {
	printf("Dereferenciar al puntero de posicion %d\n", puntero);

	t_valor_variable var;
	tPackByteReq pbr;
	tPackBytes* bytes;
	int pack_size, stat;
	char *byterq_serial, *bytes_serial;
	tPackHeader h = {.tipo_de_proceso = CPU, .tipo_de_mensaje = BYTES};

	memcpy(&pbr.head, &h, HEAD_SIZE);
	pbr.pid    = pcb->id;
	pbr.page   = puntero / pag_size;
	pbr.offset = puntero % pag_size;
	pbr.size   = sizeof(t_puntero);

	if ((byterq_serial = serializeByteRequest(&pbr, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if((stat = send(sock_mem, byterq_serial, pack_size, 0)) == -1){
		perror("Fallo send de byte request. error");
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((stat = recv(sock_mem, &h, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de header desde Memoria. error");
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((bytes_serial = recvGeneric(sock_mem)) == NULL){
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((bytes = deserializeBytes(bytes_serial)) == NULL){
		err_exec = FALLO_DESERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	memcpy(&var, bytes->bytes, sizeof(var));
	printf("Dereferenciar %d y su valor es: %d\n", puntero, var);
	return var;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	printf("Asignando en %d el valor %d\n", puntero, variable);

	tPackByteAlmac pbal;
	char *byteal_serial;
	int pack_size, stat;
	tPackHeader h = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ALMAC_BYTES};

	memcpy(&pbal.head, &h, HEAD_SIZE);
	pbal.pid    = pcb->id;
	pbal.page   = puntero / pag_size;
	pbal.offset = puntero % pag_size;
	pbal.size   = sizeof (t_valor_variable);
	pbal.bytes  = (char *) &variable;

	pack_size = 0;
	if ((byteal_serial = serializeByteAlmacenamiento(&pbal, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if((stat = send(sock_mem, byteal_serial, pack_size, 0)) == -1){
		perror("Fallo send de byte request. error");
		err_exec = FALLO_SEND;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	free(byteal_serial);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
// `variable' llega con un '\0' al final
	printf("Asignando en %s el valor %d\n", variable, valor);

	char *valor_serial;
	int pack_size, stat;
	tPackHeader h_obt;
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = SET_GLOBAL};
	pack_size = 0;
	if ((valor_serial = serializeValorYVariable(head, valor, variable, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((stat = send(sock_kern, valor_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar el paquete de Valor y Variable a Kernel. error");
		err_exec = FALLO_SEND;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	head.tipo_de_proceso = KER; head.tipo_de_mensaje = SET_GLOBAL;
	if (validarRespuesta(sock_kern, head, &h_obt) != 0){
		err_exec = h_obt.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	free(valor_serial);
	return valor;
}

void irAlLabel (t_nombre_etiqueta etiqueta){
	char* label = eliminarWhitespace(etiqueta);
	printf("Se va al label %s\n", label);

	if ((pcb->pc = metadata_buscar_etiqueta(label, pcb->indiceDeEtiquetas, pcb->etiquetas_size)) == -1){
		printf("No se encontro la etiqueta %s en el indiceDeEtiquetas", label);
		err_exec = FALLO_INSTR;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	pcb->pc--; // es porque ejecutarInstruccion() incrementa el pc. Si, esto es efectivamente un asco
	free(label);
}

void llamarSinRetorno (t_nombre_etiqueta etiqueta){
	printf("Llamar sin retorno a %s\n", etiqueta);

	puts("Esta funcion no se usa en este TP. Retornamos FALLO_INSTR!");
	err_exec = FALLO_INSTR;
	sem_post(&sem_fallo_exec);
	pthread_exit(&err_exec);
}

void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Se llama la funcion %s y se retornara en %d\n", etiqueta, donde_retornar);

	int pag, off, size;

	indiceStack *nuevoStack = crearStackVacio();
	obtenerUltimoEnContexto(pcb->indiceDeStack, &pag, &off, &size);

	nuevoStack->retVar.pag    = pag;
	nuevoStack->retVar.offset = off;
	nuevoStack->retVar.size   = size;
	nuevoStack->retPos = donde_retornar;

	list_add(pcb->indiceDeStack, nuevoStack);
	pcb->contextoActual++;

	irAlLabel(etiqueta);
}

void retornar (t_valor_variable retorno){ // todo: revisar correctitud
	int contextoActual= pcb->contextoActual;
	indiceStack* stackActual = list_get(pcb->indiceDeStack,contextoActual);

	// Liberamos los argumentos y variables del stack
	while(list_size(stackActual->args)){
		posicionMemoria* arg = list_remove(stackActual->args, 0);
		freeAndNULL((void **) &arg); // se liberan los argumentos
	}
	while(list_size(stackActual->vars)){
		posicionMemoria* var = list_remove(stackActual->vars, 0);
		freeAndNULL((void **) &var); // se liberan las variables
	}

 	t_puntero retPtr = stackActual->retVar.pag * pag_size + stackActual->retVar.offset;
	asignar(retPtr, retorno);
	pcb->pc = stackActual->retPos;

	free(stackActual);
	list_remove(pcb->indiceDeStack, pcb->contextoActual);

	pcb->contextoActual--;
}

t_valor_variable obtenerValorCompartida (t_nombre_compartida variable){
	printf("Se obtiene el valor de la variable compartida %s\n", variable);

	t_valor_variable valor;
	tPackValComp *val_var;
	int pack_size, var_len;
	char *var_serial, *var;
	var = eliminarWhitespace(variable);
	var_len = strlen(var) + 1;
	tPackHeader h_obt;
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = GET_GLOBAL};

	pack_size = 0;
	if ((var_serial = serializeBytes(head, var, var_len, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(var_serial, pack_size);
	freeAndNULL((void **) &var_serial);

	head.tipo_de_proceso = KER; head.tipo_de_mensaje = GET_GLOBAL;
	if (validarRespuesta(sock_kern, head, &h_obt) != 0){
		err_exec = h_obt.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((var_serial = recvGeneric(sock_kern)) == NULL){
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((val_var = deserializeValorYVariable(var_serial)) == NULL){
		err_exec = FALLO_DESERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	memcpy(&valor, &val_var->val, sizeof(t_valor_variable));
	free(var_serial);
	free(val_var);
	free(var);
	return valor;
}



//FUNCIONES ANSISOP QUE LE PIDE AL KERNEL
void wait (t_nombre_semaforo identificador_semaforo){
	char * sem = eliminarWhitespace(identificador_semaforo);
	printf("Se pide al Kernel un wait para el semaforo %s\n", sem);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = S_WAIT};
	tPackHeader h_esp = {.tipo_de_proceso = KER, .tipo_de_mensaje = S_WAIT};
	int pack_size = 0;

	int lenId = strlen(sem) + 1;
	char *wait_serial;
	if ((wait_serial = serializeBytes(head, sem, lenId, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(wait_serial, pack_size);

	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	free(wait_serial);
	free(sem);
}

void signal_so (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un signal para el semaforo %s\n", identificador_semaforo);

	tPackHeader head  = {.tipo_de_proceso = CPU, .tipo_de_mensaje = S_SIGNAL};
	tPackHeader h_esp = {.tipo_de_proceso = KER, .tipo_de_mensaje = S_SIGNAL};
	int pack_size = 0;

	char * sem = eliminarWhitespace(identificador_semaforo);
	int lenId = strlen(sem) + 1;
	char *sig_serial;
	if ((sig_serial = serializeBytes(head, sem, lenId, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(sig_serial, pack_size);

	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	free(sig_serial);
	free(sem);
}

void liberar (t_puntero puntero){
	printf("Se pide al kernel liberar memoria. Inicio: %d\n", puntero);

	tPackHeader head;
	tPackHeader h_esp = {.tipo_de_proceso = KER, .tipo_de_mensaje = LIBERAR};

	tPackVal *pval;
	int pack_size = 0;
	pval = malloc(sizeof *pval);
	pval->head.tipo_de_proceso = CPU; pval->head.tipo_de_mensaje = LIBERAR;
	pval->val = puntero;

	char *free_serial;
	if ((free_serial = serializeVal(pval, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(free_serial, pack_size);

	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

}

tPackFS * deserializeFileDescriptor(char * aux_serial){
	tPackFS * aux = malloc(sizeof *aux);

	int off = 0;

	memcpy(&aux->fd, aux_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&aux->cantidadOpen, aux_serial + off, sizeof(int));
	off += sizeof(int);

	return aux;
} //Lo pase a funcionesPaquetes pero lo dejo comentado si tengo que debuggearlo en algun futuro no muy lejano




t_descriptor_archivo abrir(t_direccion_archivo direccion,t_banderas flags){

	printf("Se pide al kernel abrir el archivo %s\n",direccion);
	char * dir = eliminarWhitespace(direccion);
	int pack_size = 0;
	char * buffer;
	int stat;
	tPackHeader head;

	tPackBytes * bytes;
	tPackAbrir * abrir = malloc(sizeof *abrir);

	abrir->longitudDireccion = strlen(dir) + 1;
	abrir->direccion = dir;
	abrir->flags     = flags;


	char *abrir_serial;
	if ((abrir_serial = serializeAbrir(abrir, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(abrir_serial, pack_size);

	if ((stat = recv(sock_kern, &head, HEAD_SIZE, MSG_WAITALL)) < 0){
		perror("Fallo recv de puntero alojado de Kernel. error");
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	printf("El head recibido es de %d y tiene el mensaje %d \n",head.tipo_de_proceso,head.tipo_de_mensaje);


	if ((buffer = recvGeneric(sock_kern)) == NULL){
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	t_descriptor_archivo fd;
	if ((bytes = deserializeBytes(buffer)) == NULL){
				err_exec = FALLO_DESERIALIZAC;
				sem_post(&sem_fallo_exec);
				pthread_exit(&err_exec);
			}else{
				pcb->cantSyscalls ++;
			}
	fd = *((int *) bytes->bytes);
	return fd;
}

void borrar (t_descriptor_archivo direccion){
	printf("Se pide al kernel borrar el archivo %d\n", direccion);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = BORRAR};
	int pack_size = 0;

	char *borrar_serial;
	if ((borrar_serial = serializeBytes(head, (char*) &direccion, sizeof direccion, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(borrar_serial, pack_size);

	tPackHeader h_esp = {.tipo_de_proceso = KER,.tipo_de_mensaje=ARCHIVO_BORRADO};
	tPackHeader header;
	if(validarRespuesta(sock_kern,h_esp,&header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
}

void cerrar (t_descriptor_archivo descriptor_archivo){
	printf("Se pide al kernel cerrar el archivo %d\n", descriptor_archivo);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = CERRAR};
	int pack_size = 0;

	char *cerrar_serial;
	if ((cerrar_serial = serializeBytes(head, (char*) &descriptor_archivo, sizeof descriptor_archivo, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(cerrar_serial, pack_size);

	tPackHeader h_esp = {.tipo_de_proceso = KER,.tipo_de_mensaje=ARCHIVO_CERRADO};
	tPackHeader header;

	if(validarRespuesta(sock_kern,h_esp,&header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
}

void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion){
	printf("Se pide al kernel mover el archivo %d a la posicion %d\n", descriptor_archivo, posicion);

	int pack_size = 0;

	char *mov_serial;
	if ((mov_serial = serializeMoverCursor(descriptor_archivo, posicion, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(mov_serial, pack_size);

	tPackHeader h_esp = {.tipo_de_proceso = KER,.tipo_de_mensaje=CURSOR_MOVIDO};
	tPackHeader header;

	if(validarRespuesta(sock_kern,h_esp,&header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
}

void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio){
	printf("Se pide al kernel escribir el archivo %d con la informacion %s, cantidad de bytes: %d\n", descriptor_archivo, (char*)informacion, tamanio);

	int pack_size = 0;

	char *esc_serial;
	if ((esc_serial = serializeEscribir(descriptor_archivo, informacion, tamanio, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(esc_serial, pack_size);

	tPackHeader h_esp = {.tipo_de_proceso = KER,.tipo_de_mensaje = ARCHIVO_ESCRITO};
	tPackHeader header;

	if(validarRespuesta(sock_kern,h_esp,&header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
}


void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio){
	printf("Se pide al kernel leer el archivo %d, se guardara en %d, cantidad de bytes: %d\n", descriptor_archivo, informacion, tamanio);

	int pack_size = 0;
	tPackRW * read = malloc(sizeof(*read));
	read->fd = descriptor_archivo;
	read->info = &informacion;
	read->tamanio = tamanio;
	char *leer_serial;
	if ((leer_serial = serializeLeer(read, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(leer_serial, pack_size);
}

t_puntero reservar (t_valor_variable espacio){ // todo: ya casi esta
	printf("Se pide al kernel reservar %d espacio de memoria\n", espacio);

	char *ptr_serial;
	tPackVal *val;
	tPackHeader head, h_esp;

	val = malloc(sizeof *val);
	val->head.tipo_de_proceso = CPU; val->head.tipo_de_mensaje = RESERVAR;
	val->val = espacio;

	int pack_size = 0;
	char *reserva_serial;
	if ((reserva_serial = serializeVal(val, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	freeAndNULL((void **) &val);

	enviar(reserva_serial, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = RESERVAR;
	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((ptr_serial = recvGeneric(sock_kern)) == NULL){
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((val = deserializeVal(ptr_serial)) == NULL){
		err_exec = FALLO_DESERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	return val->val;
}

void enviar(char *op_kern_serial, int pack_size){
	int stat;
	if ((stat = send(sock_kern, op_kern_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar la operacion privilegiada a Kernel. error");
		err_exec = FALLO_SEND;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	printf("Se enviaron %d bytes a Kernel\n", stat);
}
