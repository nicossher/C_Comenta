#include <../include/kernel.h>

void *recibir_dispatch()
{
    while (1)
    {
        op_code motivo_desalojo = recibir_motivo_desalojo(conexion_dispatch);
        t_pcb *pcb_actualizado = recibir_pcb(conexion_dispatch);

        if (planificacion_detenida)
        {
            sem_wait(&podes_manejar_desalojo);
        }

        pthread_mutex_lock(&mutex_motivo_ultimo_desalojo);
        motivo_ultimo_desalojo = motivo_desalojo;
        pthread_mutex_unlock(&mutex_motivo_ultimo_desalojo);

        pthread_mutex_lock(&mutex_ultimo_pcb);
        ultimo_pcb_ejecutado = pcb_actualizado;
        pthread_mutex_unlock(&mutex_ultimo_pcb);

        /*
        pthread_mutex_lock(&mutex_proceso_en_ejecucion);
        if (pcb_en_ejecucion != NULL) {
        destruir_pcb(pcb_en_ejecucion); 
        }
        pthread_mutex_unlock(&mutex_proceso_en_ejecucion);
        */

        switch (motivo_desalojo)
        {
        case OPERACION_IO:
        {
            t_paquete *respuesta_kernel = recibir_paquete(conexion_dispatch);
            u_int32_t nombre_length;
            t_identificador identificador;
            buffer_read(respuesta_kernel->buffer, &identificador, sizeof(t_identificador));
            buffer_read(respuesta_kernel->buffer, &nombre_length, sizeof(u_int32_t));
            char *nombre_io = malloc(nombre_length + 1);
            buffer_read(respuesta_kernel->buffer, nombre_io, nombre_length);

            pthread_mutex_lock(&mutex_flag_cpu_libre);
            flag_cpu_libre = 1;
            pthread_cond_signal(&cond_flag_cpu_libre);
            pthread_mutex_unlock(&mutex_flag_cpu_libre);

            if (!interfaz_conectada(nombre_io))
            {
                finalizar_pcb(pcb_actualizado, INVALID_INTERFACE);
                sem_post(&cpu_libre);
                break;
            }
            t_interfaz_en_kernel *interfaz = dictionary_get(conexiones_io, nombre_io);
            if (!esOperacionValida(identificador, interfaz->tipo_interfaz))
            {
                finalizar_pcb(pcb_actualizado, INVALID_INTERFACE);
                sem_post(&cpu_libre);
                break;
            }

            // pcb_actualizado->estado_actual = BLOCKED;
            logear_cambio_estado(pcb_actualizado, EXEC, BLOCKED);
            logear_bloqueo_proceso(pcb_actualizado->pid,nombre_io);
            bloquear_pcb(pcb_actualizado);

            pthread_mutex_lock(&mutex_nombre_interfaz_bloqueante);
            interfaz_causante_bloqueo = nombre_io;
            pthread_mutex_unlock(&mutex_nombre_interfaz_bloqueante);

            // signal_contador(semaforo_multi);

            // 1. La agregamos a la cola de blocks io. datos necesarios para ahhacer el io y PID
            t_info_en_io *info_io = malloc(sizeof(t_info_en_io));
            buffer_read(respuesta_kernel->buffer, &info_io->tam_info, sizeof(u_int32_t));
            info_io->info_necesaria = malloc(info_io->tam_info);
            buffer_read(respuesta_kernel->buffer, info_io->info_necesaria, info_io->tam_info);
            info_io->pid = pcb_actualizado->pid;

            t_cola_interfaz_io *cola_interfaz = dictionary_get(colas_blocks_io, nombre_io);

            pthread_mutex_lock(&cola_interfaz->mutex);
            queue_push(cola_interfaz->cola, info_io);
            pthread_mutex_unlock(&cola_interfaz->mutex);

            t_semaforosIO *semaforos_interfaz = dictionary_get(diccionario_semaforos_io, nombre_io);
            sem_post(&semaforos_interfaz->instruccion_en_cola);

            free(nombre_io);
            eliminar_paquete(respuesta_kernel);

            sem_post(&cpu_libre);
            break;
        }
        case FIN_CLOCK:
        {
            log_info(logger_kernel, "PID: %d - Desalojado por Fin de Quantum", pcb_actualizado->pid);
            set_add_pcb_cola(pcb_actualizado, READY, cola_procesos_ready, mutex_cola_de_readys);
            actualizar_pcb_en_procesos_del_sistema(pcb_actualizado);

            logear_cambio_estado(pcb_actualizado, EXEC, READY);
            sem_post(&hay_proceso_a_ready);
            pthread_mutex_lock(&mutex_flag_cpu_libre);
            flag_cpu_libre = 1;
            pthread_cond_signal(&cond_flag_cpu_libre);
            pthread_mutex_unlock(&mutex_flag_cpu_libre);
            sem_post(&cpu_libre);
            break;
        }
        case END_PROCESS:
        {
            finalizar_pcb(pcb_actualizado, SUCCESS);

            pthread_mutex_lock(&mutex_flag_cpu_libre);
            flag_cpu_libre = 1;
            pthread_cond_signal(&cond_flag_cpu_libre);
            pthread_mutex_unlock(&mutex_flag_cpu_libre);
            sem_post(&cpu_libre);
            break;
        }
        case WAIT_SOLICITADO:
        {
            t_paquete *respuesta = recibir_paquete(conexion_dispatch);
            t_instruccion *utlima = instruccion_deserializar(respuesta->buffer, 0);
            char *recurso_solicitado = strdup(utlima->parametros[0]);
            bool flag_recurso_no_existe = false;

            if (!existe_recurso(recurso_solicitado))
            {
                enviar_codigo_operacion(RESOURCE_FAIL, conexion_dispatch);
                pthread_mutex_lock(&mutex_flag_cpu_libre);
                flag_cpu_libre = 1;
                pthread_cond_signal(&cond_flag_cpu_libre);
                pthread_mutex_unlock(&mutex_flag_cpu_libre);
                sem_post(&podes_revisar_lista_bloqueados);
                sem_post(&cpu_libre);
                finalizar_pcb(pcb_actualizado, INVALID_RESOURCE);
                flag_recurso_no_existe = true;
            }
            if (!flag_recurso_no_existe)
            {
                int32_t instancias_restantes = restar_instancia_a_recurso(recurso_solicitado);
                retener_instancia_de_recurso(recurso_solicitado, pcb_actualizado->pid);
                if (instancias_restantes < 0)
                {
                    enviar_codigo_operacion(RESOURCE_BLOCKED, conexion_dispatch);
                    agregar_pcb_a_cola_bloqueados_de_recurso(pcb_actualizado, recurso_solicitado);

                    logear_bloqueo_proceso(pcb_actualizado->pid, recurso_solicitado);
                    logear_cambio_estado(pcb_actualizado, EXEC, BLOCKED);
                    bloquear_pcb(pcb_actualizado);

                    //logear_lista_blocked();

                    pthread_mutex_lock(&mutex_flag_cpu_libre);
                    flag_cpu_libre = 1;
                    pthread_cond_signal(&cond_flag_cpu_libre);
                    pthread_mutex_unlock(&mutex_flag_cpu_libre);

                    sem_post(&cpu_libre);
                    // signal_contador(semaforo_multi);
                }
                else
                {
                    enviar_codigo_operacion(RESOURCE_OK, conexion_dispatch);
                    if(pcb_actualizado!=NULL){
                        destruir_pcb(pcb_actualizado);
                    }
                }
            }
                sem_post(&podes_revisar_lista_bloqueados);
            liberar_t_instruccion(utlima);
            eliminar_paquete(respuesta);

            break;
            
        }
        case SIGNAL_SOLICITADO:
        {
            t_paquete *respuesta_signal = recibir_paquete(conexion_dispatch);
            t_instruccion *instruccion_signal = instruccion_deserializar(respuesta_signal->buffer, 0);
            char *recurso_signal = instruccion_signal->parametros[0];

            if (!existe_recurso(recurso_signal))
            {
                enviar_codigo_operacion(RESOURCE_FAIL, conexion_dispatch);
                finalizar_pcb(pcb_actualizado, INVALID_RESOURCE);

                pthread_mutex_lock(&mutex_flag_cpu_libre);
                flag_cpu_libre = 1;
                pthread_cond_signal(&cond_flag_cpu_libre);
                pthread_mutex_unlock(&mutex_flag_cpu_libre);
                sem_post(&cpu_libre);
            }
            else
            {
                sumar_instancia_a_recurso(recurso_signal);
                restar_instancia_retenida_a_proceso(recurso_signal, pcb_actualizado->pid);
                enviar_codigo_operacion(RESOURCE_OK, conexion_dispatch);
                if(pcb_actualizado!=NULL)
                    destruir_pcb(pcb_actualizado);
            }
            liberar_t_instruccion(instruccion_signal);
            eliminar_paquete(respuesta_signal);
            break;
        }
        case KILL_PROCESS:
        {
            sem_post(&podes_eliminar_loko);
            pthread_mutex_lock(&mutex_flag_cpu_libre);
            flag_cpu_libre = 1;
            pthread_cond_signal(&cond_flag_cpu_libre);
            pthread_mutex_unlock(&mutex_flag_cpu_libre);
            break;
        }
        case OUT_OF_MEMORY:
        {
            finalizar_pcb(pcb_actualizado, OUT_OF_MEMORY);
            sem_post(&cpu_libre);
        }
        case RESOURCE_BLOCKED:
            break;
        default:
            break;
        }
    }
}
