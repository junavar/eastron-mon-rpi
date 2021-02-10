#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <stdio.h>
#include <argp.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "registro.h"

#define NOMBRE_PROGRAMA "EASTRON-MON-UI"
#define VERSION_PROGRAMA "0.97"
#define FECHA_PROGRAMA "febrero 2021"
#define AUTOR_PROGRAMA "junavar (junavarg@hotmail.com)"
#define TEXTO_PROGRAMA "Interfaz usuario del servicio de monitorizacion y control de importacion/exportacion energia electrica de red distribucion"

struct datos_instantaneos *pdatos_instantaneos;
struct linea_subscripcion *plinea_subscripcion;
int senal_recibida;

void pintar(){
	char buf[150]; //buffer para string de tiempo
	struct tm *loc_time;
	senal_recibida=1;
	// conviete tiempo registrado en tiempo local
	loc_time = localtime(&pdatos_instantaneos->marca_tiempo); // Converting current time to local time
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", loc_time);
	printf("\r");
	printf("%s ", buf);
	printf("tur:%1d ", pdatos_instantaneos->retraso);
	printf("err:%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d ",
			pdatos_instantaneos->err_potencia,
			pdatos_instantaneos->err_potencia_reactiva,
			pdatos_instantaneos->err_factor_potencia,
			pdatos_instantaneos->err_tension,
			pdatos_instantaneos->err_intensidad,
			pdatos_instantaneos->err_frecuencia,
			pdatos_instantaneos->err_energia_total_importada,
			pdatos_instantaneos->err_energia_total_exportada,
			pdatos_instantaneos->err_energia_reactiva_total_inductiva,
			pdatos_instantaneos->err_energia_reactiva_total_capacitiva);

	printf("P:%4.0fW ", pdatos_instantaneos->potencia );
	printf("Q:%4.0fvar ", pdatos_instantaneos->potencia_reactiva);
	printf("FP:%4.2f ", pdatos_instantaneos->factor_potencia);
	printf("V:%3.0fV ", pdatos_instantaneos->tension);
	printf("I:%4.1fA ", pdatos_instantaneos->intensidad);
	printf("F:%5.2fHz ", pdatos_instantaneos->frecuencia);
	printf("Ei:%3.2fkWh ", pdatos_instantaneos->energia_total_importada);
	printf("Ee:%3.2fkWh ", pdatos_instantaneos->energia_total_exportada);
	printf("Eri:%3.2fkvarh ", pdatos_instantaneos->energia_reactiva_total_inductiva);
	printf("Erc:%3.2fkvarh ", pdatos_instantaneos->energia_reactiva_total_capacitiva);
	//printf("Pavg(15min):%4.0fW ", pdatos_instantaneos->potencia_media_importada_15min);
	printf("Pavg(%3ds):%3.0fW ", pdatos_instantaneos->ventana_integracion, pdatos_instantaneos->potencia_media_importada_15min);
	printf("\r");
	fflush(stdout);
}



int main(){
	fprintf(stderr,"%s v%s %s\n", NOMBRE_PROGRAMA, VERSION_PROGRAMA, FECHA_PROGRAMA);
	fprintf(stderr,"%s\n", TEXTO_PROGRAMA);
	fprintf(stderr,"Autor: %s\n", AUTOR_PROGRAMA);

	/*
	 * Emplea el area de memoria compartida
	 */
	int shmid; // identificador de memoria compartida
	shmid = shmget(SHM_KEY, sizeof (struct datos_instantaneos),  0666);
	pdatos_instantaneos = shmat(shmid, NULL, 0);


	int shmid3; // identificador de memoria compartida para tabla de subscripcion de notificaciones
	shmid3 = shmget(SHM_KEY_3, sizeof (struct linea_subscripcion)*MAX_PIDS_PARA_SIGNAL, 0666);
	plinea_subscripcion = shmat(shmid3, NULL, 0);

	/*
	 * se subcribe el proceso en tabla para que notifique con la senal de tiempo real 1 (SIGRTMIN + 1)
	 * y se vincula con la funcion pintar()
	 */
	int i;
	// informa de los procesos ya registrados
	for (i=0;i<MAX_PIDS_PARA_SIGNAL;i++){
		if (plinea_subscripcion[i].pid!=0) {
			fprintf(stderr, "PID %d ya registrado para señal %d antes de subscripcion en posicion %d\n",
					plinea_subscripcion[i].pid, plinea_subscripcion[i].rt_senal, i);
		}
	}
	// registra el proceso actual
	for (i=0;i<MAX_PIDS_PARA_SIGNAL;i++){
		if (plinea_subscripcion[i].pid==0){
			//printf("PID registrado antes de subscripcion: %d",plinea_subscripcion[i].pid);
			plinea_subscripcion[i].pid=getpid();
			plinea_subscripcion[i].rt_senal=1;
			signal(SIGRTMIN + plinea_subscripcion[i].rt_senal, pintar);
			fprintf(stderr, "Actual proceso con PID %d registrado para señal %d en posicion %d\n",plinea_subscripcion[i].pid,plinea_subscripcion[i].rt_senal, i);
			break;
		}
	}
	// informa si no ha sido posible encontrar un hueco en la tabla y aborta ejecion con retorno -1
	if (i>=MAX_PIDS_PARA_SIGNAL){
		fprintf(stderr, "Error: No hay sitio en la tabla de procesos a enviar señal (%d posiciones reservadas)\n",MAX_PIDS_PARA_SIGNAL );
		return 1;
	}
	fprintf(stderr, "Esperando a recibir señal ");
	while (1){
		senal_recibida=0;
		sleep(1);//El programa se queda aqui siempre . Cada señal recibida ejecutará la funcion pintar()
		if (!senal_recibida) fprintf(stderr, "\r.   ");
		senal_recibida=0;
		sleep(1);
		if (!senal_recibida) fprintf(stderr, "\r..  ");
		senal_recibida=0;
		sleep(1);
		if (!senal_recibida) fprintf(stderr, "\r... ");
		senal_recibida=0;
		sleep(1);
		if (!senal_recibida) fprintf(stderr, "\r....");
	}
	return 0;
}
