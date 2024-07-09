#include <../include/kernel.h>

t_config *config_kernel;

t_log *logger_kernel;
char *ip_memoria;
char *ip_cpu;
char *puerto_memoria;
char *puerto_escucha;
char *puerto_dispatch;
char *puerto_interrupt;
char *algoritmo_planificacion;
char *ip;
u_int32_t conexion_memoria, conexion_dispatch, conexion_interrupt, flag_cpu_libre;
int socket_servidor_kernel;
int contador_pcbs;
uint32_t contador_pid;
t_pcb *ultimo_pcb_ejecutado;
uint32_t procesos_en_ready_plus;
int quantum;
char *nombre_entrada_salida_conectada;
uint16_t planificar_ready_plus;

t_dictionary *conexiones_io;
t_dictionary *colas_blocks_io; // cola de cada interfaz individual, adentro están las isntrucciones a ejecutar
t_dictionary *diccionario_semaforos_io;
t_dictionary *recursos_disponibles;
t_dictionary *cola_de_bloqueados_por_recurso;
t_dictionary *recursos_asignados_por_proceso;
t_dictionary *tiempo_restante_de_quantum_por_proceso;

pthread_mutex_t mutex_pid;
pthread_mutex_t mutex_cola_de_readys;
pthread_mutex_t mutex_lista_de_blocked;
pthread_mutex_t mutex_cola_de_new;
pthread_mutex_t mutex_proceso_en_ejecucion;
pthread_mutex_t mutex_interfaces_conectadas;
pthread_mutex_t mutex_cola_interfaces; // PARA AGREGAR AL DICCIOANRIO de la cola de cada interfaz
pthread_mutex_t mutex_diccionario_interfaces_de_semaforos;
pthread_mutex_t mutex_flag_cpu_libre;
pthread_mutex_t mutex_motivo_ultimo_desalojo;
pthread_mutex_t mutex_procesos_en_sistema;
pthread_mutex_t mutex_cola_de_ready_plus;
pthread_mutex_t mutex_flag_planificar;
sem_t hay_proceso_a_ready, cpu_libre, arrancar_quantum;
sem_t podes_revisar_lista_bloqueados;
pthread_mutex_t mutex_cola_de_exit;
sem_t hay_proceso_exit;
sem_t hay_proceso_nuevo;
t_queue *cola_procesos_ready;
t_queue *cola_procesos_new;
t_queue *cola_procesos_exit;
t_queue *cola_ready_plus;
t_list *lista_procesos_blocked; // lsita de los prccesos bloqueados
t_list *procesos_en_sistema;
t_pcb *pcb_en_ejecucion;
pthread_t planificador_corto;
pthread_t planificador_largo;
pthread_t dispatch;
pthread_t hilo_quantum;
pthread_t planificador_largo_creacion;
pthread_t planificador_largo_eliminacion;
pthread_mutex_t mutex_espacio_para_procesos;
op_code motivo_ultimo_desalojo;
uint8_t flag_planificar;
char **recursos;
char **instancias_recursos;
uint32_t espacio_para_procesos_disponible;
t_semaforo_contador *semaforo_multi;

int grado_multiprogramacion;

int main(void)
{
    iniciar_config();
    iniciar_variables();
    iniciar_listas();
    iniciar_colas_de_estados_procesos();
    iniciar_diccionarios();
    iniciar_semaforos();
    iniciar_recursos();

    // iniciar conexion con Kernel
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria, logger_kernel);
    enviar_mensaje("", conexion_memoria, KERNEL, logger_kernel);

    // iniciar conexion con CPU Dispatch e Interrupt
    conexion_dispatch = crear_conexion(ip_cpu, puerto_dispatch, logger_kernel);
    conexion_interrupt = crear_conexion(ip_cpu, puerto_interrupt, logger_kernel);
    enviar_mensaje("", conexion_dispatch, KERNEL, logger_kernel);
    enviar_mensaje("", conexion_interrupt, KERNEL, logger_kernel);

    // iniciar Servidor
    socket_servidor_kernel = iniciar_servidor(logger_kernel, puerto_escucha, "KERNEL");

    pthread_create(&dispatch, NULL, (void *)recibir_dispatch, NULL);
    pthread_detach(dispatch);

    pthread_create(&planificador_corto, NULL, (void *)iniciar_planificador_corto_plazo, NULL);
    pthread_detach(planificador_corto);

    pthread_create(&planificador_largo, NULL, (void *)iniciar_planificador_largo_plazo, NULL);
    pthread_detach(planificador_largo);

    pthread_t thread_consola;
    pthread_create(&thread_consola, NULL, (void *)iniciar_consola_interactiva, NULL);
    pthread_detach(thread_consola);

    if (strcmp(algoritmo_planificacion, "RR") == 0)
    {
        log_info(logger_kernel, "iniciando contador de quantum");
        pthread_create(&hilo_quantum, NULL, (void *)verificar_quantum, NULL);
        pthread_detach(hilo_quantum);
    }

    if(strcmp(algoritmo_planificacion, "VRR") == 0){
        log_info(logger_kernel, "iniciando contador de quantum");
        cola_ready_plus = queue_create();
        tiempo_restante_de_quantum_por_proceso = dictionary_create();
        pthread_create(&hilo_quantum, NULL, (void *)verificar_quantum_vrr, NULL);
        pthread_detach(hilo_quantum);
    }

    while (1)
    {
        pthread_t thread;
        int *socket_cliente = malloc(sizeof(int));
        *socket_cliente = esperar_cliente(socket_servidor_kernel, logger_kernel);
        pthread_create(&thread, NULL, (void *)atender_cliente, socket_cliente);
        pthread_detach(thread);
    }

    log_info(logger_kernel, "Se cerrará la conexión.");
    terminar_programa(socket_servidor_kernel, logger_kernel, config_kernel);
}


void iniciar_config(void)
{
    config_kernel = config_create("config/kernel.config");
    logger_kernel = iniciar_logger("config/kernel.log", "KERNEL", LOG_LEVEL_INFO);
    ip_memoria = config_get_string_value(config_kernel, "IP_MEMORIA");
    ip_cpu = config_get_string_value(config_kernel, "IP_CPU");
    puerto_dispatch = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");
    puerto_interrupt = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");
    puerto_memoria = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    puerto_escucha = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");
    quantum = config_get_int_value(config_kernel, "QUANTUM");
    algoritmo_planificacion = config_get_string_value(config_kernel, "ALGORITMO_PLANIFICACION");
    grado_multiprogramacion = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");
}

void *atender_cliente(void *socket_cliente_ptr)
{
    int socket_cliente = *(int *)socket_cliente_ptr;
    free(socket_cliente_ptr); // Liberamos el puntero ya que no lo necesitamos más

    op_code codigo_operacion = recibir_operacion(socket_cliente);

    switch (codigo_operacion)
    {
    case INTERFAZ_GENERICA:
        nombre_entrada_salida_conectada = recibir_mensaje_guardar_variable(socket_cliente);

        crear_interfaz(INTERFAZ_GENERICA, nombre_entrada_salida_conectada, socket_cliente);

        free(nombre_entrada_salida_conectada);
        break;
    case INTERFAZ_STDIN:
        nombre_entrada_salida_conectada = recibir_mensaje_guardar_variable(socket_cliente);

        crear_interfaz(INTERFAZ_STDIN, nombre_entrada_salida_conectada, socket_cliente);

        free(nombre_entrada_salida_conectada);
        break;
    case INTERFAZ_STDOUT:
        nombre_entrada_salida_conectada = recibir_mensaje_guardar_variable(socket_cliente);

        crear_interfaz(INTERFAZ_STDOUT, nombre_entrada_salida_conectada, socket_cliente);

        free(nombre_entrada_salida_conectada);
        break;
    case INTERFAZ_DIALFS:
        nombre_entrada_salida_conectada = recibir_mensaje_guardar_variable(socket_cliente);

        crear_interfaz(INTERFAZ_DIALFS, nombre_entrada_salida_conectada, socket_cliente);

        free(nombre_entrada_salida_conectada);
        break;
    default:
        log_info(logger_kernel, "Se recibió un mensaje de un módulo desconocido");
        break;
    }

    return NULL;
}
