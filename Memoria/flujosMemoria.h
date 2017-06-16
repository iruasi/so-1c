#ifndef FLUJOSMEMORIA_H_
#define FLUJOSMEMORIA_H_


/* Recibe la Solicitud de Bytes, obtiene el pedido solicitado, y lo reenvia al socket demandante
 * Retorna 0 si no hubieron fallos. Retorna negativo caso contrario.
 */
int manejarSolicitudBytes(int sock_in);


/* Recibe los Bytes para Almacenar, obtiene estos bytes y usa la API para almacenarlos en Memoria Fisica
 * Retorna 0 si no hubieron fallos. Retorna negativo caso contrario.
 */
int manejarAlmacenamientoBytes(int sock_cpu);

#endif /* FLUJOSMEMORIA_H_ */
