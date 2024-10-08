#include <../include/utils.h>

sem_t semaforo;

void imprimir_registros_por_pantalla(t_registros registros)
{
	printf("-------------------\n");
	printf("Valores de los Registros:\n");
	printf(" + AX = %d\n", registros.AX);
	printf(" + BX = %d\n", registros.BX);
	printf(" + CX = %d\n", registros.CX);
	printf(" + DX = %d\n", registros.DX);
	printf(" + EAX = %d\n", registros.EAX);
	printf(" + EBX = %d\n", registros.EBX);
	printf(" + ECX = %d\n", registros.ECX);
	printf(" + EDX = %d\n", registros.EDX);
	printf(" + PC = %d\n", registros.PC);
	printf(" + SI = %d\n", registros.SI);
	printf(" + DI = %d\n", registros.DI);
	printf("-------------------\n");
}

//------------------ FUNCIONES DE PCB ------------------

t_pcb *crear_pcb(u_int32_t pid, u_int32_t quantum, estados estado)
{
	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->pid = pid;
	pcb->pc = 0;
	pcb->quantum = quantum;
	pcb->estado_actual = estado;
	pcb->registros = inicializar_registros();
	return pcb;
}

void destruir_pcb(t_pcb *pcb)
{
	free(pcb);
}

void enviar_pcb(t_pcb *pcb, int socket)
{
	t_paquete *paquete = crear_paquete(PCB);
	t_buffer *buffer = paquete->buffer;
	buffer_add(buffer, &(pcb->pid), sizeof(uint32_t));
	buffer_add(buffer, &(pcb->pc), sizeof(uint32_t));
	buffer_add(buffer, &(pcb->quantum), sizeof(uint32_t));
	buffer_add(buffer, &(pcb->registros), SIZE_REGISTROS);
	buffer_add(buffer, &(pcb->estado_actual), sizeof(estados));
	buffer->offset = 0;

	enviar_paquete(paquete, socket);

	eliminar_paquete(paquete);
}

t_pcb *recibir_pcb(int socket)
{
	t_paquete *paquete_PCB = recibir_paquete(socket);
	t_buffer *buffer_PCB = paquete_PCB->buffer;

	t_pcb *pcb = malloc(sizeof(t_pcb));

	if (paquete_PCB->codigo_operacion == PCB)
	{
		buffer_read(buffer_PCB, &(pcb->pid), sizeof(pcb->pid));
		buffer_read(buffer_PCB, &(pcb->pc), sizeof(pcb->pc));
		buffer_read(buffer_PCB, &(pcb->quantum), sizeof(pcb->quantum));
		buffer_read(buffer_PCB, &(pcb->registros), SIZE_REGISTROS);
		buffer_read(buffer_PCB, &(pcb->estado_actual), sizeof(estados));
	}

	eliminar_paquete(paquete_PCB);
	return pcb;
}

t_registros inicializar_registros()
{
	t_registros registros;
	registros.AX = 0;
	registros.BX = 0;
	registros.CX = 0;
	registros.DX = 0;
	registros.EAX = 0;
	registros.EBX = 0;
	registros.ECX = 0;
	registros.EDX = 0;
	registros.SI = 0;
	registros.DI = 0;
	registros.PC = 0;
	return registros;
}

char *cod_op_to_string(op_code codigo_operacion)
{
	switch (codigo_operacion)
	{
	case SOLICITUD_INSTRUCCION:
		return "SOLICITUD_INSTRUCCION";
	case INSTRUCCION:
		return "INSTRUCCION";
	case CREAR_PROCESO:
		return "CREAR_PROCESO";
	case CPU:
		return "CPU";
	case KERNEL:
		return "KERNEL";
	case MEMORIA:
		return "MEMORIA";
	case ENTRADA_SALIDA:
		return "ENTRADA_SALIDA";
	case PCB:
		return "PCB";
	case INTERRUPTION:
		return "INTERRUPTION";
	default:
		return "CODIGO DE OPERACION INVALIDO";
	}
}
void cargar_string_al_buffer(t_buffer *buffer, char *string)
{
	int len = strlen(string);
	buffer->stream = malloc(len + 1);
	if (buffer->stream == NULL)
	{
		printf("Error: no se pudo asignar memoria para el buffer\n");
		exit(EXIT_FAILURE);
	}
	strcpy(buffer->stream, string);
	buffer->size = len;
}
char *extraer_string_del_buffer(t_buffer *buffer)
{
	char *string = malloc(buffer->size + 1);
	if (string == NULL)
	{
		printf("Error: no se pudo asignar memoria para el string\n");
		exit(EXIT_FAILURE);
	}
	strncpy(string, buffer->stream, buffer->size);
	string[buffer->size] = '\0';
	free(buffer->stream);
	buffer->size = 0;
	return string;
}

// ------------------ FUNCIONES DE LOGGER ------------------

t_log *iniciar_logger(char *path, char *nombre, t_log_level nivel)
{
	t_log *nuevo_logger;
	nuevo_logger = log_create(path, nombre, 1, nivel);
	if (nuevo_logger == NULL)
	{
		printf("Error al crear el logger: %s", strerror(errno));
		exit(1);
	}
	log_info(nuevo_logger, "Logger creado");
	return nuevo_logger;
}

// ------------------ FUNCIONES DE CONEXION/CLIENTE ------------------

int crear_conexion(char *ip, char *puerto, t_log *logger)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
	{

		log_error(logger, "Error al conectar el socket: %s", strerror(errno));
		return -1;
	}

	freeaddrinfo(server_info);

	return socket_cliente;
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

// ------------------ FUNCIONES DE SERVIDOR ------------------

int iniciar_servidor(t_log *logger, char *puerto, char *nombre)
{
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    perror("setsockopt(SO_REUSEADDR) failed");

	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	log_info(logger, "Servidor de %s escuchando en el puerto %s", nombre, puerto);

	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log *logger)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

// ------------------ FUNCIONES DE ENVIO ----------------------

void *serializar_paquete(t_paquete *paquete, int bytes)
{
	void *magic = malloc(bytes);
	if (magic == NULL) {
    perror("malloc falló");
    exit(1);
}
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento += paquete->buffer->size;

	/*
		//Dejo esto aca es util para pritear lo que hay dentro de magic para ver si esta bien te lo de
		//			en exa pero se lo das a chatgpt y te dice lo que vale

	for (int i = 0; i < bytes; i++) {
	printf("%02X ", ((unsigned char*)magic)[i]);
	}
printf("\n");
	*/
	return magic;
}

t_paquete *crear_paquete(op_code codigo_operacion)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = codigo_operacion;

	paquete->buffer = crear_buffer();
	return paquete;
}

void enviar_codigo_operacion(op_code code, int socket)
{
	send(socket, &code, sizeof(op_code), 0);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + sizeof(op_code) + sizeof(u_int32_t);
	void *a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void enviar_mensaje(char *mensaje, int socket_cliente, op_code codigo_operacion, t_log *logger)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codigo_operacion;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + sizeof(op_code) + sizeof(int);

	void *a_enviar = serializar_paquete(paquete, bytes);
	// Printeo todo para ver que se envia
	/*printf("Codigo de operacion: %d\n", paquete->codigo_operacion);
	printf("Tamanio del buffer: %d\n", paquete->buffer->size);
	printf("Mensaje: %s\n", (char*)paquete->buffer->stream);
	printf("Tamnio del mensaje: %ld\n", strlen(mensaje)+1);
	printf("Tamanio del paquete: %d\n", bytes);
	printf("Bytes a enviar: %d\n", bytes);
	printf("Socket cliente: %d\n", socket_cliente);*/

	int resultado = send(socket_cliente, a_enviar, bytes, 0);
	if (resultado == -1)
	{
		log_error(logger, "Error al enviar mensaje al socket %d: %s", socket_cliente, strerror(errno));
	}

	free(a_enviar);
	eliminar_paquete(paquete);
}

void eliminar_paquete(t_paquete *paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

// ------------------ FUNCIONES DE RECIBIR ----------------------

op_code recibir_operacion(int socket_cliente) // de utilziar esta funcion no se podria utilizar la funcion recibir paquete ya que esta ya recibe en codigo de operacion
{
	op_code cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(op_code), MSG_WAITALL) > 0)
	{
		return cod_op;
	}
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void *recibir_buffer(int *size, int socket_cliente)
{
	void *buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);
	return buffer;
}

void recibir_mensaje(int socket_cliente, t_log *logger)
{
	int size;
	char *buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	free(buffer);
}

char *recibir_mensaje_guardar_variable(int socket_cliente)
{
	int size;
	char *buffer = recibir_buffer(&size, socket_cliente);
	return buffer;
	free(buffer);
}

t_paquete *recibir_paquete(int socket_cliente)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->stream = NULL;
	paquete->buffer->size = 0;
	paquete->buffer->offset = 0;
	paquete->codigo_operacion = 0;

	int bytes_recibidos;

	bytes_recibidos = recv(socket_cliente, &(paquete->codigo_operacion), sizeof(op_code), 0);
	if (bytes_recibidos <= 0)
	{
		// El cliente se ha desconectado o ha ocurrido un error
		free(paquete->buffer);
		free(paquete);
		return NULL;
	}

	bytes_recibidos = recv(socket_cliente, &(paquete->buffer->size), sizeof(uint32_t), 0);
	if (bytes_recibidos <= 0)
	{
		// El cliente se ha desconectado o ha ocurrido un error
		free(paquete->buffer);
		free(paquete);
		return NULL;
	}

	paquete->buffer->stream = malloc(paquete->buffer->size);
	bytes_recibidos = recv(socket_cliente, paquete->buffer->stream, paquete->buffer->size, 0);
	if (bytes_recibidos <= 0)
	{
		// El cliente se ha desconectado o ha ocurrido un error
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);
		return NULL;
	}

	return paquete;
}

// ------------------ FUNCIONES DE FINALIZACION ------------------

void terminar_programa(int conexion, t_log *logger, t_config *config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}

t_buffer *crear_buffer()
{
	t_buffer *buffer;
	buffer = malloc(sizeof(t_buffer));
	buffer->size = 0;
	buffer->stream = NULL;
	buffer->offset = 0;
	return buffer;
}

void liberar_buffer(t_buffer *buffer)
{
	if (buffer != NULL)
	{
		if (buffer->stream != NULL)
		{
			free(buffer->stream);
			buffer->stream = NULL;
		}
		free(buffer);
	}
}

void destruir_buffer(t_buffer *buffer)
{
	free(buffer->stream);
	free(buffer);
}

void buffer_add(t_buffer *buffer, void *data, uint32_t size)
{
	void *new_stream = realloc(buffer->stream, buffer->size + size);
	if (new_stream == NULL)
	{
		// handle error, e.g., by logging it and returning
		printf("Error al agregar datos al buffer");
		return;
	}
	buffer->stream = new_stream;
	memcpy(buffer->stream + buffer->offset, data, size);
	buffer->size += size;
	buffer->offset += size;
}

void buffer_read(t_buffer *buffer, void *data, uint32_t size)
{
	if (buffer == NULL || data == NULL)
	{
		printf("Manejar punteros nulos");
		return;
	}

	// Verificar límites de lectura
	if (buffer->offset + size > buffer->size)
	{
		printf("Manejar error de lectura fuera de límites");
		return;
	}

	// Leer datos desde el flujo de datos del buffer
	memcpy(data, buffer->stream + buffer->offset, size);

	// Actualizar el desplazamiento del buffer
	buffer->offset += size;
}

void imprimir_instruccion(t_instruccion *instruccion)
{
	printf("Instruccion: %d\n", instruccion->identificador);
	for (int i = 0; i < instruccion->cant_parametros; i++)
	{
		printf("Parametro %d: %s\n", i + 1, instruccion->parametros[i]);
	}
}


uint32_t espacio_parametros(t_instruccion *instruccion)
{
	uint32_t espacio = 0;
	for (int i = 0; i < instruccion->cant_parametros; i++)
	{
		espacio += strlen(instruccion->parametros[i]) + 1;
	}
	return espacio;
}

t_buffer *serializar_instruccion(t_instruccion *instruccion)
{
	t_buffer *buffer = crear_buffer();

	buffer_add(buffer, &instruccion->identificador, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->cant_parametros, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->param1_length, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->param2_length, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->param3_length, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->param4_length, sizeof(uint32_t));
	buffer_add(buffer, &instruccion->param5_length, sizeof(uint32_t));
	// for (uint32_t i = 0; i < instruccion->cant_parametros; i++)
	// {
	// 	char *parametro_actual = instruccion->parametros[i];
	//     uint32_t longitud_parametro = strlen(parametro_actual) + 1; // Incluye el terminador nulo '\0'
	//     buffer_add(buffer, parametro_actual, longitud_parametro);
	// }
	if (instruccion->cant_parametros >= 1)
	{
		buffer_add(buffer, instruccion->parametros[0], instruccion->param1_length);
	}

	if (instruccion->cant_parametros >= 2)
	{
		buffer_add(buffer, instruccion->parametros[1], instruccion->param2_length);
	}
	if (instruccion->cant_parametros >= 3)
	{
		buffer_add(buffer, instruccion->parametros[2], instruccion->param3_length);
	}
	if (instruccion->cant_parametros >= 4)
	{
		buffer_add(buffer, instruccion->parametros[3], instruccion->param4_length);
	}
	if (instruccion->cant_parametros >= 5)
	{
		buffer_add(buffer, instruccion->parametros[4], instruccion->param5_length);
	}
	//printf("breakpoint");
	return buffer;
}

t_instruccion *instruccion_deserializar(t_buffer *buffer, uint32_t offset) {
    t_instruccion *instruccion = malloc(sizeof(t_instruccion));
    buffer->offset = offset;

    buffer_read(buffer, &instruccion->identificador, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->cant_parametros, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->param1_length, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->param2_length, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->param3_length, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->param4_length, sizeof(uint32_t));
    buffer_read(buffer, &instruccion->param5_length, sizeof(uint32_t));

    uint32_t tamanio = instruccion->param1_length + instruccion->param2_length + instruccion->param3_length + instruccion->param4_length + instruccion->param5_length;
    instruccion->parametros = malloc(instruccion->cant_parametros * sizeof(char*));

    char *param_storage = malloc(tamanio);
    uint32_t param_offsets[5] = {0, instruccion->param1_length, instruccion->param1_length + instruccion->param2_length, 
                                instruccion->param1_length + instruccion->param2_length + instruccion->param3_length, 
                                instruccion->param1_length + instruccion->param2_length + instruccion->param3_length + instruccion->param4_length};

    for (uint32_t i = 0; i < instruccion->cant_parametros; i++) {
        uint32_t longitud_parametro = 0;
        switch (i) {
            case 0: longitud_parametro = instruccion->param1_length; break;
            case 1: longitud_parametro = instruccion->param2_length; break;
            case 2: longitud_parametro = instruccion->param3_length; break;
            case 3: longitud_parametro = instruccion->param4_length; break;
            case 4: longitud_parametro = instruccion->param5_length; break;
            default: break;
        }

        instruccion->parametros[i] = param_storage + param_offsets[i];
        buffer_read(buffer, instruccion->parametros[i], longitud_parametro);
    }

    return instruccion;
}

t_buffer *serializar_lista_instrucciones(t_list *lista_instrucciones)
{
	t_buffer *buffer = crear_buffer();
	for (int i = 0; i < list_size(lista_instrucciones); i++)
	{
		t_instruccion *instruccion = list_get(lista_instrucciones, i);
		t_buffer *buffer_instruccion = serializar_instruccion(instruccion);
		buffer_add(buffer, buffer_instruccion->stream, buffer_instruccion->size);
		destruir_buffer(buffer_instruccion);
	}

	return buffer;
}

t_list *deserializar_lista_instrucciones(t_buffer *buffer, u_int32_t offset)
{
	t_list *lista_instrucciones = list_create();
	buffer->offset = offset;
	while (buffer->offset < buffer->size)
	{
		t_instruccion *instruccion = instruccion_deserializar(buffer, buffer->offset);
		list_add(lista_instrucciones, instruccion);
	}

	return lista_instrucciones;
}

// Función para destruir una instrucción
void destruir_instruccion(t_instruccion *instruccion)
{
	if (instruccion == NULL)
	{
		return;
	}
	for (uint32_t i = 0; i < instruccion->cant_parametros; i++)
	{
		free(instruccion->parametros[i]);
	}
	free(instruccion->parametros);
	free(instruccion);
}

void agregar_parametro_a_instruccion(t_list *parametros, t_instruccion *instruccion)
{
	int i = 0;
	if (parametros != NULL){
		while (i < instruccion->cant_parametros)
		{
			char *parametro = list_get(parametros, i);
			instruccion->parametros[i] = strdup(parametro);
			i++;
		}
	}
		
	instruccion->param1_length = 0;
	instruccion->param2_length = 0;
	instruccion->param3_length = 0;
	instruccion->param4_length = 0;
	instruccion->param5_length = 0;
	if (instruccion->cant_parametros >= 1)
		instruccion->param1_length = strlen(instruccion->parametros[0]) + 1;
	if (instruccion->cant_parametros >= 2)
		instruccion->param2_length = strlen(instruccion->parametros[1]) + 1;
	if (instruccion->cant_parametros >= 3)
		instruccion->param3_length = strlen(instruccion->parametros[2]) + 1;
	if (instruccion->cant_parametros >= 4)
		instruccion->param4_length = strlen(instruccion->parametros[3]) + 1;
	if (instruccion->cant_parametros >= 5)
		instruccion->param5_length = strlen(instruccion->parametros[4]) + 1;
}

t_instruccion *crear_instruccion(t_identificador identificador, t_list *parametros)
{
    t_instruccion *instruccionNueva = (t_instruccion*)malloc(sizeof(t_instruccion));
    

    instruccionNueva->identificador = identificador;
    instruccionNueva->cant_parametros = list_size(parametros);

    if (instruccionNueva->cant_parametros == 0) {
        instruccionNueva->parametros = NULL;
        instruccionNueva->param1_length = 0;
        instruccionNueva->param2_length = 0;
        instruccionNueva->param3_length = 0;
        instruccionNueva->param4_length = 0;
        instruccionNueva->param5_length = 0;
    } else {
        instruccionNueva->parametros = (char**)malloc(sizeof(char*) * instruccionNueva->cant_parametros);
        agregar_parametro_a_instruccion(parametros, instruccionNueva);
    }

    return instruccionNueva;
}

void agregar_instruccion_a_paquete(t_paquete *paquete, t_instruccion *instruccion)
{
	t_buffer *buffer_instruccion = serializar_instruccion(instruccion);
	buffer_add(paquete->buffer, buffer_instruccion->stream, buffer_instruccion->size);
	destruir_buffer(buffer_instruccion);
}

// -----------------------

t_buffer *serializar_solicitud_crear_proceso(t_solicitudCreacionProcesoEnMemoria *solicitud)
{
	t_buffer *buffer = crear_buffer();

	buffer_add(buffer, &solicitud->pid, sizeof(uint32_t));
	buffer_add(buffer, &solicitud->path_length, sizeof(uint32_t));
	buffer_add(buffer, solicitud->path, solicitud->path_length);

	return buffer;
}

t_list *parsear_instrucciones(FILE *archivo_instrucciones)
{
	int longitud_de_linea_maxima = 1024;
	char *line = malloc(sizeof(char) * longitud_de_linea_maxima);
	size_t len = sizeof(line);
	t_list *lista_instrucciones = list_create();
	while ((getline(&line, &len, archivo_instrucciones)) != -1)
	{
		t_list *lista_de_parametros = list_create();
		char *linea_con_instruccion = strtok(line, "\n");		  // Divide todas las líneas del archivo y me devuelve la primera
		char **tokens = string_split(linea_con_instruccion, " "); // Divide la línea en tokens separados x espacio. ["MOV", "AX", "BX"]
		int i = 1;
		while (tokens[i] != NULL)
		{
			list_add(lista_de_parametros, (void *)tokens[i]);
			i++;
		}
		t_identificador identificador = string_to_identificador(tokens[0]);
		t_instruccion *instruccion = crear_instruccion(identificador, lista_de_parametros);
		list_add(lista_instrucciones, instruccion);
		i=0;
		while (tokens[i] != NULL)
        {
            free(tokens[i]);
            i++;
        }
		free(tokens);
		list_destroy(lista_de_parametros);
		//list_destroy_and_destroy_elements(lista_de_parametros,free);
	}

	free(line);
	return lista_instrucciones;
}

t_identificador string_to_identificador(char *string)
{
	if (strcasecmp(string, "IO_FS_WRITE") == 0)
		return IO_FS_WRITE;
	if (strcasecmp(string, "IO_FS_READ") == 0)
		return IO_FS_READ;
	if (strcasecmp(string, "IO_FS_TRUNCATE") == 0)
		return IO_FS_TRUNCATE;
	if (strcasecmp(string, "IO_STDOUT_WRITE") == 0)
		return IO_STDOUT_WRITE;
	if (strcasecmp(string, "IO_STDIN_READ") == 0)
		return IO_STDIN_READ;
	if (strcasecmp(string, "SET") == 0)
		return SET;
	if (strcasecmp(string, "MOV_IN") == 0)
		return MOV_IN;
	if (strcasecmp(string, "MOV_OUT") == 0)
		return MOV_OUT;
	if (strcasecmp(string, "SUM") == 0)
		return SUM;
	if (strcasecmp(string, "SUB") == 0)
		return SUB;
	if (strcasecmp(string, "JNZ") == 0)
		return JNZ;
	if (strcasecmp(string, "IO_GEN_SLEEP") == 0)
		return IO_GEN_SLEEP;
	if (strcasecmp(string, "IO_FS_DELETE") == 0)
		return IO_FS_DELETE;
	if (strcasecmp(string, "IO_FS_CREATE") == 0)
		return IO_FS_CREATE;
	if (strcasecmp(string, "RESIZE") == 0)
		return RESIZE;
	if (strcasecmp(string, "COPY_STRING") == 0)
		return COPY_STRING;
	if (strcasecmp(string, "WAIT") == 0)
		return WAIT;
	if (strcasecmp(string, "SIGNAL") == 0)
		return SIGNAL;
	if (strcasecmp(string, "EXIT") == 0)
		return EXIT;
	return -1;
}

t_buffer *serializar_interrupcion(t_interrupcion *interrupcion)
{
	t_buffer *buffer = crear_buffer();
	buffer_add(buffer, &interrupcion->pid, sizeof(uint32_t));
	buffer_add(buffer, &interrupcion->motivo, sizeof(op_code));
	return buffer;
}

t_interrupcion *deserializar_interrupcion(t_buffer *buffer)
{
	t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
	buffer->offset = 0;
	buffer_read(buffer, &interrupcion->pid, sizeof(uint32_t));
	buffer_read(buffer, &interrupcion->motivo, sizeof(op_code));
	return interrupcion;
}

void enviar_motivo_desalojo(op_code motivo, uint32_t socket)
{
	send(socket, &motivo, sizeof(op_code), 0);
}

op_code recibir_motivo_desalojo(uint32_t socket_cliente)
{
	op_code motivo;
	if (recv(socket_cliente, &motivo, sizeof(op_code), MSG_WAITALL) > 0)
	{
		return motivo;
	}
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void enviar_interrupcion(u_int32_t pid,op_code interrupcion_code,u_int32_t socket)
{
	t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
	interrupcion->motivo = interrupcion_code;
	interrupcion->pid = pid;

	t_paquete *paquete = crear_paquete(INTERRUPTION);
	buffer_add(paquete->buffer, &interrupcion->pid, sizeof(uint32_t));
	buffer_add(paquete->buffer, &interrupcion->motivo, sizeof(op_code));
	enviar_paquete(paquete, socket);
	free(interrupcion);
	eliminar_paquete(paquete);
}

op_code tipo_interfaz_to_cod_op(cod_interfaz tipo){
    switch (tipo)
    {
    case GENERICA:
        return INTERFAZ_GENERICA;
    case STDIN:
        return INTERFAZ_STDIN;
    case STDOUT:
        return INTERFAZ_STDOUT;
    case DIALFS:
        return INTERFAZ_DIALFS;
    default:
        return -1;
    }
}

cod_interfaz cod_op_to_tipo_interfaz(op_code cod_op){
	switch (cod_op)
	{
	case INTERFAZ_GENERICA:
		return GENERICA;
	case INTERFAZ_STDIN:
		return STDIN;
	case INTERFAZ_STDOUT:
		return STDOUT;
	case INTERFAZ_DIALFS:
		return DIALFS;
	default:
		return -1;
	}
}



// FUNCIONES PARA ESCRIBIR O LEER EN MEMORIA
void enviar_soli_lectura(t_paquete *paquete_enviado,t_list *direcciones_fisicas,size_t tamanio_de_lectura,u_int32_t socket,u_int32_t pid)
{
    uint32_t num_direcciones = (uint32_t)list_size(direcciones_fisicas);
	buffer_add(paquete_enviado->buffer, &pid, sizeof(uint32_t));
    buffer_add(paquete_enviado->buffer, &num_direcciones, sizeof(uint32_t));
    buffer_add(paquete_enviado->buffer, &tamanio_de_lectura, sizeof(uint32_t));
    
    for (size_t j = 0; j < list_size(direcciones_fisicas); j++) {
        t_direc_fisica *direccion_fisica = list_get(direcciones_fisicas, j);
        buffer_add(paquete_enviado->buffer, &(direccion_fisica->direccion_fisica), sizeof(uint32_t));
        buffer_add(paquete_enviado->buffer, &(direccion_fisica->desplazamiento_necesario), sizeof(uint32_t));
    }

    //buffer_add(paquete_enviado->buffer, &size_of_element, sizeof(uint32_t)); // Tamaño de lectura//creo que no es necesario
    enviar_paquete(paquete_enviado, socket);
    eliminar_paquete(paquete_enviado);

}

void enviar_soli_escritura(t_paquete *paquete,t_list *direc_fisicas,size_t tamanio,void *valor,u_int32_t socket,u_int32_t pid)
{
    uint32_t num_direcciones = (uint32_t)list_size(direc_fisicas);
	buffer_add(paquete->buffer, &pid, sizeof(uint32_t));
    buffer_add(paquete->buffer, &num_direcciones, sizeof(uint32_t));

    uint32_t offset = 0;
    for (size_t i = 0; i < list_size(direc_fisicas); i++) {
        t_direc_fisica *direc = list_get(direc_fisicas, i);
        size_t size_to_copy = (i == list_size(direc_fisicas) - 1) ? tamanio - offset : direc->desplazamiento_necesario;

        buffer_add(paquete->buffer, &(direc->direccion_fisica), sizeof(uint32_t));
        buffer_add(paquete->buffer, &(direc->desplazamiento_necesario), sizeof(uint32_t));
        buffer_add(paquete->buffer, ((char *)valor) + offset, size_to_copy);
        
        offset += direc->desplazamiento_necesario;
    }

    //buffer_add(paquete->buffer, &size_of_element, sizeof(uint32_t)); // Tamaño de escritura//creo que no es necesario
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}
void enviar_soli_lectura_sin_pid(t_paquete *paquete_enviado,t_list *direcciones_fisicas,size_t tamanio_de_lectura,u_int32_t socket)
{
    uint32_t num_direcciones = (uint32_t)list_size(direcciones_fisicas);
    buffer_add(paquete_enviado->buffer, &num_direcciones, sizeof(uint32_t));
    buffer_add(paquete_enviado->buffer, &tamanio_de_lectura, sizeof(uint32_t));
    
    for (size_t j = 0; j < list_size(direcciones_fisicas); j++) {
        t_direc_fisica *direccion_fisica = list_get(direcciones_fisicas, j);
        buffer_add(paquete_enviado->buffer, &(direccion_fisica->direccion_fisica), sizeof(uint32_t));
        buffer_add(paquete_enviado->buffer, &(direccion_fisica->desplazamiento_necesario), sizeof(uint32_t));
    }

}

void enviar_soli_escritura_sin_pid(t_paquete *paquete,t_list *direc_fisicas,size_t tamanio,void *valor,u_int32_t socket)
{
    uint32_t num_direcciones = (uint32_t)list_size(direc_fisicas);
    buffer_add(paquete->buffer, &num_direcciones, sizeof(uint32_t));

    uint32_t offset = 0;
    for (size_t i = 0; i < list_size(direc_fisicas); i++) {
        t_direc_fisica *direc = list_get(direc_fisicas, i);
        size_t size_to_copy = (i == list_size(direc_fisicas) - 1) ? tamanio - offset : direc->desplazamiento_necesario;

        buffer_add(paquete->buffer, &(direc->direccion_fisica), sizeof(uint32_t));
        buffer_add(paquete->buffer, &(direc->desplazamiento_necesario), sizeof(uint32_t));
        buffer_add(paquete->buffer, ((char *)valor) + offset, size_to_copy);
        
        offset += direc->desplazamiento_necesario;
    }

}
char* estado_to_string(estados estado)
{
	switch (estado){
		case NEW:
			return "NEW";
		case READY:
			return "READY";
		case EXEC:
			return "EXEC";
		case BLOCKED:
			return "BLOCKED";
		case TERMINATED:
			return "TERMINATED";
		default:
			return NULL;
	}
}

char* number_to_string(int number) {
	return string_itoa(number);
}


const char* identificador_to_string(t_identificador id) {
    switch (id) {
        // 5 PARÁMETROS
        case IO_FS_WRITE: return "IO_FS_WRITE";
        case IO_FS_READ: return "IO_FS_READ";
        // 3 PARAMETROS
        case IO_FS_TRUNCATE: return "IO_FS_TRUNCATE";
        case IO_STDOUT_WRITE: return "IO_STDOUT_WRITE";
        case IO_STDIN_READ: return "IO_STDIN_READ";
        // 2 PARAMETROS
        case SET: return "SET";
        case MOV_IN: return "MOV_IN";
        case MOV_OUT: return "MOV_OUT";
        case SUM: return "SUM";
        case SUB: return "SUB";
        case JNZ: return "JNZ";
        case IO_GEN_SLEEP: return "IO_GEN_SLEEP";
        case IO_FS_DELETE: return "IO_FS_DELETE";
        case IO_FS_CREATE: return "IO_FS_CREATE";
        // 1 PARAMETRO
        case RESIZE: return "RESIZE";
        case COPY_STRING: return "COPY_STRING";
        case WAIT: return "WAIT";
        case SIGNAL: return "SIGNAL";
        // 0 PARAMETROS
        case EXIT: return "EXIT";
        // Manejo de caso por defecto
        default: return "UNKNOWN_IDENTIFIER";
    }
}
const char* op_code_to_string(op_code code) {
    switch (code) {
        case SOLICITUD_INSTRUCCION: return "SOLICITUD_INSTRUCCION";
        case INSTRUCCION: return "INSTRUCCION";
        case CREAR_PROCESO: return "CREAR_PROCESO";
        case FINALIZAR_PROCESO: return "FINALIZAR_PROCESO";
        case CPU: return "CPU";
        case KERNEL: return "KERNEL";
        case MEMORIA: return "MEMORIA";
        case ENTRADA_SALIDA: return "ENTRADA_SALIDA";
        case PCB: return "PCB";
        case INTERRUPTION: return "INTERRUPTION";
        case PRUEBA: return "PRUEBA";
        case INTERFAZ_DIALFS: return "INTERFAZ_DIALFS";
        case INTERFAZ_STDIN: return "INTERFAZ_STDIN";
        case INTERFAZ_STDOUT: return "INTERFAZ_STDOUT";
        case INTERFAZ_GENERICA: return "INTERFAZ_GENERICA";
        case FIN_OPERACION_IO: return "FIN_OPERACION_IO";
        case END_PROCESS: return "END_PROCESS";
        case FIN_CLOCK: return "FIN_CLOCK";
        case KILL_PROCESS: return "KILL_PROCESS";
        case OPERACION_IO: return "OPERACION_IO";
        case IO_SUCCESS: return "IO_SUCCESS";
        case EJECUTAR_IO: return "EJECUTAR_IO";
        case INTERRUPCION_CLOCK: return "INTERRUPCION_CLOCK";
        case CERRAR_IO: return "CERRAR_IO";
        case ACCESO_TABLA_PAGINAS: return "ACCESO_TABLA_PAGINAS";
        case AJUSTAR_TAMANIO_PROCESO: return "AJUSTAR_TAMANIO_PROCESO";
        case OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case ESCRITURA_MEMORIA: return "ESCRITURA_MEMORIA";
        case LECTURA_MEMORIA: return "LECTURA_MEMORIA";
        case OK: return "OK";
        case MEMORIA_LEIDA: return "MEMORIA_LEIDA";
        case PAGINA_A_MARCO: return "PAGINA_A_MARCO";
        case MARCO: return "MARCO";
        case WAIT_SOLICITADO: return "WAIT_SOLICITADO";
        case SIGNAL_SOLICITADO: return "SIGNAL_SOLICITADO";
        case INVALID_RESOURCE: return "INVALID_RESOURCE";
        case SUCCESS: return "SUCCESS";
        case INVALID_INTERFACE: return "INVALID_INTERFACE";
        case RESOURCE_FAIL: return "RESOURCE_FAIL";
        case RESOURCE_OK: return "RESOURCE_OK";
		case INTERRUPTED_BY_USER: return "INTERRUPTED_BY_USER";
        default: return "UNKNOWN_OPCODE";
    }
}
void liberar_t_instruccion(t_instruccion *instruccion)
{
    if (instruccion != NULL) {
        // Si los parámetros fueron asignados, liberarlos
        if (instruccion->parametros != NULL) {
            if (instruccion->cant_parametros > 0 && instruccion->parametros[0] != NULL) {
                free(instruccion->parametros[0]); // Liberar el bloque de memoria contiguo
            }
            free(instruccion->parametros); // Liberar el array de punteros
        }
        // Liberar la estructura
        free(instruccion);
    }
}
void liberar_t_instruccion_memoria(t_instruccion *instruccion)
{
    // Liberar cada uno de los parámetros
    for (uint32_t i = 0; i < instruccion->cant_parametros; i++)
    {
        free(instruccion->parametros[i]);
    }

    // Liberar el array de parámetros
    free(instruccion->parametros);

    // Liberar la estructura
    free(instruccion);
}
char* agregar_prefijo(const char *prefijo, const char *ruta)
{
    size_t longitud_prefijo = strlen(prefijo);
    size_t longitud_ruta = strlen(ruta);
    char *ruta_completa = malloc(longitud_prefijo + longitud_ruta + 1); // +1 para el carácter nulo
    if (ruta_completa == NULL) {
        fprintf(stderr, "Error al asignar memoria\n");
        exit(EXIT_FAILURE);
    }
    strcpy(ruta_completa, prefijo);
    strcat(ruta_completa, ruta);
    return ruta_completa;
}
