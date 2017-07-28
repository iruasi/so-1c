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

extern AnSISOP_funciones functions;
extern int pag_size, stack_size;
extern t_log * logTrace;

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
	log_trace(logTrace,"funcion puntero a contexto");
	int i, abs_size;
	indiceStack *stack;

	for (i = abs_size = 0; i < ctxt; ++i){
		stack = list_get(stack_ind, i);
		abs_size += sizeof(t_valor_variable) * (list_size(stack->args) + list_size(stack->vars));
	}
	return abs_size;
}

void obtenerUltimoEnContexto(t_list *stack, int *pag, int *off, int *size){
	log_trace(logTrace,"funcion obtener ultimo en contexto");
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
	log_trace(logTrace,"funcion obtener variable");
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
	//printf("definir la variable %c\n", variable);
	log_trace(logTrace,"inicio definirVariable %c",variable);
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
		log_trace(logTrace,"No se pudo definir la variable, stack del proceso lleno");
		err_exec = MEM_OVERALLOC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	//printf("La variable '%c' se define en (p,o,s) %d, %d, %d\n", variable, var->pos.pag, var->pos.offset, var->pos.size);
	//printf("Contexto: %d\n", pcb->contextoActual);
	log_trace(logTrace,"La variable '%c' se define en (p,o,s) %d, %d, %d [PID %d]", variable, var->pos.pag, var->pos.offset, var->pos.size,pcb->id);
	log_trace(logTrace,"Contexto %d [PID %d]",pcb->contextoActual,pcb->id);
	if (list_size(pcb->indiceDeStack) == 0){
		ult_stack = crearStackVacio();
		list_add(ult_stack->vars, var);
		list_add(pcb->indiceDeStack, ult_stack);

	} else {
		ult_stack = list_get(pcb->indiceDeStack, pcb->contextoActual);
		list_add(ult_stack->vars, var);
	}
	log_trace(logTrace,"fin definir variable [PID %d]",pcb->id);
	return stack_ptr + pcb->paginasDeCodigo * pag_size;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable){
	//printf("Obtener posicion de %c\n", variable);
	log_trace(logTrace,"inicio ObtenerPosicionVariable de %c [PID%d]",variable,pcb->id);
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
	log_trace(logTrace,"fin obtenerPosicionVariable [PID%d]",pcb->id);
	return stack_ptr + pm.pag * pag_size + pm.offset;
}

void finalizar(void){
	//printf("Finalizar\n");
	log_trace(logTrace,"inicio funcion Finalizar [PID %d]",pcb->id);
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
	log_trace(logTrace,"fin funcion finalizar[PID %d]",pcb->id);
}

t_valor_variable dereferenciar(t_puntero puntero) {
	//printf("Dereferenciar al puntero de posicion %d\n", puntero);
	log_trace(logTrace,"inicio dereferenciar al puntero de posicion %d [PID %d]",puntero,pcb->id);
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
	//printf("Dereferenciar %d y su valor es: %d\n", puntero, var);
	log_trace(logTrace,"fin funcion deferenciar %d y su valor es %d [PID %d]",puntero,var,pcb->id);
	return var;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	//printf("Asignando en %d el valor %d\n", puntero, variable);
	log_trace(logTrace,"inicio asignar en %d el valor %d [PID %d]",puntero,variable,pcb->id);
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
	log_trace(logTrace,"fin asignar [PID %d]",pcb->id);
	free(byteal_serial);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
// `variable' llega con un '\0' al final
	//printf("Asignando en %s el valor %d\n", variable, valor);
	log_trace(logTrace,"inicio asignar valor compartida en %s el valor %d [PID %d]",variable,valor,pcb->id);
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
	log_trace(logTrace,"fin asignar valor compartida [PID %d]",pcb->id);
	return valor;
}

void irAlLabel (t_nombre_etiqueta etiqueta){
	char* label = eliminarWhitespace(etiqueta);
	//printf("Se va al label %s\n", label);
	log_trace(logTrace,"inicio ir al label %s [PID %d]",label,pcb->id);
	if ((pcb->pc = metadata_buscar_etiqueta(label, pcb->indiceDeEtiquetas, pcb->etiquetas_size)) == -1){
		printf("No se encontro la etiqueta %s en el indiceDeEtiquetas", label);
		err_exec = FALLO_INSTR;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	pcb->pc--; // es porque ejecutarInstruccion() incrementa el pc. Si, esto es efectivamente un asco
	log_trace(logTrace,"fin ir al label [PID %d]",pcb->id);
	free(label);
}

void llamarSinRetorno (t_nombre_etiqueta etiqueta){
	//printf("Llamar sin retorno a %s\n", etiqueta);
	log_trace(logTrace,"inicio llamarSinRetorno a %s [PID %d]",etiqueta,pcb->id);
	puts("Esta funcion no se usa en este TP. Retornamos FALLO_INSTR!");
	err_exec = FALLO_INSTR;
	sem_post(&sem_fallo_exec);
	log_trace(logTrace,"fin llamarSinRetorno");
	pthread_exit(&err_exec);
}

void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	//printf("Se llama la funcion %s y se retornara en %d\n", etiqueta, donde_retornar);
	log_trace(logTrace,"se llama la funcion %s y se retornara en %d [PID %d]",etiqueta,donde_retornar,pcb->id);
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
	log_trace(logTrace,"fin llamarConRetorno[PID %d]",pcb->id);
}

void retornar (t_valor_variable retorno){ // todo: revisar correctitud
	log_trace(logTrace,"inicio retornoar [PID %d]",pcb->id);
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
	log_trace(logTrace,"fin retornar [PID %d]",pcb->id);
	pcb->contextoActual--;
}

t_valor_variable obtenerValorCompartida (t_nombre_compartida variable){
	//printf("Se obtiene el valor de la variable compartida %s\n", variable);
	log_trace(logTrace,"inicio obtenerValorCompartida %s [PID %d]",variable,pcb->id);
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
	log_trace(logTrace,"fin obtenerValorCompartida [PID %d]",pcb->id);
	return valor;
}



//FUNCIONES ANSISOP QUE LE PIDE AL KERNEL
void wait (t_nombre_semaforo identificador_semaforo){
	char * sem = eliminarWhitespace(identificador_semaforo);
	//printf("Se pide al Kernel un wait para el semaforo %s\n", sem);
	log_trace(logTrace,"se pide al kernel un wait para el semaforo %s [PID %d]",sem,pcb->id);
	tPackHeader head   = {.tipo_de_proceso = CPU, .tipo_de_mensaje = S_WAIT};
	tPackHeader h_esp  = {.tipo_de_proceso = KER};
	int pack_size = 0;

	int lenId = strlen(sem) + 1;
	char *wait_serial;
	if ((wait_serial = serializeBytes(head, sem, lenId, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(wait_serial, pack_size);

	h_esp.tipo_de_mensaje = S_WAIT;
	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		if (head.tipo_de_mensaje != PCB_BLOCK){ // fallo el syscall
			err_exec = head.tipo_de_mensaje;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);

		} else // Kernel pide desalojar el PCB para bloquearlo
			bloqueado = true;
	}
	log_trace(logTrace,"fin wait a kernel [PID %d]",pcb->id);
	free(wait_serial);
	free(sem);
}

void signal_so (t_nombre_semaforo identificador_semaforo){
	//printf("Se pide al kernel un signal para el semaforo %s\n", identificador_semaforo);
	log_trace(logTrace,"inicio signal_so para el sem %s [PID %d]",identificador_semaforo,pcb->id);
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
	log_trace(logTrace,"fin signal_so [PID %d]",pcb->id);
	free(sig_serial);
	free(sem);
}

void liberar (t_puntero puntero){
	//printf("Se pide al kernel liberar memoria. Inicio: %d\n", puntero);
	log_trace(logTrace,"inicio liberar a kernel incio %d [PID %d]",puntero,pcb->id);
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
	log_trace(logTrace,"fin liberar a kernel [PID %d]",pcb->id);

}

t_descriptor_archivo abrir(t_direccion_archivo direccion,t_banderas flags){
	char *dir = eliminarWhitespace(direccion);
	log_trace(logTrace, "Se pide al kernel abrir el archivo %s\n [PID %d]", dir,pcb->id);

	int pack_size = 0;
	char *buffer, *abrir_serial;
	tPackHeader head, h_esp;
	tPackPID *val;
	tPackAbrir *abrir = malloc(sizeof *abrir);
	t_descriptor_archivo fd;

	abrir->longitudDireccion = strlen(dir) + 1;
	abrir->direccion = dir;
	abrir->flags     = flags;

	if ((abrir_serial = serializeAbrir(abrir, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(abrir_serial, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = ENTREGO_FD;
	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((buffer = recvGeneric(sock_kern)) == NULL){
		err_exec = FALLO_RECV;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	if ((val = deserializeVal(buffer)) == NULL){
		err_exec = FALLO_DESERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);

	}else{
		pcb->cantSyscalls ++;
	}

	//fd = *((int *) bytes->bytes);
	fd = val->val;
	log_trace(logTrace, "Se recibio el fd %d \nFin abrir [PID %d]", fd,pcb->id);

	free(abrir_serial);
	free(buffer);
	free(val);
	return fd;
}

void borrar (t_descriptor_archivo fd){
	log_trace(logTrace,"inicio borrar el archivo %d [PID %d]", fd, pcb->id);

	tPackHeader header, h_esp;
	char *borrar_serial;
	int pack_size = 0;

	tPackVal * val = malloc(sizeof(*val));
	val->head.tipo_de_proceso = CPU;
	val->head.tipo_de_proceso = BORRAR;
	val->val = fd;

	if ((borrar_serial = serializeVal(val, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(borrar_serial, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = ARCHIVO_BORRADO;
	if(validarRespuesta(sock_kern, h_esp, &header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
	log_trace(logTrace,"fin borrar [PID %d]",pcb->id);

	free(borrar_serial);
	free(val);
}

void cerrar (t_descriptor_archivo descriptor_archivo){
	log_trace(logTrace,"Cerrar archivo fd: %d", descriptor_archivo);

	tPackHeader header, h_esp;

	int pack_size = 0;
	tPackVal * val = malloc(sizeof(*val));
	val->head.tipo_de_proceso = CPU;
	val->head.tipo_de_mensaje = CERRAR;
	val->val = descriptor_archivo;

	char *cerrar_serial;
	if ((cerrar_serial = serializeVal(val,&pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(cerrar_serial, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = ARCHIVO_CERRADO;
	if(validarRespuesta(sock_kern, h_esp, &header)){
		err_exec = header.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls ++;
	}
	log_trace(logTrace,"fin cerrar [PID %d]",pcb->id);

	free(cerrar_serial);
	free(val);
}

void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion){
	//printf("Se pide al kernel mover el archivo %d a la posicion %d\n", descriptor_archivo, posicion);
	log_trace(logTrace,"inicio moverCursor el archivo %d a la posicion %d [PID %d]",descriptor_archivo,posicion,pcb->id);
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
	log_trace(logTrace,"fin moverCursor [PID %d]",pcb->id);

	free(mov_serial);
}

void escribir (t_descriptor_archivo fd, void* informacion, t_valor_variable tamanio){
	log_trace(logTrace,"inicio escribir el archivo %d con la info %s bytes %d [PID %d]", fd, (char *)informacion,tamanio, pcb->id);

	int pack_size = 0;
	char *esc_serial;
	tPackHeader h_esp, head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ESCRIBIR};

	tPackRW *escr = malloc(sizeof *escr);
	escr->fd = fd;
	escr->info = informacion;
	escr->tamanio = tamanio;

	if ((esc_serial = serializeRW(head, escr, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(esc_serial, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = ARCHIVO_ESCRITO;
	if(validarRespuesta(sock_kern,h_esp,&head)){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}else{
		pcb->cantSyscalls++;
	}
	log_trace(logTrace,"fin escribir [PID %d]",pcb->id);
}

void leer (t_descriptor_archivo descriptor_archivo, t_puntero ptr, t_valor_variable tamanio){
	log_trace(logTrace, "Leer fd %d por %d bytes y almacenar en %d [PID %d]", descriptor_archivo, tamanio, ptr,pcb->id);

	tPackHeader head, h_esp;
	int pack_size = 0;

	tPackLeer *p_leer = malloc(sizeof *p_leer);
	p_leer->fd   = descriptor_archivo;
	p_leer->size = tamanio;

	char *buff;
	if ((buff = serializeLeer(p_leer, &pack_size)) == NULL){
		err_exec = FALLO_SERIALIZAC;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	enviar(buff, pack_size);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = ARCHIVO_LEIDO;
	if (validarRespuesta(sock_kern, h_esp, &head) != 0){
		err_exec = head.tipo_de_mensaje;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	// todo:
	//buffer = recvGeneric(sock_kern);
	//bla = deserializeBytes(buffer);
	//writeToPointer(ptr, bytes, size);

	log_trace(logTrace, "fin leer [PID %d]",pcb->id);
	free(p_leer);
	free(buff);
}

t_puntero reservar (t_valor_variable espacio){
	//printf("Se pide al kernel reservar %d espacio de memoria\n", espacio);
	log_trace(logTrace,"inicio reservar %d espacio de memoria [PID %d]",espacio,pcb->id);
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
	log_trace(logTrace,"fin reservar [PID %d]",pcb->id);
	return val->val;
}

void enviar(char *op_kern_serial, int pack_size){
	log_trace(logTrace,"inicio envair [PID %d]",pcb->id);
	int stat;
	if ((stat = send(sock_kern, op_kern_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar la operacion privilegiada a Kernel. error");
		err_exec = FALLO_SEND;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}
	//printf("Se enviaron %d bytes a Kernel\n", stat);
	log_trace(logTrace,"fin enviar(se enviaron %d bytes a kernel) [PID %d]",stat,pcb->id);
}
