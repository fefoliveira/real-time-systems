/*
 *
 * Versao com implementação de fila de print -
 *    06/06/2025
 *
 */

#include <esp32c3_regs.h>
#include <mdk.h>

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
#define ClkT 2 // equivale a 2 segundos

#define CPU_INTR_TIMER                                                         \
  6 // Número da interrupção do timer na CPU(ajuste conforme necessário)
// SYSTIMER clock ESP32C3 = 16 MHz → 1000 ms = 16_000_000 ticks
#define Slice 16000000UL // 1 segundo
#define MaxNumberTask 4
#define NUM_TASKS 4
#define SizeTaskStack 2048 // Tamanho da pilha da tarefa
#define MAX_NKREAD_QUEUE 5 // N mero m ximo de threads esperando por leitura
#define MAX_NKPRINT_QUEUE                                                      \
  50 // Número máximo de mensagens esperando por impressão
#define MAX_NAME_LENGTH 30
unsigned int NumberTaskAdd = -1;
volatile int TaskRunning = 0;
char myName[MAX_NAME_LENGTH];
int SchedulerAlgorithm;

enum Scheduler { RR, RM, EDF };
enum Taskstates { INITIAL, READY, RUNNING, DEAD, BLOCKED };
typedef struct {
  int queue[MaxNumberTask];
  int tail;
  int head;
} ReadyList;
ReadyList ready_queue;

typedef struct {
  short count;
  int sem_queue[MaxNumberTask], tail, header;
} sem_t;

typedef struct {
  int tid;            // ID da thread esperando pela leitura
  const char *format; // Formato da entrada esperado (similar ao scanf)
  void *var;          // Argumentos onde os dados ser o armazenados
} NkReadQueueEntry;

typedef struct {
  const char *format; // Formato da entrada esperado (similar ao printf)
  char type;          // 'd', 'f', 'c', 's', '%'
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

typedef struct {
  int CallNumber;
  unsigned char *p0;
  unsigned char *p1;
  unsigned char *p2;
  unsigned char *p3;
} Parameters;
volatile Parameters kernelargs;

typedef struct {
  int16_t Tid;
  const char *name;
  unsigned short Prio;
  uint8_t prio;
  unsigned int Time;
  unsigned short Join;
  unsigned short State;
  uint8_t Stack[SizeTaskStack]; // Vetor de pilha
  uint8_t *P;                   // Ponteiro de pilha

  // Variaveis para medicao de tempo com o systimer_baremetal
  uint64_t last_start_time;
  uint64_t current_run_time;
  uint64_t wcet;
} TaskDescriptor;
TaskDescriptor Descriptors[MaxNumberTask]; // Array de descritores de tarefas

// Aparentemente estava quebrando sem os paddings (mas revisar)
typedef struct {
  uint32_t ra, t0, t1, t2;
  uint32_t s0, s1;
  uint32_t a0, a1, a2, a3, a4, a5, a6, a7;
  uint32_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  uint32_t t3, t4, t5, t6;
  uint32_t mstatus, mepc;
  uint32_t padding1, padding2;
} Context;
/*
 *Servicos do kernel
 *
 */
enum sys_temCall {
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

void kernel(Parameters *args) {
  kernelargs = *args;

  switch (kernelargs.CallNumber) {
  case TASKCREATE:
    sys_taskcreate((int *)kernelargs.p0, (void (*)())kernelargs.p1,
                   (uint8_t)kernelargs.p2);
    break;
  case SEM_WAIT:
    // Serial.println("SEMWAIT: ") ;
    sys_semwait((sem_t *)kernelargs.p0);
    break;
  case SEM_POST:
    sys_sempost((sem_t *)kernelargs.p0);
    break;
  case SEM_INIT:
    // Serial.println("SEMINIT: ") ;
    sys_seminit((sem_t *)kernelargs.p0, (int)kernelargs.p1);
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
    sys_nkprint((char *)kernelargs.p0, (void *)kernelargs.p1);
    break;
  case GETMYNUMBER:
    sys_getmynumber((int *)kernelargs.p0);
    break;
  case NKREAD:
    sys_nkread((char *)kernelargs.p0, (void *)kernelargs.p1);
    break;
  default:
    break;
  }
}

// --- MACROS DE CONTEXTO ---
#define SAVE_CONTEXT                                                           \
  "addi sp, sp, -128 \n\t"                                                     \
  "sw ra, 0(sp) \n\t"                                                          \
  "sw t0, 4(sp) \n\t"                                                          \
  "sw t1, 8(sp) \n\t"                                                          \
  "sw t2, 12(sp) \n\t"                                                         \
  "sw s0, 16(sp) \n\t"                                                         \
  "sw s1, 20(sp) \n\t"                                                         \
  "sw a0, 24(sp) \n\t"                                                         \
  "sw a1, 28(sp) \n\t"                                                         \
  "sw a2, 32(sp) \n\t"                                                         \
  "sw a3, 36(sp) \n\t"                                                         \
  "sw a4, 40(sp) \n\t"                                                         \
  "sw a5, 44(sp) \n\t"                                                         \
  "sw a6, 48(sp) \n\t"                                                         \
  "sw a7, 52(sp) \n\t"                                                         \
  "sw s2, 56(sp) \n\t"                                                         \
  "sw s3, 60(sp) \n\t"                                                         \
  "sw s4, 64(sp) \n\t"                                                         \
  "sw s5, 68(sp) \n\t"                                                         \
  "sw s6, 72(sp) \n\t"                                                         \
  "sw s7, 76(sp) \n\t"                                                         \
  "sw s8, 80(sp) \n\t"                                                         \
  "sw s9, 84(sp) \n\t"                                                         \
  "sw s10, 88(sp) \n\t"                                                        \
  "sw s11, 92(sp) \n\t"                                                        \
  "sw t3, 96(sp) \n\t"                                                         \
  "sw t4, 100(sp) \n\t"                                                        \
  "sw t5, 104(sp) \n\t"                                                        \
  "sw t6, 108(sp) \n\t"                                                        \
  "csrr t0, mstatus \n\t"                                                      \
  "sw t0, 112(sp) \n\t"                                                        \
  "csrr t0, mepc \n\t"                                                         \
  "sw t0, 116(sp) \n\t"

#define RESTORE_CONTEXT                                                        \
  "lw t0, 116(sp) \n\t"                                                        \
  "csrw mepc, t0 \n\t"                                                         \
  "lw t0, 112(sp) \n\t"                                                        \
  "csrw mstatus, t0 \n\t"                                                      \
  "lw ra, 0(sp) \n\t"                                                          \
  "lw t0, 4(sp) \n\t"                                                          \
  "lw t1, 8(sp) \n\t"                                                          \
  "lw t2, 12(sp) \n\t"                                                         \
  "lw s0, 16(sp) \n\t"                                                         \
  "lw s1, 20(sp) \n\t"                                                         \
  "lw a0, 24(sp) \n\t"                                                         \
  "lw a1, 28(sp) \n\t"                                                         \
  "lw a2, 32(sp) \n\t"                                                         \
  "lw a3, 36(sp) \n\t"                                                         \
  "lw a4, 40(sp) \n\t"                                                         \
  "lw a5, 44(sp) \n\t"                                                         \
  "lw a6, 48(sp) \n\t"                                                         \
  "lw a7, 52(sp) \n\t"                                                         \
  "lw s2, 56(sp) \n\t"                                                         \
  "lw s3, 60(sp) \n\t"                                                         \
  "lw s4, 64(sp) \n\t"                                                         \
  "lw s5, 68(sp) \n\t"                                                         \
  "lw s6, 72(sp) \n\t"                                                         \
  "lw s7, 76(sp) \n\t"                                                         \
  "lw s8, 80(sp) \n\t"                                                         \
  "lw s9, 84(sp) \n\t"                                                         \
  "lw s10, 88(sp) \n\t"                                                        \
  "lw s11, 92(sp) \n\t"                                                        \
  "lw t3, 96(sp) \n\t"                                                         \
  "lw t4, 100(sp) \n\t"                                                        \
  "lw t5, 104(sp) \n\t"                                                        \
  "lw t6, 108(sp) \n\t"                                                        \
  "addi sp, sp, 128 \n\t"                                                      \
  "mret \n\t"

void timer_interrupt_clear() {
  SYSTIMER_INT_CLR = (1U << 0);
  (void)SYSTIMER_INT_ST;
}

uint32_t handle_timer(uint32_t current_sp) {
  // Salva o SP da tarefa atual no seu descritor para retornar depois
  Descriptors[TaskRunning].P = (uint8_t *)current_sp;
  timer_interrupt_clear();

  wakeUP();
  serialEvent();
  switchTaskUnsafe();
  processPrintQueue();

  return (uint32_t)Descriptors[TaskRunning].P;
}

uint32_t handle_ecall(uint32_t current_sp) {
  uint32_t mcause_val;
  // Lê o registrador que nos diz O QUE causou a interrupção
  __asm__ volatile("csrr %0, mcause" : "=r"(mcause_val));

  // mcause 8 = Syscall de Usuário | mcause 11 = Syscall de Máquina
  if (mcause_val == 8 || mcause_val == 11) {

    Context *ctx = (Context *)current_sp;
    ctx->mepc += 4; // mepc em exceção aponta para a instrução ECALL, então PC é
                    // incrementado para evitar loop infinito.

    Parameters *args = (Parameters *)ctx->a0;

    // Se ecall não foi chamada por uma tarefa (TaskRunning == 0), o kernel
    // retorna pra próxima linha (utilizado na criação de tasks no main)
    if (TaskRunning == 0) {
      kernel(args);
      return current_sp;
    }

    Descriptors[TaskRunning].P = (uint8_t *)current_sp;
    kernel(args);
    return (uint32_t)Descriptors[TaskRunning].P;
  } else {

    // Outra exceção encontrada!
    Context *ctx = (Context *)current_sp;
    printf("\n\n=== EXCEÇÃO DETECTADA ===\n");
    printf("Codigo mcause : %u\n", mcause_val);
    printf("mepc (Erro em): 0x%08x\n", ctx->mepc);
    printf("TaskRunning   : %d\n", TaskRunning);

    __asm__ volatile("csrci mstatus, 0x8");
    while (1)
      ;
  }
}

__attribute__((naked, section(".iram1"))) void timer_vector(void) {
  __asm__ volatile(
      SAVE_CONTEXT
      "mv a0, sp \n\t"         // Passa o SP atual como argumento
      "call handle_timer \n\t" // Chama a lógica C
      "mv sp, a0 \n\t" // Assume o SP retornado (pode ser de outra tarefa)
      RESTORE_CONTEXT);
}

__attribute__((naked, section(".iram1"))) void ecall_vector(void) {
  __asm__ volatile(
      SAVE_CONTEXT
      "mv a0, sp \n\t"         // Passa o SP atual como argumento
      "call handle_ecall \n\t" // Chama a lógica C
      "mv sp, a0 \n\t" // Assume o SP retornado (pode ser de outra tarefa)
      RESTORE_CONTEXT);
}

void callsvc(Parameters *args) {
  // 1. Força o compilador a colocar o ponteiro 'args' no registrador a0.
  // O registrador a0 é o padrão (ABI) para passar o primeiro argumento.
  register Parameters *a0 asm("a0") = args;

  // 2. Chama a exceção de hardware "Environment Call" (ecall).
  // O hardware automaticamente pula para o 'ecall_vector'
  __asm__ volatile("ecall" : : "r"(a0) : "memory");
}

// Função para iniciar a primeira tarefa. O ponteiro da pilha é enviado para a0
// (procure ABI=ilp32 para mais informações)
//  O atributo naked é crucial para evitar que o compilador adicione
//  prólogo/epílogo, dando controle total sobre o que acontece no início da
//  tarefa.
__attribute__((naked)) void startOS(uint8_t *initial_sp) {
  __asm__ volatile("mv sp, a0 \n\t" RESTORE_CONTEXT);
}

// Vetor de interrupção, sem prólogo/epílogo, alinhado em 256 bytes para
// garantir que cada ID de interrupção seja corretamente mapeada para seu
// handler.
__attribute__((naked, section(".iram1"), aligned(256))) void
vector_table(void) {
  __asm__ volatile(
      ".option push\n"  // salva opções atuais (antes de tirar o norvc)
      ".option norvc\n" // sem otimização de instruções comprimidas para
                        // garantir que cada salto seja exatamente 4 bytes (uma
                        // instrução RISC-V normal)

      "j ecall_vector\n" // Handler de ecall (endereço base mtvec)
      "j .\n"
      "j .\n"
      "j .\n"
      "j .\n"
      "j .\n"
      "j timer_vector\n" // Handler de timer, id configurado na inialização do
                         // SYSTIMER

      ".option pop\n" // restaura opções anteriores (antes do norvc ser ativado
  );
}

// SYSTIMER INIT — Modo período, procedimento exato do (Technical Reference
// Manual) TRM 10.5.3

static void systimer_init(void) {
  // Habilita clock do systimer geral e habilita a execução da UNIT0.
  SYSTIMER_CONF = (1U << 31) | (1U << 30);

  // Configura comparador 0, para timer da UNIT0 e período definido por Slice
  SYSTIMER_TARGET0_CONF = Slice & 0x3FFFFFF;

  // BIT 1 do COMP0_LOAD para sincronizar o período ao COMP0
  SYSTIMER_COMP0_LOAD = 1;

  // limpa e depois seta PERIOD_MODE (bit30) para entrar em modo período
  SYSTIMER_TARGET0_CONF = Slice & 0x3FFFFFF;                // PERIOD_MODE=0
  SYSTIMER_TARGET0_CONF = (1U << 30) | (Slice & 0x3FFFFFF); // PERIOD_MODE=1

  // habilita COMP0 via CONF_REG bit23 (TARGET0_WORK_EN)
  SYSTIMER_CONF = (1U << 31) | (1U << 30) | (1U << 24);

  // Limpa e habilita interrupção do COMP0 (bit0)
  SYSTIMER_INT_CLR = 1U;
  SYSTIMER_INT_ENA = 1U;

  // Roteamento INTMTX_SYSTIMER_T0 para CPU_INTR_TIMER e habilitação da
  // interrupção no controlador
  INTMTX_SYSTIMER_T0_MAP = CPU_INTR_TIMER;
  INTCTL_ENABLE |= (1U << CPU_INTR_TIMER);
  INTCTL_PRI(CPU_INTR_TIMER) =
      2; // Prioridade média-baixa para não atrapalhar outras ISRs críticas
}

void wakeUP() // acorda a task bloqueada a espera de passagem de tempo
{
  int i = 1;
  for (i = 1; i < NUM_TASKS; i++) {
    // sleep
    if (Descriptors[i].Time > 0) {
      Descriptors[i].Time--;
      if (Descriptors[i].Time <= 0 && Descriptors[i].State == BLOCKED) {
        Descriptors[i].State = READY;
        InsertReadyList(i); // tempo de espera se esgotou
      }
    }
  }
}

// Escalonador

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

uint64_t get_systimer_baremetal() {
  // PASSO 1: Solicita o snapshot (Setar bit 30)
  SYSTIMER_UNIT0_OP = (1U << 30);

  // PASSO 2: Pequena espera para sincronização do hardware (similar ao spin(1)
  // do mdk.h)
  for (volatile int i = 0; i < 10; i++) {
    asm volatile("nop");
  }

  // PASSO 3: Lê os registradores de sombra
  uint32_t low_bits = SYSTIMER_UNIT0_VALUE_LO;
  uint32_t high_bits = SYSTIMER_UNIT0_VALUE_HI;

  // Junta as duas partes de 32 bits em uma variável de 64 bits
  uint64_t tempo_atual = ((uint64_t)high_bits << 32) | low_bits;

  return tempo_atual;
}

/*
 * Insere a task no final da Ready List
 *  sortReadyList() realizada na switchTask()
 */
void InsertReadyList(int id) {
  // verifica se já está na lista
  for (int i = 0; i < ready_queue.head; i++) {
    if (ready_queue.queue[i] == id)
      return;
  }
  ready_queue.queue[ready_queue.head] = id;
  ready_queue.head++;
}

void switchTask() {
  // saveContext(&Descriptors[TaskRunning]);

  switchTaskUnsafe();
  // restoreContext(&Descriptors[TaskRunning]);
}

void switchTaskUnsafe() {

  if (TaskRunning != 0) {
    uint64_t now = get_systimer_baremetal();
    uint64_t elapsed = now - Descriptors[TaskRunning].last_start_time;
    Descriptors[TaskRunning].current_run_time += elapsed;

    if (Descriptors[TaskRunning].State == BLOCKED ||
        Descriptors[TaskRunning].State == DEAD) {
      uint64_t ticks = Descriptors[TaskRunning].current_run_time;

      // Update WCET (Worst-Case Execution Time)
      if (ticks > Descriptors[TaskRunning].wcet) {
        Descriptors[TaskRunning].wcet = ticks;
      }

      uint32_t ms_int = (uint32_t)(ticks / 16000);
      uint32_t ms_frac_us = (uint32_t)((ticks % 16000) / 16);

      uint32_t wcet_ms_int = (uint32_t)(Descriptors[TaskRunning].wcet / 16000);
      uint32_t wcet_ms_frac_us =
          (uint32_t)((Descriptors[TaskRunning].wcet % 16000) / 16);

      printf("[TASK TIME] Task %d (%s) finished. Run: %u.%03u ms, WCET: "
             "%u.%03u ms (%u ticks)\n",
             Descriptors[TaskRunning].Tid,
             Descriptors[TaskRunning].name ? Descriptors[TaskRunning].name
                                           : "unnamed",
             (unsigned int)ms_int, (unsigned int)ms_frac_us,
             (unsigned int)wcet_ms_int, (unsigned int)wcet_ms_frac_us,
             (unsigned int)ticks);
      Descriptors[TaskRunning].current_run_time = 0;
    }

    for (int i = 0; i < ready_queue.head - 1; i++) {
      ready_queue.queue[i] = ready_queue.queue[i + 1];
    }
    ready_queue.head--;
    if (Descriptors[TaskRunning].State != BLOCKED) {
      Descriptors[TaskRunning].State = READY;
      InsertReadyList(TaskRunning);
    }
  }

  sortReadyList();
  if (ready_queue.head > 0) {
    TaskRunning = ready_queue.queue[0];
  } else {
    TaskRunning = 0;
  }

  Descriptors[TaskRunning].State = RUNNING;
  Descriptors[TaskRunning].last_start_time = get_systimer_baremetal();
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
      if (Descriptors[ready_queue.queue[j]].Prio >
          Descriptors[ready_queue.queue[j + 1]].Prio) {
        int temp = ready_queue.queue[j];
        ready_queue.queue[j] = ready_queue.queue[j + 1];
        ready_queue.queue[j + 1] = temp;
      }
    }
  }
}

/*
 *
 * Idle Process - executa quando ready list vazia
 *
 */
void idle() {
  while (1) {
  };
}
/*
 *
 * Rotinas do kernel - Sys Call
 *
 */

// Isso aqui também foi alterdo é necessário revisar
void sys_taskcreate(int *tid, void (*taskFunction)(void), uint8_t priority) {
  NumberTaskAdd++;
  *tid = NumberTaskAdd;
  Descriptors[NumberTaskAdd].Tid = *tid;
  Descriptors[NumberTaskAdd].State = READY;
  Descriptors[NumberTaskAdd].Join = 0;
  Descriptors[NumberTaskAdd].Time = 0;
  Descriptors[NumberTaskAdd].prio = priority;
  Descriptors[NumberTaskAdd].last_start_time = 0;
  Descriptors[NumberTaskAdd].current_run_time = 0;
  Descriptors[NumberTaskAdd].wcet = 0;

  // Calcula o topo da pilha alinhado em 16 bytes
  uint8_t *stackTop =
      (uint8_t *)((uintptr_t)(Descriptors[*tid].Stack + SizeTaskStack) & ~0xF);

  // Posiciona o contexto exatamente 128 bytes abaixo do topo
  Context *ctx = (Context *)(stackTop - sizeof(Context));
  memset(ctx, 0,
         sizeof(Context)); // Zera tudo (incluindo registradores de uso geral)

  ctx->mepc = (uint32_t)taskFunction; // Onde a tarefa começa a rodar
  ctx->mstatus = 0x1880; // Interrupções habilitadas, modo de máquina

  // O Stack Pointer da tarefa aponta DIRETAMENTE para a estrutura Context
  Descriptors[*tid].P = (uint8_t *)ctx;
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

void sys_getmynumber(int *number) { *number = Descriptors[TaskRunning].Tid; }

void sys_ligaled() {
  gpio_output(BUILT_IN_LED);
  gpio_write(BUILT_IN_LED, 0); // No ESP32C3, LOW acende o LED e HIGH apaga
}

void sys_desligaled() {
  gpio_output(BUILT_IN_LED);
  gpio_write(BUILT_IN_LED, 1);
}

void sys_setmyname(const char *name) { Descriptors[TaskRunning].name = name; }

void sys_getmyname(const char *name) {
  strcpy(name, Descriptors[TaskRunning].name);
}

void sys_semwait(sem_t *semaforo) {
  semaforo->count--;
  if (semaforo->count < 0) {
    semaforo->sem_queue[semaforo->tail] = TaskRunning;
    Descriptors[TaskRunning].State = BLOCKED;
    semaforo->tail++;
    if (semaforo->tail == MaxNumberTask - 1)
      semaforo->tail = 0;
    switchTask();
  }
}

void sys_sempost(sem_t *semaforo) {
  semaforo->count++;
  if (semaforo->count <= 0) {
    // Não adiona vários bloqueados na fila, apenas o primeiro
    Descriptors[semaforo->sem_queue[semaforo->header]].State = READY;
    InsertReadyList(semaforo->sem_queue[semaforo->header]);
    semaforo->header++;
    if (semaforo->header == MaxNumberTask - 1)
      semaforo->header = 0;
  }
}

void sys_seminit(sem_t *semaforo, int ValorInicial) {
  semaforo->count = ValorInicial;
  semaforo->header = 0;
  semaforo->tail = 0;
}

void sys_sleep(unsigned int segundo) {
  // Descriptors[TaskRunning].Time = segundo/ClkT;
  Descriptors[TaskRunning].Time = segundo;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();

    // select() ;
  }
}
void sys_msleep(unsigned int mili) {
  Descriptors[TaskRunning].Time = (mili / ClkT) / 1000;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
  }
}

void sys_usleep(unsigned int micro) {
  Descriptors[TaskRunning].Time = (micro / ClkT) / 1000000;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTask();
  }
}

/*
 *  calcularPrecisao( float valor) chamada pela sys_nkprint
 */
static inline int calcularPrecisao(float valor) {
  int PRECISAO_FLOAT_ARDUINO = 6;
  int precisao = 0;
  int valorInteiro = (int)valor;
  while (valorInteiro > 0) {
    valorInteiro = valorInteiro / 10;
    precisao++;
  }
  return PRECISAO_FLOAT_ARDUINO - precisao;
}

void enqueueNkPrint(int tid, const char *format, void *var) {
  while (printTailMutex == true)
    ;                    // Espera se printTailMutex estiver ocupado
  printTailMutex = true; // Bloqueia o printTailMutex

  char type = 'd';
  if (strchr(format, '%')) {
    char *percent = strchr(format, '%');
    switch (*(percent + 1)) {
    case 'd':
      type = 'd';
      break;
    case 'f':
      type = 'f';
      break;
    case 'c':
      type = 'c';
      break;
    case 's':
      type = 's';
      break;
    case '%':
      type = '%';
      break;
    }
  }

  nkprintQueue[nkprintQueueTail].format = format;
  nkprintQueue[nkprintQueueTail].type = type;

  switch (type) {
  case 'd':
    if (var != NULL)
      nkprintQueue[nkprintQueueTail].var.i = *(int *)var;
    else
      nkprintQueue[nkprintQueueTail].var.i = 0;
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
  while (printHeadMutex == true)
    ;                    // Espera se printHeadMutex estiver ocupado
  printHeadMutex = true; // Bloqueia o printHeadMutex
  NkPrintQueueEntry entry = nkprintQueue[nkprintQueueHead];
  nkprintQueueHead = (nkprintQueueHead + 1) % MAX_NKPRINT_QUEUE;
  printHeadMutex = false; // Libera o printHeadMutex
  return entry;
}

void sys_nkprint(const char *format, void *var) {
  // Adicionar a mensagem na fila de escrita
  enqueueNkPrint(Descriptors[TaskRunning].Tid, format, var);
  // switchTask();
}

void print_float(float value, int precision) {
  // 1. Lida com números negativos
  if (value < 0.0) {
    printf("-");
    value = -value;
  }

  // 2. Aplica arredondamento básico com base na precisão (se > 0.5 para a
  // próxima casa decimal)
  float rounding = 0.5;
  for (int i = 0; i < precision; ++i) {
    rounding /= 10.0;
  }
  value += rounding;

  int integer_part = (int)value;

  float fractional_part = value - (float)integer_part;

  printf("%d", integer_part);

  if (precision > 0) {
    printf(".");
    for (int i = 0; i < precision; i++) {
      fractional_part *= 10.0;
      int digit = (int)fractional_part;
      printf("%d", digit);
      fractional_part -= (float)digit;
    }
  }
}

void serial_print(const char *fmt, NkPrintQueueEntry entry) {
  float *auxfloat;
  switch (entry.type) {
  case 'd':
    printf(fmt, entry.var.i);
    break;

  case 'c':
    printf(fmt, entry.var.c);
    break;

  case 'f':
    auxfloat = (float *)&entry.var.f;

    for (const char *p = fmt; *p != '\0'; p++) {

      if (*p == '%') {
        const char *temp = p + 1;

        // Ignora possíveis modificadores (ex: %.2f, %04f, %.*f)
        // para garantir que foi selecionado o 'f' corretamente
        while (*temp == '.' || *temp == '*' || (*temp >= '0' && *temp <= '9')) {
          temp++;
        }

        if (*temp == 'f') {
          print_float(*auxfloat, calcularPrecisao(*auxfloat));

          p = temp; // Avança o ponteiro principal para pular o 'f'
          continue; // Vai para a próxima iteração do laço
        }
      }

      // Se não for o '%f', imprime o caractere normal (texto antes e depois)
      printf("%c", *p);
    }
    break;

  case 's':
    printf(fmt, entry.var.s);
    break;

  case '%':
    printf(fmt);
    break;

  default:
    printf("Formato invalido");
    break;
  }
}

void processPrintQueue() {
  while (printTailMutex)
    ;
  printTailMutex = true;
  int snapshotTail = nkprintQueueTail;
  printTailMutex = false;

  while (nkprintQueueHead != snapshotTail) {
    NkPrintQueueEntry entry = dequeueNkPrint();
    serial_print((char *)entry.format, entry);
  }
}

void sys_taskexit(void) {
  Descriptors[TaskRunning].State = BLOCKED;
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

float stringToFloat(const char *str) {
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

int stringToInt(const char *str, int isUnsigned) {
  int result = 0;
  int sign = 1;

  // Ignora possíveis espaços em branco no início
  while (*str == ' ' || *str == '\n' || *str == '\r') {
    str++;
  }

  // Verifica sinal negativo
  if (*str == '-') {
    if (!isUnsigned) {
      sign = -1;
    }
    str++;
  }

  // Converte os caracteres numéricos
  for (; *str >= '0' && *str <= '9'; str++) {
    result = result * 10 + (*str - '0');
  }

  return result * sign;
}

void serialEvent() {
  while (serial_available_usb()) {
    char c = serial_read_usb();
    if (c == '\n') {
      serialInputBuffer[serialInputIndex] = '\0'; // Termina a string
      serialInputIndex = 0;                       // Reinicia o ?ndice

      // Desbloquear a thread que esta esperando por entrada
      if (nkreadQueueHead != nkreadQueueTail) {
        NkReadQueueEntry entry = dequeueNkRead();
        if (strcmp(entry.format, "%f") == 0) {
          // Para float, usar nossa fun??o auxiliar
          *(float *)(entry.var) = stringToFloat(serialInputBuffer);
        } else if (strcmp(entry.format, "%c") == 0) {
          *(char *)entry.var = serialInputBuffer[0];
        } else if (strcmp(entry.format, "%s") == 0) {
          strcpy(entry.var, serialInputBuffer);
        } else if (strcmp(entry.format, "%d") == 0) {
          *(int *)entry.var = stringToInt(serialInputBuffer, 0);
        } else if (strcmp(entry.format, "%u") == 0) {
          *(unsigned *)entry.var = stringToInt(serialInputBuffer, 1);
        }
        Descriptors[entry.tid].State = READY;
        InsertReadyList(entry.tid);
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

void taskcreate(
    int *ID, void (*funcao)(),
    uint8_t Priority) // parametros armazenados em R0 e R1 na chamada
{
  Parameters arg;
  arg.CallNumber = TASKCREATE;
  arg.p0 = (unsigned char *)ID;
  arg.p1 = (unsigned char *)funcao;
  arg.p2 = (unsigned char *)(uint16_t)Priority;
  callsvc(&arg);
}
void start(int scheduler) {
  Parameters arg;
  arg.CallNumber = START;
  arg.p0 = (unsigned char *)scheduler;
  callsvc(&arg);
}
void semwait(sem_t *semaforo) {
  Parameters arg;
  arg.CallNumber = SEM_WAIT;
  arg.p0 = (unsigned char *)semaforo;
  callsvc(&arg);
}

void sempost(sem_t *semaforo) {
  Parameters arg;
  arg.CallNumber = SEM_POST;
  arg.p0 = (unsigned char *)semaforo;
  callsvc(&arg);
}

void seminit(sem_t *semaforo, int ValorInicial) {
  Parameters arg;
  arg.CallNumber = SEM_INIT;
  arg.p0 = (unsigned char *)semaforo;
  arg.p1 = (unsigned char *)ValorInicial;
  callsvc(&arg);
}
void setmyname(const char *name) {
  Parameters arg;
  arg.CallNumber = SETMYNAME;
  arg.p0 = (unsigned char *)name;
  callsvc(&arg);
}
void getmynumber(int *number) {
  Parameters arg;
  arg.CallNumber = GETMYNUMBER;
  arg.p0 = (unsigned char *)number;
  callsvc(&arg);
}
void getmyname(const char *name) {
  Parameters arg;
  arg.CallNumber = GETMYNAME;
  arg.p0 = (unsigned char *)name;
  callsvc(&arg);
}
void sleep(int time) {
  Parameters arg;
  arg.CallNumber = SLEEP;
  arg.p0 = (unsigned char *)time;
  callsvc(&arg);
}
void msleep(int time) {
  Parameters arg;
  arg.CallNumber = SLEEP;
  arg.CallNumber = MSLEEP;
  arg.p0 = (unsigned char *)time;
  callsvc(&arg);
}

void usleep(int time) {
  Parameters arg;
  arg.CallNumber = USLEEP;
  arg.p0 = (unsigned char *)time;
  callsvc(&arg);
}
void taskexit(void) {
  Parameters arg;
  arg.CallNumber = EXITTASK;
  callsvc(&arg);
}

void ligaled(void) {
  Parameters arg;
  arg.CallNumber = LIGALED;
  callsvc(&arg);
}

void desligaled(void) {
  Parameters arg;
  arg.CallNumber = DESLIGALED;
  callsvc(&arg);
}

void nkprint(char *fmt, void *number) {
  Parameters arg;
  arg.CallNumber = NKPRINT;
  arg.p0 = (unsigned char *)fmt;
  arg.p1 = (unsigned char *)number;
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
int i, j;
// sem_t s0;
// sem_t s1;
// sem_t s2;
// sem_t s3;

void p0() {
  printf("'p3' (Read Test) started\n");
  static int my_tid;
  getmynumber(&my_tid);

  int valor_inteiro = 0;
  float valor_float = 0.0;
  char buffer_texto[30] = "";

  while (1) {
    nkprint("Running p0 (TID: %d)\n", &my_tid);
    nkprint("\n--- Teste de Entrada (nkread) ---\n", 0);

    // semwait(&s0);

    // 1. Lendo um Inteiro
    nkprint("Digite um numero inteiro: ", 0);
    nkread("%d",
           &valor_inteiro); // A task bloqueia aqui ate o \n chegar na serial
    nkprint("Inteiro lido: %d\n", &valor_inteiro);

    ligaled();
    // sempost(&s1);

    // 2. Lendo um Float
    nkprint("Digite um numero float (ex: 3.14): ", 0);
    nkread("%f", &valor_float);
    nkprint("Float lido: %f\n", &valor_float);

    desligaled();

    // 3. Lendo uma String
    nkprint("Digite uma palavra: ", 0);
    nkread("%s", buffer_texto);
    nkprint("String lida: %s\n", buffer_texto);

    ligaled();

    nkprint("Digite uma letra: ", 0);
    nkread("%c", buffer_texto);
    nkprint("Letra lida: %c\n", buffer_texto);

    desligaled();

    nkprint("---------------------------------\n", 0);

    // Pausa para não poluir a tela imediatamente caso caia em algum loop
    sleep(2);
  }
}

void p1() {
  static int number1;
  getmynumber(&number1);
  while (1) {
    printReadyList();
    // sempost(&s0);
    // nkprint("P1 will sleep: ", 0);
    // semwait(&s1);
    sleep(10);
    nkprint("Sem running: ", 0);
  }
}

void p2() {
  static int number2;
  getmynumber(&number2);
  while (1) {
    printReadyList();
    // nkprint("P2 will sleep: ", 0);
    sleep(20);
    nkprint("P2 running: ", 0);
  }
}

void p3() {
  static int number3;
  int teste = 10;
  char teste2 = 'A';
  float teste3 = 3.14159;
  char teste4[20] = "Hello, World!";
  getmynumber(&number3);
  while (1) {
    nkprint("P3 running\n", 0);
    nkprint("int: %d\n", &teste);
    nkprint("char: %c\n", &teste2);
    nkprint("float: %f\n", &teste3);
    nkprint("percent: %%\n", 0);
    nkprint("string: %s\n", &teste4);
    sleep(2);
  }
}

/*************************************************************
 *                                                            *
 *               Setup e criar Tasks                    *
 *                                                            *
 *                                                            *
 *************************************************************/

int main(void) {
  // Desabilita o Watchdog para evitar resets inesperados durante o
  // desenvolvimento
  wdt_disable();

  // Configura o vetor de interrupção.
  __asm__ volatile("csrw mtvec, %0" ::"r"((uintptr_t)vector_table | 1));

  // Habilita interrupções globalmente (MIE=1)
  // Não é necessário pois cada tarefa tem seu próprio mstatus com MIE=1
  //__asm__ volatile ("csrsi mstatus, 0x8");

  // Verificar número das interrupções, talvez seja melhor trocar os defines
  // para esse código.
  nkprint("FakeOS \n", 0);
  nkprint("Versao 0.0 \n", 0);

  // seminit(&s0, 1);
  // seminit(&s1, 0);
  // seminit(&s2, 0);
  // seminit(&s3, 0);

  taskcreate(&tid0, idle, 100);
  taskcreate(&tid1, p0, 10);
  taskcreate(&tid2, p1, 20);
  taskcreate(&tid3, p2, 30);
  // taskcreate(&tid3,p3,1);

  start(RR);

  systimer_init();
  Descriptors[0].last_start_time = get_systimer_baremetal();

  startOS(Descriptors[0].P); // coloca a task idle para rodar (não vai executar
                             // até a interrupção, vai ficar no while (1))

  // O código do main() não deve rodar nada, pois a task idle() já deve começar
  // a executar.
  printf("Erro: main() foi executado! Isso não deveria acontecer.\n");
  while (1)
    ;
}