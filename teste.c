//Estados
//0 -> Balanceado x~2 y~2 
//1 -> Balanceado x~1 y~1
//2 -> Tranquilo x~2 y~1
//3 -> Ocioso x~4 y~1
//4 -> Crítico x~1 y~5
#define STATE 5


//variaveis
#define N_PARKING_SPACES 100 // número de vagas
#define N_ACCESS_GATES 20 // cancelas de entrada
#define N_OUTPUT_GATES 20 // cancelas de saida
#define N_CARS 150


//bibliotecas
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include "time.h"
#include <sys/time.h>
#include <sys/resource.h>

//semaforos
sem_t s_barriers_in;
sem_t s_barriers_out;
sem_t s_park_spaces;
sem_t m_park_space;

typedef struct car_object{
	uint16_t id;
	uint16_t park_time;
	uint16_t time_waiting;
} Car_t;

//variavel global compartilhada entre as threads
uint8_t u8_available_parks_spaces = N_PARKING_SPACES;

const uint8_t u8_AwaitTimeMax[] = {2, 1, 2, 4, 1, 1};
const uint8_t u8_StayTimeMax[] = {2, 1, 1, 1, 5, 200};

uint64_t time_used () {
		struct rusage ru;
		struct timeval t;
		getrusage(RUSAGE_SELF,&ru);
		t = ru.ru_utime;
		return (uint64_t) t.tv_sec;
}

void *car_thread_body(void	*car){

	// cast do argumento para Car_t*
	Car_t* t_arrived_car = (Car_t*)car;
	
	// carro chegando
	printf("o carro %d chegou.\n", t_arrived_car->id);
	
	// tempos auxiliares
	uint64_t t0, t1;
	
	// inicializacao da contagem
	t0 = time_used();
	
	// semaforo para verificar vagas livres do estacionamento em geral
	sem_wait(&s_park_spaces);
	
	// semaforo para verificacao de cancelas de entrada desocupadas
	sem_wait(&s_barriers_in);
	
	// semaforo para verificar utilizacao do recurso u8_available_parks_spaces
	sem_wait(&m_park_space);

	// fim da contagem de tempo de espera
	t1 = time_used();
	
	// calculo do tempo de espera, em segundos
	t_arrived_car->time_waiting = (unsigned)(t1-t0);

	// o carro utilizou uma vaga na garagem, entao o 
	// numero de vagas disponiveis diminui
	u8_available_parks_spaces--;
	
	// demonstracao grafica de utilizacao de vaga
	printf("o carro %d estacionou. ele esperou por %d s e ficara por %d s.\n", t_arrived_car->id, t_arrived_car->time_waiting, t_arrived_car->park_time);
	printf("agora ha %d vagas livres\n", u8_available_parks_spaces);
	
	// liberacao do semaforo do recurso u8_available_parks_spaces
	sem_post(&m_park_space);
	
	// liberacao de cancela de entrada
	sem_post(&s_barriers_in);
	
	// o carro fica ocupando a vaga pelo tempo aleatorio dado
	sleep(t_arrived_car->park_time);

	//o tempo de permanencia acabou, o carro vai embora
	
	// semaforo para verificacao de cancelas de saida desocupadas 
	sem_wait(&s_barriers_out);
	
	// semaforo para verificar utilizacao do recurso u8_available_parks_spaces
	sem_wait(&m_park_space);
	
	// o carro esta saindo, entao uma vaga ocupada se torna desocupada
	u8_available_parks_spaces++;
	
	// demonstracao grafica da vaga desocupando
	printf("o carro %d foi embora.\n agora tem %d vagas livres\n", t_arrived_car->id, u8_available_parks_spaces);
	
	// liberar semaforo do recurso u8_available_parks_spaces
	sem_post(&m_park_space);
	
	// liberar cancela de saida
	sem_post(&s_barriers_out);
	
	// liberacao do estacionamento em geral
	sem_post(&s_park_spaces);
	
	// fim da thread
	pthread_exit(NULL);
}


int main(){

    // indice de id do carro
	uint16_t id;
	
	// status de retorno de threads
	int status;
	
	// variaveis que recebem o tempo randomico de permanencia e de chegada
	uint8_t stay_time, await_time;
	
	// array de threads relacionadas aos carros
	pthread_t car_thread_arr[N_CARS];

	// atributo das threads
	pthread_attr_t attr;

    sem_init(&s_barriers_in, 0, N_ACCESS_GATES);
    sem_init(&s_barriers_out, 0, N_ACCESS_GATES);
    sem_init(&s_park_spaces, 0, N_PARKING_SPACES);
    sem_init(&m_park_space, 0, 1);

    // inicia threads
	pthread_attr_init (&attr) ;
   	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    // aloca memoria para o array de structs que representam os carros
	Car_t** car_arr = (Car_t**)malloc(N_CARS * sizeof(Car_t*));

    // loop de criar carro
	for (id = 0; id < N_CARS; id++){
		
		//tempos aleatórios
		stay_time = rand() % (u8_StayTimeMax[STATE] + 1);
		await_time = rand() % (u8_AwaitTimeMax[STATE] + 1);
		
		// alocar memoria para a struct carr_arr[id]
		car_arr[id] = (Car_t*)malloc(sizeof(Car_t));
		
		// identidade do carro
		car_arr[id]->id = id;
		// tempo de permanencia do carro
		car_arr[id]->park_time = stay_time;
		// e zerando seu contador de tempo de espera
		car_arr[id]->time_waiting = 0;
		
		// thread que representa o carr_arr[id]
		status = pthread_create (&car_thread_arr[id], &attr, car_thread_body, (void *) car_arr[id]) ;
	
		// Erro se não conseguir criar thread do carro
		if (status)
		{
			perror ("LOG: pthread_create error\n") ;
			exit (1) ;
		}
	
		// para criar uma nova thread, deve-se esperar o tempo await_time
		sleep(await_time);
	}

    // inicializacao da media do tempo de espera 
	int mean_waiting_time = 0;
	
	// mostra tempo de espera de todos os carros e soma na media
	for(id=0; id < N_CARS; id++){
		printf("tempo de espera do carro %d: %d\n", id, car_arr[id]->time_waiting);
		mean_waiting_time += car_arr[id]->time_waiting;
	}
	
	mean_waiting_time /= N_CARS;
	printf("media do tempo de espera: %d\n", mean_waiting_time);
	
	// libera memoria alocada para todos os carros
	for (id=0; id<N_CARS; id++){
        free(car_arr[id]);
    }
    
    // libera  memoria alocada para o array de carros
    free(car_arr);
    
	// destroi atributos e das threads
	pthread_attr_destroy (&attr) ;
	pthread_exit (NULL) ;
	
	return 0;
}
