

#ifndef KERNEL_H_
#define KERNEL_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <commons/log.h>
#include <commons/string.h>
#include <readline/readline.h>
#include <../include/utils.h>
#include <semaphore.h>




typedef struct{
    pthread_mutex_t mutex;
    t_queue *cola;
} t_cola_interfaz_io;

typedef struct{
    t_pcb *pcb;
    op_code motivo;
} t_proceso_en_exit;

typedef struct{
    int valor_actual;
    int valor_maximo;
    sem_t contador;
    pthread_mutex_t mutex_valor_actual;
    pthread_mutex_t mutex_valor_maximo;
} t_semaforo_contador;

extern t_config *config_kernel;
extern t_config *config_conexiones;

extern t_log *logger_kernel;
extern char *ip_memoria;
extern char *ip_cpu;
extern char *puerto_memoria;
extern char *puerto_escucha;
extern char *puerto_dispatch;
extern char *puerto_interrupt;
extern char *algoritmo_planificacion;
extern char *ip;
extern u_int32_t conexion_memoria, conexion_dispatch, conexion_interrupt, flag_cpu_libre,flag_grado_multi;
extern int socket_servidor_kernel;
extern int contador_pcbs;
extern uint32_t contador_pid;
extern int quantum;
extern op_code motivo_ultimo_desalojo;
extern char *nombre_entrada_salida_conectada;
extern int grado_multiprogramacion;
extern t_pcb *ultimo_pcb_ejecutado;
extern uint32_t procesos_en_ready_plus;
extern uint16_t planificar_ready_plus;
extern sem_t podes_revisar_lista_bloqueados;




extern t_dictionary *conexiones_io;
extern t_dictionary *colas_blocks_io; // cola de cada interfaz individual, adentro están las isntrucciones a ejecutar
extern t_dictionary *diccionario_semaforos_io;
extern t_dictionary *recursos_disponibles;
extern t_dictionary *cola_de_bloqueados_por_recurso;
extern t_dictionary *recursos_asignados_por_proceso;
extern t_dictionary *tiempo_restante_de_quantum_por_proceso;

extern pthread_mutex_t mutex_motivo_ultimo_desalojo;
extern pthread_mutex_t mutex_pid;
extern pthread_mutex_t mutex_cola_de_readys;
extern pthread_mutex_t mutex_lista_de_blocked;
extern pthread_mutex_t mutex_cola_de_new;
extern pthread_mutex_t mutex_proceso_en_ejecucion;
extern pthread_mutex_t mutex_interfaces_conectadas;
extern pthread_mutex_t mutex_cola_interfaces; // PARA AGREGAR AL DICCIOANRIO de la cola de cada interfaz
extern pthread_mutex_t mutex_diccionario_interfaces_de_semaforos;
extern pthread_mutex_t mutex_flag_cpu_libre;
extern pthread_cond_t cond_flag_cpu_libre;
extern pthread_mutex_t mutex_procesos_en_sistema;
extern pthread_mutex_t mutex_cola_de_ready_plus;
extern sem_t hay_proceso_a_ready, cpu_libre, arrancar_quantum;
extern t_queue *cola_procesos_ready;
extern t_queue *cola_procesos_new;
extern t_queue *cola_procesos_exit;
extern t_queue *cola_ready_plus;
extern t_list *lista_procesos_blocked; // lsita de los prccesos bloqueados
extern t_list *procesos_en_sistema;
extern t_pcb *pcb_en_ejecucion;
extern pthread_t planificador_corto;
extern pthread_t dispatch;
extern pthread_t hilo_quantum;
extern pthread_t planificador_largo_creacion;
extern pthread_t planificador_largo_eliminacion;
extern char **recursos;
extern char **instancias_recursos;
extern pthread_mutex_t mutex_cola_de_exit;
extern sem_t hay_proceso_exit;
extern sem_t hay_proceso_nuevo;
extern sem_t hay_proceso_a_ready_plus;
extern pthread_t planificador_largo;
extern uint8_t flag_planificar;
extern pthread_mutex_t mutex_flag_planificar;
extern t_semaforo_contador *semaforo_multi;
extern sem_t podes_planificar_corto_plazo;
extern sem_t podes_manejar_desalojo;
extern sem_t podes_eliminar_procesos; 
extern sem_t podes_crear_procesos;
extern sem_t grado_multi;
extern bool planificacion_detenida;
extern sem_t podes_eliminar_loko;
extern pthread_mutex_t mutex_ultimo_pcb;
extern pthread_mutex_t mutex_flag_planificar_plus;
extern char* interfaz_causante_bloqueo;
extern pthread_mutex_t mutex_nombre_interfaz_bloqueante;
extern sem_t podes_manejar_recepcion_de_interfaces;

// --------------------- FUNCIONES DE INICIO -------------------------
void iniciar_semaforos();
void iniciar_diccionarios();
void iniciar_listas();
void iniciar_colas_de_estados_procesos();
void iniciar_config_kernel(char *ruta_config, char* ruta_logger);
void iniciar_recursos();
void iniciar_variables();

void *atender_cliente(void *socket_cliente);
void iniciar_semaforo_contador(t_semaforo_contador *semaforo, uint32_t valor_inicial);
void destruir_semaforo_contador(t_semaforo_contador *semaforo);
void destruir_semaforos();
void eliminar_diccionarios();
void eliminar_listas();
void eliminar_colas();

// --------------------- FUNCIONES DE CONSOLA INTERACTIVA -------------------------
void *iniciar_consola_interactiva();
bool validar_comando(char *comando);
void ejecutar_comando(char *comandoRecibido);
void listar_procesos();
// void ejecutar_script(t_buffer* buffer);

// --------------------- FUNCIONES DE PCB -------------------------
void ejecutar_PCB(t_pcb *pcb);
void setear_pcb_en_ejecucion(t_pcb *pcb);
void set_add_pcb_cola(t_pcb *pcb, estados estado, t_queue *cola, pthread_mutex_t mutex);
t_pcb *buscar_pcb_por_pid(u_int32_t pid, t_list *lista);
void agregar_pcb_a_cola_bloqueados_de_recurso(t_pcb *pcb, char *nombre);
void finalizar_pcb(t_pcb *pcb, op_code motivo);
void listar_procesos_en_ready();
void listar_procesos_en_ready_plus();
bool hay_proceso_ejecutandose();
uint32_t tener_index_pid(uint32_t pid);
void bloquear_pcb(t_pcb *pcb);
void logear_lista_blocked();
t_pcb *buscar_pcb_en_procesos_del_sistema(uint32_t pid);
void actualizar_pcb_en_procesos_del_sistema(t_pcb *pcb_actualizado);
uint32_t buscar_index_pid_bloqueado(uint32_t pid);
bool tiene_mismo_pid(uint32_t pid1, uint32_t pid2);

// --------------------- FUNCIONES DE PLANIFICACION -------------------------
void iniciar_planificador_corto_plazo();
void iniciar_planificador_largo_plazo();
void creacion_de_procesos();
void eliminacion_de_procesos();
void *recibir_dispatch();
void *verificar_quantum();
void *verificar_quantum_vrr();
void signal_contador(t_semaforo_contador *semaforo);
void wait_contador(t_semaforo_contador *semaforo);
void cambiar_grado(uint32_t nuevo_grado);
void iniciar_planificacion();

// --------------------- FUNCIONES DE INTERFACES ENTRADA/SALIDA -------------------------
bool interfaz_conectada(char *nombre_interfaz);
bool esOperacionValida(t_identificador identificador, cod_interfaz tipo);
void crear_interfaz(op_code tipo, char *nombre, uint32_t conexion);
void ejecutar_instruccion_io(char *nombre_interfaz, t_info_en_io *info_io, t_interfaz_en_kernel *conexion_io);
void atender_interfaz(char *nombre_interfaz);


// -------------------- FUNCIONES DE RECURSOS -------------------------------
void iniciar_recurso(char* nombre, char* instancias);
bool existe_recurso(char *nombre);
void liberar_recursos(uint32_t pid);
void retener_instancia_de_recurso(char *nombre_recurso, uint32_t pid);
int32_t restar_instancia_a_recurso(char *nombre);
void sumar_instancia_a_recurso(char *nombre);
void logear_recursos_por_proceso(uint32_t pid);
void eliminar_pcb_de_cola_bloqueados_de_recurso(uint32_t pid, char *nombre);
void imprimir_recursos();
void restar_instancia_retenida_a_proceso(char *recurso, uint32_t pid);
// ---------------------- FUNCIONES DE LOGS --------------------------
void logear_bloqueo_proceso(uint32_t pid, char* motivo);
void logear_cambio_estado(t_pcb *pcb, estados estado_anterior, estados estado_actual);

#endif