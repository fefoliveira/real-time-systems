/*
 * 
 * Versao com implementação de fila de print - 
 *    06/06/2025 
 *
*/

#include <avr/io.h>
#include <Arduino.h>
#include <TimerOne.h>
/*
*
* Vari veis do Kernel
*
*/
/*Define intervalo das interrup  es de Clock 
 * Tabela de Interrup  es :
 * 1       ClkT = 1   segundo
 * 0.1     ClkT = 100 milissegundos
 * 0.01    Clkt = 10  milissegundos
 * 0.001   Clkt = 1   milissegundo
 * 0.0001  ClkT = 100 microssegundos
 * 0.00001 ClkT = 10  microssegundos
*/
#define ClkT 2 //equivale a 2 segundos

#define Slice 1000000 //1 segundo
#define MaxNumberTask 4
#define NUM_TASKS 4
#define SizeTaskStack 128// Tamanho da pilha da tarefa
#define MAX_NKREAD_QUEUE 5 // N mero m ximo de threads esperando por leitura
#define MAX_NKPRINT_QUEUE 50 // Número máximo de mensagens esperando por impressão
#define MAX_NAME_LENGTH 30
unsigned int NumberTaskAdd=-1;
volatile int TaskRunning = 0;
char myName[MAX_NAME_LENGTH];
int  SchedulerAlgorithm ;

enum Scheduler{
  RR,
  RM,
  EDF
};
enum Taskstates{
  INITIAL,
  READY,
  RUNNING,
  DEAD,
  BLOCKED
};
typedef struct 
{
    int queue[MaxNumberTask];
    int tail;
    int head;
}ReadyList;
ReadyList ready_queue;

typedef struct 
{
  short count;
  int sem_queue[MaxNumberTask], tail, header;
}sem_t;

typedef struct {
    int tid; // ID da thread esperando pela leitura
    const char *format; // Formato da entrada esperado (similar ao scanf)
    void *var; // Argumentos onde os dados ser o armazenados
} NkReadQueueEntry;

typedef struct {
    const char *format; // Formato da entrada esperado (similar ao printf)
    char type; // 'd', 'f', 'c', 's', '%'
    union {
        int i;
        float f;
        char c;
        const char *s;
    } var; // Variável que será impressa
} NkPrintQueueEntry;

NkReadQueueEntry nkreadQueue[MAX_NKREAD_QUEUE];
int nkreadQueueHead = 0;
int nkreadQueueTail = 0;

NkPrintQueueEntry nkprintQueue[MAX_NKPRINT_QUEUE];
int nkprintQueueHead = 0;
int nkprintQueueTail = 0;
volatile bool printTailMutex;
volatile bool printHeadMutex;

char serialInputBuffer[128]; // Buffer para armazenar a entrada da serial
int serialInputIndex = 0;

typedef struct
{
  int CallNumber;
  unsigned char *p0;
  unsigned char *p1;
  unsigned char *p2;
  unsigned char *p3;
}Parameters;
volatile Parameters kernelargs ;


typedef struct {
  int16_t Tid;
  const char *name;
  unsigned short Prio;
  uint8_t Period;
  uint8_t Wcet;
  uint8_t DynamicPeriod;
  uint8_t DynamicWcet;
  unsigned int Time;
  unsigned short Join;
  unsigned short State;
  uint8_t Stack[SizeTaskStack]; // Vetor de pilha
  uint8_t* P; // Ponteiro de pilha
} TaskDescriptor;
TaskDescriptor Descriptors[MaxNumberTask]; // Array de descritores de tarefas
/* 
*
*Servicos do kernel
*
*/
enum sys_temCall{
  TASKCREATE,
  SEM_WAIT,
  SEM_POST,
  SEM_INIT,
  WRITELCDN,
  WRITELCDS,
  EXITTASK,
  SLEEP,
  MSLEEP,
  USLEEP,
  RMSSLEEP,
  LIGALED,
  DESLIGALED,
  START, 
  TASKJOIN,
  SETMYNAME,
  GETMYNAME,
  NKPRINT,
  GETMYNUMBER,
  NKREAD,
};

/*************************************************************
*                                                            *
* Rotinas do kernel                                          *
*                                                            *
*                                                            *
*************************************************************/
void kernel() {
    switch(kernelargs.CallNumber){
    case TASKCREATE: 
      sys_taskcreate((int *)kernelargs.p0,(void(*)())kernelargs.p1,(uint8_t)kernelargs.p2, (uint8_t)kernelargs.p3);
      break;
    case SEM_WAIT: 
     // Serial.println("SEMWAIT: ") ;
      sys_semwait((sem_t *)kernelargs.p0);
      break;
    case SEM_POST: 
      sys_sempost((sem_t *)kernelargs.p0);
      break;
    case SEM_INIT: 
      //Serial.println("SEMINIT: ") ;
      sys_seminit((sem_t *)kernelargs.p0,(int)kernelargs.p1);
      break;
    case WRITELCDN: // NAO TEREMOS
      // LCDcomando((int)arg->p1);
      // LCDnum((int)arg->p0);
      break;
    case WRITELCDS: // NAO TEREMOS
      // LCDcomando((int)arg->p1);
      // LCDputs((char*)arg->p0);
      break;
    case EXITTASK: 
      sys_taskexit();
      break;
    case SLEEP: 
      sys_sleep((int)kernelargs.p0);
      break;
    case MSLEEP: 
      sys_msleep((int)kernelargs.p0);
      break;
    case USLEEP: 
      sys_usleep((int)kernelargs.p0);
      break;
    case RMSSLEEP: 
      sys_rmssleep();
      break;
    case LIGALED: 
      sys_ligaled();
      break;
    case DESLIGALED: 
      sys_desligaled();
      break;
    case START: 
      sys_start((int)kernelargs.p0);
      break;
    case TASKJOIN: // NAO TEREMOS
     // sys_taskjoin((int)arg->p0);
      break;
    case SETMYNAME: 
      sys_setmyname((const char *)kernelargs.p0);
      break;
    case GETMYNAME: 
      sys_getmyname((const char *)kernelargs.p0);
      break;  
    case NKPRINT: 
       sys_nkprint((char *)kernelargs.p0,(void *)kernelargs.p1);
       break;
    case GETMYNUMBER: 
       sys_getmynumber((int *)kernelargs.p0);
       break;
    case NKREAD: 
       sys_nkread((char *)kernelargs.p0,(void *)kernelargs.p1);
       break;
    default:
       break;
  }
}
/*
* Passa a executar a rotina do kernel com interrupcoes desabiitadas
*
*/
void callsvc(Parameters *args)
{
  noInterrupts();
  kernelargs = *args ;
  kernel() ;
  interrupts();
}
/*
*
*
*/
void saveContext(TaskDescriptor* task) {
    asm volatile (
        "push r0 \n\t"
        "in r0, __SREG__ \n\t"
        "cli \n\t"
        "push r0 \n\t"
        "push r1 \n\t"
        "clr r1 \n\t"
        "push r2 \n\t"
        "push r3 \n\t"
        "push r4 \n\t"
        "push r5 \n\t"
        "push r6 \n\t"
        "push r7 \n\t"
        "push r8 \n\t"
        "push r9 \n\t"
        "push r10 \n\t"
        "push r11 \n\t"
        "push r12 \n\t"
        "push r13 \n\t"
        "push r14 \n\t"
        "push r15 \n\t"
        "push r16 \n\t"
        "push r17 \n\t"
        "push r18 \n\t"
        "push r19 \n\t"
        "push r20 \n\t"
        "push r21 \n\t"
        "push r22 \n\t"
        "push r23 \n\t"
        "push r24 \n\t"
        "push r25 \n\t"
        "push r26 \n\t"
        "push r27 \n\t"
        "push r28 \n\t"
        "push r29 \n\t"
        "push r30 \n\t"
        "push r31 \n\t"
        "in %A0, __SP_L__ \n\t"
        "in %B0, __SP_H__ \n\t"
        : "=r" (task->P)
    );
}
void restoreContext(TaskDescriptor* task) {
    asm volatile (
        "out __SP_L__, %A0 \n\t"
        "out __SP_H__, %B0 \n\t"
        "pop r31 \n\t"
        "pop r30 \n\t"
        "pop r29 \n\t"
        "pop r28 \n\t"
        "pop r27 \n\t"
        "pop r26 \n\t"
        "pop r25 \n\t"
        "pop r24 \n\t"
        "pop r23 \n\t"
        "pop r22 \n\t"
        "pop r21 \n\t"
        "pop r20 \n\t"
        "pop r19 \n\t"
        "pop r18 \n\t"
        "pop r17 \n\t"
        "pop r16 \n\t"
        "pop r15 \n\t"
        "pop r14 \n\t"
        "pop r13 \n\t"
        "pop r12 \n\t"
        "pop r11 \n\t"
        "pop r10 \n\t"
        "pop r9 \n\t"
        "pop r8 \n\t"
        "pop r7 \n\t"
        "pop r6 \n\t"
        "pop r5 \n\t"
        "pop r4 \n\t"
        "pop r3 \n\t"
        "pop r2 \n\t"
        "pop r1 \n\t"
        "pop r0 \n\t"
        "out __SREG__, r0 \n\t"
        "pop r0 \n\t"
        : : "r" (task->P)
    );
}


void wakeUP() //acorda a task bloqueada a espera de passagem de tempo
{
  int i=1;
  for(i=1;i<=NUM_TASKS; i++)
  {
    //sleep
    if(Descriptors[i].Time>0)
    {
      Descriptors[i].Time--;
      if(Descriptors[i].Time <= 0 && Descriptors[i].State == BLOCKED && Descriptors[i].State != READY)
      {
        Descriptors[i].State = READY;
        InsertReadyList(i) ; //tempo de espera se esgotou
      }
    }

    if(Descriptors[i].DynamicPeriod>0)
    {
      Descriptors[i].DynamicPeriod--;
      if(Descriptors[i].DynamicPeriod <= 0) {
        Descriptors[i].DynamicPeriod = Descriptors[i].Period;
        Descriptors[i].DynamicWcet = Descriptors[i].Wcet;
        if(i != TaskRunning && Descriptors[i].State != READY) { // ← só insere se não estiver rodando
            Descriptors[i].State = READY;
            InsertReadyList(i);
        }
      }
    }
  }

  if(Descriptors[TaskRunning].DynamicWcet>0)
  {
    Descriptors[TaskRunning].DynamicWcet--;
    if(Descriptors[TaskRunning].DynamicWcet <= 0)
    {
      Descriptors[TaskRunning].State = BLOCKED;
    }
  }
}

//Escalonador

/*
* Imprime a Ready List
* Usada para  testes
*/
void printReadyList() {
    nkprint("Ready list tasks: ", 0);
    for (int i = 0; i < ready_queue.head; i++) {
        nkprint(" Index:", 0);
        nkprint("%d", (void *)&ready_queue.queue[i]);
    }
    nkprint("\n", 0);
}


/*
* Insere a task no final da Ready List
*  sortReadyList() realizada na switchTask()
*/
void InsertReadyList(int id) {
  ready_queue.queue[ready_queue.head] = id;
  ready_queue.head++;
  
}


/*
*   
* Se a task atual n o   Idle (TaskRunning != 0), a task   removida da Ready List
* Caso n o esteja bloqueada, ela   reinserida no final da Ready List
* A remo  o   feita com o deslocamento para a esquerda 
* Chama a fun  o sortReadyList()
* Atualiza TaskRunning com a primeira task da Ready List (TaskRunning = ready_queue.queue[0])
* Se a Ready List estiver vazia, TaskRunning ser  0 (Idle) 
*
*/
void switchTask() {    
  saveContext(&Descriptors[TaskRunning]);

  if (TaskRunning != 0){
    for (int i = 0; i < ready_queue.head - 1; i++) {
      ready_queue.queue[i] = ready_queue.queue[i + 1];
    }
    ready_queue.head--;
    if (Descriptors[TaskRunning].State != BLOCKED)  {   

      // >>> ADICIONE ESTA LINHA ABAIXO <<<
      Descriptors[TaskRunning].State = READY;
      InsertReadyList(TaskRunning);   
    }
  }
  sortReadyList();
  if (ready_queue.head > 0){
    TaskRunning = ready_queue.queue[0];
  } else {
    TaskRunning = 0;
  }
  
  Descriptors[TaskRunning].State = RUNNING;
  restoreContext(&Descriptors[TaskRunning]);
}

/*
*   
* Algoritmo Bubble Sort para a reordena o da Ready List
* O crit rio de ordena  o   a prioridade (Prio) definida para a task
* Menor valor n merico indica maior prioridade
*
*/
void sortReadyList() {
  for (int i = 0; i < ready_queue.head - 1; i++) {
      for (int j = 0; j < ready_queue.head - i - 1; j++) {
          if (Descriptors[ready_queue.queue[j]].Period > Descriptors[ready_queue.queue[j + 1]].Period) {
              int temp = ready_queue.queue[j];
              ready_queue.queue[j] = ready_queue.queue[j + 1];
              ready_queue.queue[j + 1] = temp;
          }
      }
  }
}

//Trata a interrupcao do Timer

void systemContext() { //Chamada pela interrupcao do Timer
  wakeUP();
  serialEvent();
  switchTask();
  processPrintQueue();
}

/*
*
* Idle Process - executa quando ready list vazia
*
*/
void idle() {
     while (1) {
     } ;
}
/*
*
* Rotinas do kernel - Sys Call
*
*/
void sys_taskcreate(int *tid, void (*taskFunction)(void), uint8_t period, uint8_t wcet) {

    NumberTaskAdd++ ;
    *tid = NumberTaskAdd;
    Descriptors[NumberTaskAdd].Tid=*tid;
    Descriptors[NumberTaskAdd].State=READY;
    Descriptors[NumberTaskAdd].Join=0;
    Descriptors[NumberTaskAdd].Time=0 ;
    Descriptors[NumberTaskAdd].Period=period;
    Descriptors[NumberTaskAdd].Wcet=wcet;
    Descriptors[NumberTaskAdd].DynamicPeriod=period;
    Descriptors[NumberTaskAdd].DynamicWcet=wcet;
    uint8_t* stack = Descriptors[*tid].Stack + SizeTaskStack - 1;
    Descriptors[*tid].P = stack;


    *(stack--) = ((uint16_t)taskFunction) & 0xFF; // PC low byte
    *(stack--) = ((uint16_t)taskFunction >> 8) & 0xFF; // PC high byte
    *(stack--) = 0x00; // R0
    *(stack--) = 0x80; // SREG with global interrupts enabled

    for (int i = 1; i < 32; i++) {
        *(stack--) = i; // Initialize all other registers with their number
    }

    Descriptors[*tid].P = stack;
}



void sys_start(int scheduler) {
    int i;
    SchedulerAlgorithm = scheduler;
    switch (SchedulerAlgorithm) {
        case RR:
            for (i = 1; i <= NumberTaskAdd; i++) {
                InsertReadyList(i);
            }
            sortReadyList();
            break;
        default:
            break;
    }
}

void sys_getmynumber(int *number)
{
  *number=Descriptors[TaskRunning].Tid ;
}

void sys_ligaled()
{
  PORTB = PORTB | 0x20;
}

void sys_desligaled()
{
  PORTB = PORTB & 0xDF;
}

void sys_setmyname(const char *name)
{
  Descriptors[TaskRunning].name=name;
}

void sys_getmyname(const char *name)
{
  strcpy(name, Descriptors[TaskRunning].name);
}

void sys_semwait(sem_t *semaforo)
{   
    semaforo->count--;
    if(semaforo->count < 0)
    {
      semaforo->sem_queue[semaforo->tail] = TaskRunning;
      Descriptors[TaskRunning].State = BLOCKED ;
      semaforo->tail++;
      if(semaforo->tail == MaxNumberTask-1) semaforo->tail = 0;
      switchTask();
    }
}

void sys_sempost(sem_t *semaforo)
{
     semaforo->count++;
     if(semaforo->count <= 0)
     {
       Descriptors[semaforo->sem_queue[semaforo->header]].State = READY;
       InsertReadyList(semaforo->sem_queue[semaforo->header]);
       semaforo->header++;
       if(semaforo->header == MaxNumberTask-1) semaforo->header = 0;
     }
}

void sys_seminit(sem_t *semaforo, int ValorInicial)
{
  semaforo->count = ValorInicial;
  semaforo->header = 0;
  semaforo->tail = 0;
}

void sys_sleep(unsigned int segundo)
{
  //Descriptors[TaskRunning].Time = segundo/ClkT;
  Descriptors[TaskRunning].Time = (segundo*1000000)/Slice ;
  if(Descriptors[TaskRunning].Time > 0)
  {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
    
    //select() ;
  }
}
void sys_msleep(unsigned int mili)
{
  Descriptors[TaskRunning].Time = (mili/ClkT)/1000;
  if(Descriptors[TaskRunning].Time > 0)
  {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
  }
}

void sys_usleep(unsigned int micro)
{
  Descriptors[TaskRunning].Time = (micro/ClkT)/1000000;
  if(Descriptors[TaskRunning].Time > 0)
  {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
  }
}

void sys_rmssleep(void)
{
  if(Descriptors[TaskRunning].State == RUNNING)
  {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
  }
}

/*
*  calcularPrecisao( float valor) chamada pela sys_nkprint
*/
static inline int calcularPrecisao( float valor)
{
  int PRECISAO_FLOAT_ARDUINO = 6;
  int precisao = 0;
  int valorInteiro = (int)valor;
  while(valorInteiro > 0)
  {
    valorInteiro = valorInteiro / 10;
    precisao++;
  }
  return PRECISAO_FLOAT_ARDUINO - precisao;
}

void enqueueNkPrint(int tid, const char *format, void *var) {
    while(printTailMutex == true); // Espera se printTailMutex estiver ocupado
    printTailMutex = true; // Bloqueia o printTailMutex
    
    char type = 'd';
    if (strchr(format, '%')) {
      char *percent = strchr(format, '%');
      switch (*(percent + 1)) {
        case 'd': type = 'd'; break;
        case 'f': type = 'f'; break;
        case 'c': type = 'c'; break;
        case 's': type = 's'; break;
        case '%': type = '%'; break;
      }
    }

    nkprintQueue[nkprintQueueTail].format = format;
    nkprintQueue[nkprintQueueTail].type = type;

    switch (type) {
      case 'd':
        nkprintQueue[nkprintQueueTail].var.i = *(int *)var;
        break;
      case 'f':
        nkprintQueue[nkprintQueueTail].var.f = *(float *)var;
        break;
      case 'c':
        nkprintQueue[nkprintQueueTail].var.c = *(char *)var;
        break;
      case 's':
        nkprintQueue[nkprintQueueTail].var.s = (const char *)var;
        break;
      default:
        break;
    }

    nkprintQueueTail = (nkprintQueueTail + 1) % MAX_NKPRINT_QUEUE;
    printTailMutex = false; // Libera o printTailMutex
}

NkPrintQueueEntry dequeueNkPrint() {
    while(printHeadMutex == true); // Espera se printHeadMutex estiver ocupado
    printHeadMutex = true; // Bloqueia o printHeadMutex
    NkPrintQueueEntry entry = nkprintQueue[nkprintQueueHead];
    nkprintQueueHead = (nkprintQueueHead + 1) % MAX_NKPRINT_QUEUE;
    printHeadMutex = false; // Libera o printHeadMutex
    return entry;
}

void sys_nkprint(const char *format, void *var) {
  // Adicionar a mensagem na fila de escrita
  enqueueNkPrint(Descriptors[TaskRunning].Tid, format, var);
  switchTask();
}

void serial_print(char *fmt, NkPrintQueueEntry entry)
{
  int *auxint;
  float *auxfloat;
  char *auxchar;
  int size=0;
  int accuracy=1;
  
  while (*fmt)
  {
    switch(*fmt)
      {
        case '%':
          fmt++;
          switch(*fmt)
            {
              case '%':
                Serial.print(*fmt);
                break;
              case 'c':
                Serial.print(entry.var.c);
                break;
              case 's':
                Serial.print(entry.var.s);
                break;
              case 'd':
                Serial.print(entry.var.i);
                break;
              case 'f':
                auxfloat=&entry.var.f;
                int precisao = calcularPrecisao(*auxfloat);
                Serial.print(*auxfloat, precisao);
                break;
              // case '.':
              //   fmt++;
              //   while(*fmt != 'f')
              //   {
              //     size*=accuracy;
              //     size+= (*fmt - '0');
              //     accuracy*=10;
              //     fmt++;
              //   }
              //   auxfloat=number;
              //   // printfloat(*auxfloat,size);
              //   break;
              // case 'x':
              //   auxint=number;
              //   // printhexL(*auxint);
              //   break;
              // case 'X':
              //   auxint=number;
              //   // printhexU(*auxint);
              //   break;
              // case 'b':
              //   fmt++;
              //   switch(*fmt)
              //   {
              //     case 'b':
              //       size=8;
              //       break;
              //     case 'w':
              //       size=16;
              //       break;
              //     case 'd':
              //       size=32;
              //       break;
              //     default:
              //       fmt -= 2;
              //       size = 32;
              //       break;
              //   }
              //   auxint=number;
              //   // printbinary(*auxint, size);
              //   break;
              default:
                break;
            }
        break;
        case '\\':
          fmt++;
          if (*fmt == 'n')
          {
            Serial.print("\n");
          }
          else
          {
            Serial.print("\\");
          }
        break;
        default:
          Serial.print(*fmt);
        break;
      }
    fmt++;
  Serial.flush();
  //delay(100) ;
  }
}

void processPrintQueue() {
  while (printTailMutex);
  printTailMutex = true;
  int snapshotTail = nkprintQueueTail;
  printTailMutex = false;

  while (nkprintQueueHead != snapshotTail) {
    NkPrintQueueEntry entry = dequeueNkPrint();
    serial_print((char *)entry.format, entry);
  }
}


void sys_taskexit(void)
{
  Descriptors[TaskRunning].State=BLOCKED;
  switchTask();
}
void enqueueNkRead(int tid, const char *format, void *var) {
    nkreadQueue[nkreadQueueTail].tid = tid;
    nkreadQueue[nkreadQueueTail].format = format;
    nkreadQueue[nkreadQueueTail].var = var;
    nkreadQueueTail = (nkreadQueueTail + 1) % MAX_NKREAD_QUEUE;
}

NkReadQueueEntry dequeueNkRead() {
    NkReadQueueEntry entry = nkreadQueue[nkreadQueueHead];
    nkreadQueueHead = (nkreadQueueHead + 1) % MAX_NKREAD_QUEUE;
    return entry;
}

void sys_nkread(const char *format, void *var) {
  // Adicionar a thread atual na fila de leitura
  enqueueNkRead(Descriptors[TaskRunning].Tid, format, var);
  // Bloquear a thread atual
  Descriptors[TaskRunning].State = BLOCKED;
  switchTask();
}

float stringToFloat(const char* str) {
    float result = 0.0;
    float factor = 1.0;

    if (*str == '-') {
        str++;
        factor = -1.0;
    }

    // Parte inteira
    for (; *str >= '0' && *str <= '9'; str++) {
        result = result * 10.0 + (*str - '0');
    }

    // Parte fracion?ria
    if (*str == '.') {
        float fraction = 0.1;
        str++;
        for (; *str >= '0' && *str <= '9'; str++) {
            result += (*str - '0') * fraction;
            fraction *= 0.1;
        }
    }

    return result * factor;
}

void serialEvent() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            serialInputBuffer[serialInputIndex] = '\0';  // Termina a string
            serialInputIndex = 0;  // Reinicia o ?ndice

            // Desbloquear a thread que esta esperando por entrada
            if (nkreadQueueHead != nkreadQueueTail) {
                NkReadQueueEntry entry = dequeueNkRead();
                if (strcmp(entry.format, "%f") == 0) {
                    // Para float, usar nossa fun??o auxiliar
                    *(float *)(entry.var) = stringToFloat(serialInputBuffer);
                } else {
                    // Interpretar a entrada de acordo com o formato fornecido
                    sscanf(serialInputBuffer, entry.format, entry.var);
                }
                Descriptors[entry.tid].State = READY;
            }
        } else {
            if (serialInputIndex < 127) {
                serialInputBuffer[serialInputIndex++] = c;
            }
        }
    }
}


/*************************************************************
*                                                            *
* Chamadas de Sistema a N vel de usu rio                     *
*           User Call                                        *
*                                                            *
*************************************************************/

void taskcreate(int *ID,void (*funcao)(), uint8_t period, uint8_t wcet) //parametros armazenados em R0 e R1 na chamada
{
  Parameters arg;
  arg.CallNumber=TASKCREATE;
  arg.p0=(unsigned char *)ID;
  arg.p1=(unsigned char *)funcao;
  arg.p2=(unsigned char *)(uint16_t)period;
  arg.p3=(unsigned char *)(uint16_t)wcet;
  callsvc(&arg);
}
void start(int scheduler)
{
  Parameters arg;
  arg.CallNumber=START;
  arg.p0=(unsigned char *)scheduler;
  callsvc(&arg);
}
void semwait(sem_t *semaforo)
{
  Parameters arg;
  arg.CallNumber=SEM_WAIT;
  arg.p0=(unsigned char *)semaforo;
  callsvc(&arg);
}

void sempost(sem_t *semaforo)
{
  Parameters arg;
  arg.CallNumber=SEM_POST;
  arg.p0=(unsigned char *)semaforo;
  callsvc(&arg);
}

void seminit(sem_t *semaforo, int ValorInicial)
{
  Parameters arg;
  arg.CallNumber=SEM_INIT;
  arg.p0=(unsigned char *)semaforo;
  arg.p1=(unsigned char *)ValorInicial;
  callsvc(&arg);
}
void setmyname(const char *name)
{
  Parameters arg;
  arg.CallNumber=SETMYNAME;
  arg.p0=(unsigned char *)name;
  callsvc(&arg);
}
void getmynumber(int *number)
{
  Parameters arg;
  arg.CallNumber=GETMYNUMBER;
  arg.p0=(unsigned char *)number;
  callsvc(&arg);
}
void getmyname(const char *name)
{
  Parameters arg;
  arg.CallNumber=GETMYNAME;
  arg.p0=(unsigned char *)name;
  callsvc(&arg);
}
void sleep(int time)
{
  Parameters arg;
  arg.CallNumber=SLEEP;
  arg.p0=(unsigned char *)time;
  callsvc(&arg);
}
void msleep(int time)
{  
  Parameters arg;
  arg.CallNumber=SLEEP;
  arg.CallNumber=MSLEEP;
  arg.p0=(unsigned char *)time;
  callsvc(&arg);
}

void usleep(int time)
{
  Parameters arg;
  arg.CallNumber=USLEEP;
  arg.p0=(unsigned char *)time;
  callsvc(&arg);
}
void rmssleep(void)
{
  Parameters arg;
  arg.CallNumber=RMSSLEEP;
  callsvc(&arg);
}
void taskexit(void)
{
  Parameters arg;
  arg.CallNumber=EXITTASK;
  callsvc(&arg);
}

void ligaled(void)
{
  Parameters arg;
  arg.CallNumber=LIGALED;
  callsvc(&arg);
}

void desligaled(void)
{
  Parameters arg;
  arg.CallNumber=DESLIGALED;
  callsvc(&arg);
}

void nkprint(char *fmt,void *number)
{
  Parameters arg;
  arg.CallNumber=NKPRINT;
  arg.p0=(unsigned char *)fmt;
  arg.p1=(unsigned char *)number;
  callsvc(&arg);
}
void nkread(const char *format, void *var) {
    Parameters arg;
    arg.CallNumber = NKREAD;
    arg.p0 = (unsigned char *)format;
    arg.p1 = (unsigned char *)var;
    callsvc(&arg);
}
/*************************************************************
*                                                            *
*                   Programa do  usu rio                     *
*                       - Aplica  o -                        *
*                                                            *
*************************************************************/


volatile int16_t tid0, tid1, tid2, tid3, tid4;
int i, j ;
//sem_t s0;
//sem_t s1;
//sem_t s2;
//sem_t s3;

void p0() {     
  static int number0;
  getmynumber(&number0);  
  while (1) {
    //rintReadyList();
    nkprint("P0 running\n", 0);
    for(volatile long d = 0; d < (817200L); d++);
    nkprint("P0 Finished\n", 0);
    sys_rmssleep();
  }
}

void p1() {
  static int number1;
  getmynumber(&number1);
  while (1) {
    //printReadyList();
    nkprint("P1 running\n", 0);
    for(volatile long d = 0; d < 1816000L; d++);
    nkprint("P1 Finished\n", 0);
    sys_rmssleep();
  }
}

void p2() {
  static int number2;
  getmynumber(&number2);
  while (1) {
    //printReadyList();
    nkprint("P2 running\n", 0);
    for(volatile long d = 0; d < 2724000L; d++);
    nkprint("P2 Finished\n", 0);
    sys_rmssleep();
  }
}

/*************************************************************
*                                                            *
*               Setup e criar Tasks                    *
*                                                            *
*                                                            *
*************************************************************/


void setup() {
    Serial.begin(9600) ;
    nkprint("FakeOS \n", 0) ;
    nkprint("Versao 0.0 \n", 0) ;
  
    //seminit(&s0, 1);
    //seminit(&s1, 0);
    //seminit(&s2, 0);
    //seminit(&s3, 0);
 
    taskcreate(&tid0,idle,100, 50);
    taskcreate(&tid1,p0,10, 2);
    taskcreate(&tid2,p1,20, 5);
    taskcreate(&tid3,p2,30, 8);
    // taskcreate(&tid3,p3,1);
    start (RR) ; //coloca as tasks na fila

  noInterrupts(); 
  Timer1.initialize(Slice);
 // Timer1.initialize(ClkT*1000000);
  Timer1.attachInterrupt(systemContext);
  restoreContext(&Descriptors[0]); //coloca a task idle para rodar
}

void loop() {
  while (1);
}