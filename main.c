#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mpi.h>
#include <unistd.h>
#include <string.h>

#define NUM_PROCESSES 3
#define QUEUE_SIZE 10
#define TAG_CLOCK 0

// ==================== ESTRUTURAS ====================

// Mensagem contendo relógio vetorial e destino
typedef struct {
    int clock[NUM_PROCESSES];
    int dest;  // Processo destino
} Message;

// Fila genérica para mensagens
typedef struct {
    Message buffer[QUEUE_SIZE];
    int count;
    int in;
    int out;
    pthread_mutex_t mutex;
    pthread_cond_t cond_full;
    pthread_cond_t cond_empty;  
} MessageQueue;

// Processo com relógio vetorial e duas filas
typedef struct {
    int rank;
    int clock[NUM_PROCESSES];
    MessageQueue send_queue;     
    MessageQueue receive_queue;  
    pthread_t thread_central;
    pthread_t thread_receiver;
    pthread_t thread_sender;
    int finished;  
    pthread_mutex_t finished_mutex;
} Process;

// ==================== FUNÇÕES DE FILA ====================

void init_queue(MessageQueue *q) {
    q->count = 0;
    q->in = 0;
    q->out = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond_full, NULL);
    pthread_cond_init(&q->cond_empty, NULL);
}

void destroy_queue(MessageQueue *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond_full);
    pthread_cond_destroy(&q->cond_empty);
}

// Produtor: adiciona mensagem na fila
void enqueue(MessageQueue *q, Message msg, int rank, const char *queue_name) {
    pthread_mutex_lock(&q->mutex);
    
    while (q->count == QUEUE_SIZE) {
        printf("[P%d] Fila %s cheia, aguardando...\n", rank, queue_name);
        fflush(stdout);
        pthread_cond_wait(&q->cond_full, &q->mutex);
    }
    
    q->buffer[q->in] = msg;
    q->in = (q->in + 1) % QUEUE_SIZE;
    q->count++;
    
    pthread_cond_signal(&q->cond_empty);
    pthread_mutex_unlock(&q->mutex);
}

// Consumidor: retira mensagem da fila
Message dequeue(MessageQueue *q, int rank, const char *queue_name) {
    pthread_mutex_lock(&q->mutex);
    
    while (q->count == 0) {
        printf("[P%d] Fila %s vazia, aguardando...\n", rank, queue_name);
        fflush(stdout);
        pthread_cond_wait(&q->cond_empty, &q->mutex);
    }
    
    Message msg = q->buffer[q->out];
    q->out = (q->out + 1) % QUEUE_SIZE;
    q->count--;
    
    pthread_cond_signal(&q->cond_full);
    pthread_mutex_unlock(&q->mutex);
    
    return msg;
}

// ==================== OPERAÇÕES DO RELÓGIO VETORIAL ====================

void print_clock(Process *p, const char *operation) {
    printf("[P%d] %s: (%d,%d,%d)\n", 
           p->rank, operation, p->clock[0], p->clock[1], p->clock[2]);
    fflush(stdout);
}

// EVENT: incrementa relógio local
void event_operation(Process *p) {
    p->clock[p->rank]++;
    print_clock(p, "EVENT");
}

// SEND: incrementa relógio e coloca na fila de envio
void send_operation(Process *p, int dest) {
    p->clock[p->rank]++;
    
    Message msg;
    memcpy(msg.clock, p->clock, sizeof(int) * NUM_PROCESSES);
    msg.dest = dest;
    
    printf("[P%d] SEND(%d) - colocando na fila de envio\n", p->rank, dest);
    print_clock(p, "Estado ao enviar");
    
    enqueue(&p->send_queue, msg, p->rank, "ENVIO");
}

// RECEIVE: retira da fila de recepção e atualiza relógio
void receive_operation(Process *p) {
    printf("[P%d] RECEIVE - aguardando mensagem na fila de recepção\n", p->rank);
    fflush(stdout);
    
    Message msg = dequeue(&p->receive_queue, p->rank, "RECEPÇÃO");
    
    printf("[P%d] RECEIVE - mensagem obtida\n", p->rank);
    printf("  Relógio antes: (%d,%d,%d)\n", 
           p->clock[0], p->clock[1], p->clock[2]);
    printf("  Relógio recebido: (%d,%d,%d)\n", 
           msg.clock[0], msg.clock[1], msg.clock[2]);
    
    // Atualiza relógio vetorial: V_local[i] = max(V_local[i], V_recebido[i])
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (msg.clock[i] > p->clock[i]) {
            p->clock[i] = msg.clock[i];
        }
    }
    
    // Incrementa posição própria
    p->clock[p->rank]++;
    
    printf("  Relógio depois: (%d,%d,%d)\n", 
           p->clock[0], p->clock[1], p->clock[2]);
    fflush(stdout);
}

// ==================== THREADS ====================

// Thread Central: executa a sequência de operações
void* thread_central(void *arg) {
    Process *p = (Process *)arg;
    
    printf("[P%d] Thread Central iniciada\n", p->rank);
    fflush(stdout);
    
    switch (p->rank) {
        case 0:
            // P0: EVENT() SEND(1) RECEIVE() SEND(2) RECEIVE() SEND(1) EVENT()
            event_operation(p);
            send_operation(p, 1);
            receive_operation(p);
            send_operation(p, 2);
            receive_operation(p);
            send_operation(p, 1);
            event_operation(p);
            break;
            
        case 1:
            // P1: SEND(0) RECEIVE() RECEIVE()
            send_operation(p, 0);
            receive_operation(p);
            receive_operation(p);
            break;
            
        case 2:
            // P2: EVENT() SEND(0) RECEIVE()
            event_operation(p);
            send_operation(p, 0);
            receive_operation(p);
            break;
    }
    
    printf("\n[P%d] === ESTADO FINAL: (%d,%d,%d) ===\n\n", 
           p->rank, p->clock[0], p->clock[1], p->clock[2]);
    fflush(stdout);
    
    // Sinaliza que terminou
    pthread_mutex_lock(&p->finished_mutex);
    p->finished = 1;
    pthread_mutex_unlock(&p->finished_mutex);
    
    return NULL;
}

// Thread Receptora: recebe mensagens via MPI e coloca na fila de recepção
void* thread_receiver(void *arg) {
    Process *p = (Process *)arg;
    
    printf("[P%d] Thread Receptora iniciada\n", p->rank);
    fflush(stdout);
    
    while (1) {
        // Verifica se a thread central terminou
        pthread_mutex_lock(&p->finished_mutex);
        int done = p->finished;
        pthread_mutex_unlock(&p->finished_mutex);
        
        // Usa MPI_Iprobe para verificar se há mensagens sem bloquear indefinidamente
        int flag;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_CLOCK, MPI_COMM_WORLD, &flag, &status);
        
        if (flag) {
            // Há mensagem disponível, recebe
            Message msg;
            MPI_Recv(msg.clock, NUM_PROCESSES, MPI_INT, 
                     MPI_ANY_SOURCE, TAG_CLOCK, MPI_COMM_WORLD, &status);
            
            printf("[P%d] Thread Receptora: mensagem recebida de P%d via MPI\n", 
                   p->rank, status.MPI_SOURCE);
            fflush(stdout);
            
            // Coloca na fila de recepção
            enqueue(&p->receive_queue, msg, p->rank, "RECEPÇÃO");
        } else if (done) {
            // Thread central terminou e não há mais mensagens, pode sair
            break;
        }
        
        usleep(10000); // 10ms para evitar busy waiting
    }
    
    printf("[P%d] Thread Receptora finalizada\n", p->rank);
    fflush(stdout);
    return NULL;
}

// Thread Emissora: retira da fila de envio e envia via MPI
void* thread_sender(void *arg) {
    Process *p = (Process *)arg;
    
    printf("[P%d] Thread Emissora iniciada\n", p->rank);
    fflush(stdout);
    
    while (1) {
        // Verifica se há mensagens na fila
        pthread_mutex_lock(&p->send_queue.mutex);
        int has_messages = (p->send_queue.count > 0);
        pthread_mutex_unlock(&p->send_queue.mutex);
        
        // Verifica se a thread central terminou
        pthread_mutex_lock(&p->finished_mutex);
        int done = p->finished;
        pthread_mutex_unlock(&p->finished_mutex);
        
        if (has_messages) {
            // Retira mensagem da fila de envio
            Message msg = dequeue(&p->send_queue, p->rank, "ENVIO");
            
            printf("[P%d] Thread Emissora: enviando mensagem para P%d via MPI\n", 
                   p->rank, msg.dest);
            printf("  Relógio enviado: (%d,%d,%d)\n", 
                   msg.clock[0], msg.clock[1], msg.clock[2]);
            fflush(stdout);
            
            // Envia via MPI
            MPI_Send(msg.clock, NUM_PROCESSES, MPI_INT, msg.dest, TAG_CLOCK, MPI_COMM_WORLD);
            
        } else if (done) {
            // Thread central terminou e fila vazia, pode sair
            break;
        } else {
            usleep(10000); // 10ms para evitar busy waiting
        }
    }
    
    printf("[P%d] Thread Emissora finalizada\n", p->rank);
    fflush(stdout);
    return NULL;
}

// ==================== MAIN ====================

int main(int argc, char *argv[]) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    
    if (provided < MPI_THREAD_MULTIPLE) {
        printf("ERRO: MPI não fornece suporte adequado para threads!\n");
        MPI_Finalize();
        return 1;
    }
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size != NUM_PROCESSES) {
        if (rank == 0) {
            printf("ERRO: Este programa requer exatamente %d processos!\n", NUM_PROCESSES);
            printf("Execute com: mpirun -np 3 ./etapa3\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    // Inicializa processo
    Process p;
    p.rank = rank;
    p.finished = 0;
    pthread_mutex_init(&p.finished_mutex, NULL);
    
    // Inicializa relógio vetorial
    for (int i = 0; i < NUM_PROCESSES; i++) {
        p.clock[i] = 0;
    }
    
    // Inicializa filas
    init_queue(&p.send_queue);
    init_queue(&p.receive_queue);
    
    if (rank == 0) {
        printf("=======================================================\n");
        printf("  ETAPA 3: RELÓGIOS VETORIAIS + MPI + THREADS\n");
        printf("  Modelo Produtor/Consumidor com Filas\n");
        printf("=======================================================\n\n");
        fflush(stdout);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Cria as 3 threads
    pthread_create(&p.thread_receiver, NULL, thread_receiver, &p);
    pthread_create(&p.thread_sender, NULL, thread_sender, &p);
    pthread_create(&p.thread_central, NULL, thread_central, &p);
    
    // Aguarda thread central terminar
    pthread_join(p.thread_central, NULL);
    
    // Aguarda threads auxiliares terminarem
    pthread_join(p.thread_receiver, NULL);
    pthread_join(p.thread_sender, NULL);
    
    // Limpeza
    destroy_queue(&p.send_queue);
    destroy_queue(&p.receive_queue);
    pthread_mutex_destroy(&p.finished_mutex);
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("\n=======================================================\n");
        printf("  EXECUÇÃO FINALIZADA COM SUCESSO!\n");
        printf("=======================================================\n");
    }
    
    MPI_Finalize();
    return 0;
}
