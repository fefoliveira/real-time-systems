/*
 * ============================================================================
 * Projeto: fakeOS - ESP32-C3 (RISC-V)
 * Autores:
 *   - João Vítor Navarro
 *   - Fernando Oliveira
 *   - Isabela Rennhack
 *
 * Funcionalidades Implementadas:
 *   - Gerência de Memória Dinâmica (First-Fit) integrada com as syscalls
 *     nkmalloc() e nkfree() delegando para mem_malloc() e mem_free().
 *   - Protocolo de Herança de Prioridade (Priority Inheritance Protocol - PIP)
 *     nos semáforos para resolver inversão de prioridade em tarefas periódicas
 *     e aperiódicas (bus_manager_task, communications_task,
 * meteorological_task).
 * ============================================================================
 */

#include <esp32c3_regs.h>
// #include <math.h>
#include <mdk.h>
// #include <stdbool.h>
// #include <stdint.h>
/*
 * Variaveis do kernel
 */

#define ClkT 2

#define CPU_INTR_TIMER 6
#define Slice 16000000UL // 1 segundo (16 MHz tick)

#define MaxNumberTask 32
#define NUM_TASKS 10
#define SizeTaskStack 2048 // Tamanho da pilha para RISC-V
#define MAX_NKREAD_QUEUE 5
#define MAX_NKPRINT_QUEUE 50
#define MAX_NAME_LENGTH 30

unsigned int NumberTaskAdd = -1;
volatile int TaskRunning = 0;
char myName[MAX_NAME_LENGTH];
int SchedulerAlgorithm;

// Constante do limite de Liu & Layland
#define RM_SCHEDULABILITY_BOUND 0.693f

// Definicoes pro Polling Server:
#define POLLING_SERVER_PERIOD 15
#define POLLING_SERVER_EXECTIME 3
#define APERIODIC_QUEUE_SIZE 10
enum AperiodicEvent { EVENT_NONE, EVENT_P2_SERIAL };
volatile enum AperiodicEvent aperiodicFunctionQueue[APERIODIC_QUEUE_SIZE];
volatile int aperiodicQueueHead = 0;
volatile int aperiodicQueueTail = 0;

enum Scheduler { RR, RM, EDF };

enum Taskstates {
  INITIAL,
  READY,
  RUNNING,
  DEAD,
  BLOCKED,
  BLOCKED_SEM // NOVO ESTADO
};

typedef struct {
  int queue[MaxNumberTask];
  int tail;
  int head;
} ReadyList;

ReadyList ready_queue;

typedef struct {
  short count;
  int sem_queue[MaxNumberTask], tail, header;
  int ownerTid;
} sem_t;

typedef struct {
  int tid;
  const char *format;
  void *var;
} NkReadQueueEntry;

typedef struct {
  const char *format;
  char type;
  union {
    int i;
    float f;
    char c;
    const char *s;
  } var;
} NkPrintQueueEntry;

NkReadQueueEntry nkreadQueue[MAX_NKREAD_QUEUE];
int nkreadQueueHead = 0;
int nkreadQueueTail = 0;

NkPrintQueueEntry nkprintQueue[MAX_NKPRINT_QUEUE];
int nkprintQueueHead = 0;
int nkprintQueueTail = 0;
volatile bool printTailMutex;
volatile bool printHeadMutex;

char serialInputBuffer[128];
int serialInputIndex = 0;

typedef struct {
  int sensor_id;
  float temperatura;
} DadosSensor;

typedef struct {
  int CallNumber;
  unsigned char *p0;
  unsigned char *p1;
  unsigned char *p2;
  unsigned char *p3;
  unsigned char *p4;
  unsigned char *p5;
  unsigned char *p6;
} Parameters;

volatile Parameters kernelargs;

typedef struct {
  int16_t Tid;
  const char *name;
  unsigned short BasePrio;
  unsigned short Prio;
  unsigned int Time;
  unsigned short Join;
  unsigned short State;
  uint8_t Stack[SizeTaskStack];
  uint8_t *P;

  // variaveis estaticas para o escalonador RMS (RM)
  uint8_t period;
  uint8_t execTime;

  // variaveis dinamicas para o escalonador RMS (RM)
  uint8_t remainingPeriod;
  uint8_t remainingExecTime;

  // Variaveis para medicao de tempo com o systimer_baremetal
  uint64_t last_start_time;
  uint64_t current_run_time;
  uint64_t wcet;
} TaskDescriptor;

TaskDescriptor Descriptors[MaxNumberTask];

// Estrutura de Contexto RISC-V
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
 * Servicos do kernel
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
  NKMALLOC,
  NKFREE,
};

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

/*************************************************************
 * Protótipos de Funções                                     *
 *************************************************************/
void sys_taskcreate(int *tid, void (*taskFunction)(void), uint8_t period,
                    uint8_t execTime, uint8_t remainingPeriod,
                    uint8_t remainingExecTime);
void sys_semwait(sem_t *semaforo);
void sys_sempost(sem_t *semaforo);
void sys_seminit(sem_t *semaforo, int ValorInicial);
void sys_taskexit(void);
void sys_sleep(unsigned int segundo);
void sys_msleep(unsigned int mili);
void sys_usleep(unsigned int micro);
void sys_rmssleep(void);
void sys_ligaled(void);
void sys_desligaled(void);
void sys_start(int scheduler);
void sys_setmyname(const char *name);
void sys_getmyname(const char *name);
void sys_nkprint(const char *format, void *var);
void sys_getmynumber(int *number);
void sys_nkread(const char *format, void *var);
void sys_nkmalloc(void **ptr, size_t size_in_bytes);
void sys_nkfree(void *ptr);
void *mem_malloc(size_t size);
void mem_free(void *ptr);
void mem_init(void);
void *nkmalloc(size_t num_bytes);
void nkfree(void *ptr);

bool validateRMSchedulability();
float getTaskUtilization(int taskId);
float getTotalSystemUtilization();
void InsertReadyList(int id);
void printReadyList();
void switchTaskUnsafe();
void sortReadyList();
void wakeUP();
void idle();
void serialEvent();
void processPrintQueue();
void nkprint(char *fmt, void *number);
void push_aperiodic_function(enum AperiodicEvent event);

uint64_t get_systimer_baremetal();
void setmyname(const char *name);

/*************************************************************
 * Rotinas do kernel                                         *
 *************************************************************/

void kernel(Parameters *args) {
  kernelargs = *args;
  switch (kernelargs.CallNumber) {
  case TASKCREATE:
    sys_taskcreate((int *)kernelargs.p0, (void (*)())kernelargs.p1,
                   *(uint8_t *)kernelargs.p3, *(uint8_t *)kernelargs.p4,
                   *(uint8_t *)kernelargs.p5, *(uint8_t *)kernelargs.p6);
    break;
  case SEM_WAIT:
    sys_semwait((sem_t *)kernelargs.p0);
    break;
  case SEM_POST:
    sys_sempost((sem_t *)kernelargs.p0);
    break;
  case SEM_INIT:
    sys_seminit((sem_t *)kernelargs.p0, (int)kernelargs.p1);
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
  case NKMALLOC:
    sys_nkmalloc((void **)kernelargs.p0, (size_t)kernelargs.p1);
    break;
  case NKFREE:
    sys_nkfree((void *)kernelargs.p0);
    break;
  default:
    break;
  }
}

void timer_interrupt_clear() {
  SYSTIMER_INT_CLR = (1U << 0);
  (void)SYSTIMER_INT_ST;
}

/*
    GET SYSTIMER BAREMETAL -PEGAR O TEMPO DO SISTEMA- SEM DELAY
*/

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

uint32_t handle_timer(uint32_t current_sp) {
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
  __asm__ volatile("csrr %0, mcause" : "=r"(mcause_val));

  if (mcause_val == 8 || mcause_val == 11) {
    Context *ctx = (Context *)current_sp;
    ctx->mepc += 4;

    Parameters *args = (Parameters *)ctx->a0;

    if (TaskRunning == 0) {
      kernel(args);
      return current_sp;
    }

    Descriptors[TaskRunning].P = (uint8_t *)current_sp;
    kernel(args);
    return (uint32_t)Descriptors[TaskRunning].P;
  } else {
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
  __asm__ volatile(SAVE_CONTEXT "mv a0, sp \n\t"
                                "call handle_timer \n\t"
                                "mv sp, a0 \n\t" RESTORE_CONTEXT);
}

__attribute__((naked, section(".iram1"))) void ecall_vector(void) {
  __asm__ volatile(SAVE_CONTEXT "mv a0, sp \n\t"
                                "call handle_ecall \n\t"
                                "mv sp, a0 \n\t" RESTORE_CONTEXT);
}

void callsvc(Parameters *args) {
  register Parameters *a0 asm("a0") = args;
  __asm__ volatile("ecall" : : "r"(a0) : "memory");
}

__attribute__((naked)) void startOS(uint8_t *initial_sp) {
  __asm__ volatile("mv sp, a0 \n\t" RESTORE_CONTEXT);
}

__attribute__((naked, section(".iram1"), aligned(256))) void
vector_table(void) {
  __asm__ volatile(".option push\n"
                   ".option norvc\n"

                   "j ecall_vector\n"
                   "j .\n"
                   "j .\n"
                   "j .\n"
                   "j .\n"
                   "j .\n"
                   "j timer_vector\n"

                   ".option pop\n");
}

static void systimer_init(void) {
  SYSTIMER_CONF = (1U << 31) | (1U << 30);
  SYSTIMER_TARGET0_CONF = Slice & 0x3FFFFFF;
  SYSTIMER_COMP0_LOAD = 1;
  SYSTIMER_TARGET0_CONF = Slice & 0x3FFFFFF;
  SYSTIMER_TARGET0_CONF = (1U << 30) | (Slice & 0x3FFFFFF);
  SYSTIMER_CONF = (1U << 31) | (1U << 30) | (1U << 24);
  SYSTIMER_INT_CLR = 1U;
  SYSTIMER_INT_ENA = 1U;
  INTMTX_SYSTIMER_T0_MAP = CPU_INTR_TIMER;
  INTCTL_ENABLE |= (1U << CPU_INTR_TIMER);
  INTCTL_PRI(CPU_INTR_TIMER) = 2;
}

void wakeUP() {
  int i = 1;
  for (i = 1; i < NUM_TASKS; i++) {
    if (Descriptors[i].Time > 0) {
      Descriptors[i].Time--;
      if (Descriptors[i].Time <= 0 && Descriptors[i].State == BLOCKED &&
          Descriptors[i].State != READY) {
        Descriptors[i].State = READY;
        InsertReadyList(i);
      }
    }

    if (Descriptors[i].remainingPeriod > 0) {
      Descriptors[i].remainingPeriod--;

      if (Descriptors[i].remainingPeriod <= 0) {
        Descriptors[i].remainingPeriod = Descriptors[i].period;
        Descriptors[i].remainingExecTime = Descriptors[i].execTime;
        // Nova lógica para ver se a tarefa não está com bloqueio de semáforo
        if (i != TaskRunning && Descriptors[i].State != READY &&
            Descriptors[i].State != BLOCKED_SEM) {
          Descriptors[i].State = READY;
          InsertReadyList(i);
        }
      }
    }
  }

  if (Descriptors[TaskRunning].remainingExecTime > 0) {
    Descriptors[TaskRunning].remainingExecTime--;

    if (Descriptors[TaskRunning].remainingExecTime <= 0) {
      Descriptors[TaskRunning].State = BLOCKED;
    }
  }
}

/*************************************************************
 * Escalonador                                               *
 *************************************************************/

void printReadyList() {
  nkprint("Ready list tasks: ", 0);
  for (int i = 0; i < ready_queue.head; i++) {
    nkprint(" Index:", 0);
    nkprint("%d", (void *)&ready_queue.queue[i]);
  }
  nkprint("\n", 0);
}

void InsertReadyList(int id) {
  for (int i = 0; i < ready_queue.head; i++) {
    if (ready_queue.queue[i] == id)
      return;
  }
  ready_queue.queue[ready_queue.head] = id;
  ready_queue.head++;
}
void switchTaskUnsafe() {
  uint64_t now = get_systimer_baremetal();
  uint64_t elapsed = now - Descriptors[TaskRunning].last_start_time;
  Descriptors[TaskRunning].current_run_time += elapsed;

  if (TaskRunning != 0 && (Descriptors[TaskRunning].State == BLOCKED ||
                           Descriptors[TaskRunning].State == BLOCKED_SEM)) {
    uint64_t ticks = Descriptors[TaskRunning].current_run_time;
    
    // Update WCET (Worst-Case Execution Time)
    if (ticks > Descriptors[TaskRunning].wcet) {
      Descriptors[TaskRunning].wcet = ticks;
    }

    uint32_t ms_int = (uint32_t)(ticks / 16000);
    uint32_t ms_frac_us = (uint32_t)((ticks % 16000) / 16);
    
    uint32_t wcet_ms_int = (uint32_t)(Descriptors[TaskRunning].wcet / 16000);
    uint32_t wcet_ms_frac_us = (uint32_t)((Descriptors[TaskRunning].wcet % 16000) / 16);

    printf("[TASK TIME] Task %d (%s) finished. Run: %u.%03u ms, WCET: %u.%03u ms (%u ticks)\n",
           Descriptors[TaskRunning].Tid,
           Descriptors[TaskRunning].name ? Descriptors[TaskRunning].name
                                         : "unnamed",
           (unsigned int)ms_int, (unsigned int)ms_frac_us,
           (unsigned int)wcet_ms_int, (unsigned int)wcet_ms_frac_us,
           (unsigned int)ticks);
    Descriptors[TaskRunning].current_run_time = 0;
  }

  if (TaskRunning != 0) {
    // 1. Encontra onde a tarefa atual está na fila
    int indexToRemove = -1;
    for (int i = 0; i < ready_queue.head; i++) {
      if (ready_queue.queue[i] == TaskRunning) {
        indexToRemove = i;
        break;
      }
    }

    // 2. Remove apenas se encontrou, puxando os elementos
    if (indexToRemove != -1) {
      for (int i = indexToRemove; i < ready_queue.head - 1; i++) {
        ready_queue.queue[i] = ready_queue.queue[i + 1];
      }
      ready_queue.head--;
    }

    // 3. Reinsere a tarefa se ela não estiver bloqueada
    if (Descriptors[TaskRunning].State != BLOCKED &&
        Descriptors[TaskRunning].State != BLOCKED_SEM) {
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

// old switchTaskUnsafe() tava apagando a tarefa de maior prioridade após
// a tarefa aperiódica ser executada
/*
void switchTaskUnsafe()
{
  if (TaskRunning != 0)
  {
    for (int i = 0; i < ready_queue.head - 1; i++)
    {
      ready_queue.queue[i] = ready_queue.queue[i + 1];
    }
    ready_queue.head--;

    if (Descriptors[TaskRunning].State != BLOCKED)
    {
      Descriptors[TaskRunning].State = READY;
      InsertReadyList(TaskRunning);
    }
  }

  sortReadyList();

  if (ready_queue.head > 0)
  {
    TaskRunning = ready_queue.queue[0];
  }
  else
  {
    TaskRunning = 0;
  }

  Descriptors[TaskRunning].State = RUNNING;
}
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

void idle() {
  setmyname("IDLE");
  while (1) {
  };
}

/*************************************************************
 * Rotinas do kernel - Sys Call                              *
 *************************************************************/

void sys_taskcreate(int *tid, void (*taskFunction)(void), uint8_t period,
                    uint8_t execTime, uint8_t remainingPeriod,
                    uint8_t remainingExecTime) {
  NumberTaskAdd++;
  *tid = NumberTaskAdd;

  Descriptors[NumberTaskAdd].Tid = *tid;
  Descriptors[NumberTaskAdd].name = NULL;
  Descriptors[NumberTaskAdd].State = READY;
  Descriptors[NumberTaskAdd].Join = 0;
  Descriptors[NumberTaskAdd].Time = 0;
  Descriptors[NumberTaskAdd].BasePrio = 255;
  Descriptors[NumberTaskAdd].Prio = 255;
  Descriptors[NumberTaskAdd].period = period;
  Descriptors[NumberTaskAdd].execTime = execTime;
  Descriptors[NumberTaskAdd].remainingPeriod = remainingPeriod;
  Descriptors[NumberTaskAdd].remainingExecTime = remainingExecTime;
  Descriptors[NumberTaskAdd].last_start_time = 0;
  Descriptors[NumberTaskAdd].current_run_time = 0;
  Descriptors[NumberTaskAdd].wcet = 0;

  if (Descriptors[NumberTaskAdd].period == 0) {
    Descriptors[NumberTaskAdd].remainingPeriod = 0;
    Descriptors[NumberTaskAdd].remainingExecTime = 0;
  }

  uint8_t *stackTop =
      (uint8_t *)((uintptr_t)(Descriptors[*tid].Stack + SizeTaskStack) & ~0xF);
  Context *ctx = (Context *)(stackTop - sizeof(Context));
  memset(ctx, 0, sizeof(Context));

  ctx->mepc = (uint32_t)taskFunction;
  ctx->mstatus = 0x1880;

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
  case RM:
    // Habilitado para ESP32C3, que possui memoria suficiente
    if (!validateRMSchedulability()) {
      printf("ERROR: System not schedulable with RM!\n");
    }

    for (i = 1; i <= NumberTaskAdd; i++) {
      Descriptors[i].BasePrio =
          (Descriptors[i].period > 0) ? Descriptors[i].period : 255;
      Descriptors[i].Prio = Descriptors[i].BasePrio;
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
  gpio_write(BUILT_IN_LED, 0);
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
  if (semaforo->count >= 0) {
    semaforo->ownerTid = TaskRunning;
    return;
  }

  if (semaforo->ownerTid >= 0) {
    if (Descriptors[TaskRunning].Prio < Descriptors[semaforo->ownerTid].Prio) {
      Descriptors[semaforo->ownerTid].Prio = Descriptors[TaskRunning].Prio;
      sortReadyList();
    }
  }

  if (semaforo->count < 0) {
    semaforo->sem_queue[semaforo->tail] = TaskRunning;
    Descriptors[TaskRunning].State = BLOCKED_SEM;
    semaforo->tail = (semaforo->tail + 1) % MaxNumberTask;
    switchTaskUnsafe();
  }
}

void sys_sempost(sem_t *semaforo) {
  int releasingTid = semaforo->ownerTid;

  semaforo->count++;
  if (semaforo->count <= 0) {
    int nextOwner = semaforo->sem_queue[semaforo->header];
    Descriptors[nextOwner].State = READY;
    InsertReadyList(nextOwner);
    semaforo->header = (semaforo->header + 1) % MaxNumberTask;
    semaforo->ownerTid = nextOwner;
  } else {
    semaforo->ownerTid = -1;
  }

  if (releasingTid >= 0) {
    Descriptors[releasingTid].Prio = Descriptors[releasingTid].BasePrio;
    sortReadyList();
  }
}

void sys_seminit(sem_t *semaforo, int ValorInicial) {
  semaforo->count = ValorInicial;
  semaforo->header = 0;
  semaforo->tail = 0;
  semaforo->ownerTid = -1;
}

void sys_sleep(unsigned int segundo) {
  Descriptors[TaskRunning].Time = segundo;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTaskUnsafe();
  }
}

void sys_msleep(unsigned int mili) {
  Descriptors[TaskRunning].Time = (mili / ClkT) / 1000;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTaskUnsafe();
  }
}

void sys_usleep(unsigned int micro) {
  Descriptors[TaskRunning].Time = (micro / ClkT) / 1000000;
  if (Descriptors[TaskRunning].Time > 0) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTaskUnsafe();
  }
}

void sys_rmssleep(void) {
  if (Descriptors[TaskRunning].State == RUNNING) {
    Descriptors[TaskRunning].State = BLOCKED;
    switchTaskUnsafe();
  }
}

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
    ;

  printTailMutex = true;

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
  printTailMutex = false;
}

NkPrintQueueEntry dequeueNkPrint() {
  while (printHeadMutex == true)
    ;

  printHeadMutex = true;
  NkPrintQueueEntry entry = nkprintQueue[nkprintQueueHead];
  nkprintQueueHead = (nkprintQueueHead + 1) % MAX_NKPRINT_QUEUE;
  printHeadMutex = false;
  return entry;
}

void sys_nkprint(const char *format, void *var) {
  enqueueNkPrint(Descriptors[TaskRunning].Tid, format, var);
}

void print_float(float value, int precision) {
  if (value < 0.0) {
    printf("-");
    value = -value;
  }

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
        while (*temp == '.' || *temp == '*' || (*temp >= '0' && *temp <= '9')) {
          temp++;
        }
        if (*temp == 'f') {
          print_float(*auxfloat, calcularPrecisao(*auxfloat));
          p = temp;
          continue;
        }
      }
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
  switchTaskUnsafe();
}

void sys_nkmalloc(void **ptr, size_t size_in_bytes) {
  *ptr = mem_malloc(size_in_bytes);
}

void sys_nkfree(void *ptr) { mem_free(ptr); }

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
  enqueueNkRead(Descriptors[TaskRunning].Tid, format, var);
  Descriptors[TaskRunning].State = BLOCKED;
  switchTaskUnsafe();
}

float stringToFloat(const char *str) {
  float result = 0.0;
  float factor = 1.0;

  if (*str == '-') {
    str++;
    factor = -1.0;
  }

  for (; *str >= '0' && *str <= '9'; str++) {
    result = result * 10.0 + (*str - '0');
  }

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

  while (*str == ' ' || *str == '\n' || *str == '\r') {
    str++;
  }

  if (*str == '-') {
    if (!isUnsigned) {
      sign = -1;
    }
    str++;
  }

  for (; *str >= '0' && *str <= '9'; str++) {
    result = result * 10 + (*str - '0');
  }

  return result * sign;
}

void serialEvent() {
  while (serial_available_usb()) {
    char c = serial_read_usb();

    if (c == '\n') {
      serialInputBuffer[serialInputIndex] = '\0';
      serialInputIndex = 0;

      if (nkreadQueueHead != nkreadQueueTail) {
        NkReadQueueEntry entry = dequeueNkRead();
        if (strcmp(entry.format, "%f") == 0) {
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
      } else if (serialInputBuffer[0] != '\0') {
        push_aperiodic_function(EVENT_P2_SERIAL);
      }
    } else {
      if (serialInputIndex < 127) {
        serialInputBuffer[serialInputIndex++] = c;
      }
    }
  }
}

/*************************************************************
 * Chamadas de sistema a nivel de usuario                    *
 *************************************************************/

void taskcreate(int *ID, void (*funcao)(), uint8_t period, uint8_t execTime) {
  Parameters arg;
  arg.CallNumber = TASKCREATE;
  arg.p0 = (unsigned char *)ID;
  arg.p1 = (unsigned char *)funcao;
  arg.p3 = (unsigned char *)&period;
  arg.p4 = (unsigned char *)&execTime;
  arg.p5 = (unsigned char *)&period;
  arg.p6 = (unsigned char *)&execTime;
  callsvc(&arg);
}

void taskcreate_polling_server(int *ID, void (*funcao)()) {
  Parameters arg;
  uint8_t period = POLLING_SERVER_PERIOD;
  uint8_t execTime = POLLING_SERVER_EXECTIME;

  arg.CallNumber = TASKCREATE;
  arg.p0 = (unsigned char *)ID;
  arg.p1 = (unsigned char *)funcao;
  arg.p3 = (unsigned char *)&period;
  arg.p4 = (unsigned char *)&execTime;
  arg.p5 = (unsigned char *)&period;
  arg.p6 = (unsigned char *)&execTime;

  callsvc(&arg);
}

void push_aperiodic_function(enum AperiodicEvent event) {
  int next = (aperiodicQueueHead + 1) % APERIODIC_QUEUE_SIZE;
  if (next != aperiodicQueueTail) {
    aperiodicFunctionQueue[aperiodicQueueHead] = event;
    aperiodicQueueHead = next;
  }
}

enum AperiodicEvent pop_aperiodic_function() {
  if (aperiodicQueueHead == aperiodicQueueTail) {
    return EVENT_NONE;
  }
  enum AperiodicEvent event = aperiodicFunctionQueue[aperiodicQueueTail];
  aperiodicQueueTail = (aperiodicQueueTail + 1) % APERIODIC_QUEUE_SIZE;
  return event;
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

void rmssleep(void) {
  Parameters arg;
  arg.CallNumber = RMSSLEEP;
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

void *nkmalloc(size_t num_bytes) {
  void *ptr = NULL;
  Parameters arg;
  arg.CallNumber = NKMALLOC;
  arg.p0 = (unsigned char *)&ptr;
  arg.p1 = (unsigned char *)num_bytes;
  callsvc(&arg);
  return ptr;
}

void nkfree(void *ptr) {
  Parameters arg;
  arg.CallNumber = NKFREE;
  arg.p0 = (unsigned char *)ptr;
  callsvc(&arg);
}
/*************************************************************
 * Programa do usuario - aplicacao                           *
 *************************************************************/

volatile int tid0, tid1, tid2, tid3, tid4;
int i, j;
sem_t sharedResourceMutex;

void bus_manager_task() {
  static int number0;
  static int activationCount = 0;
  setmyname("BUS_MANAGER");
  getmynumber(&number0);
  while (1) {
    activationCount++;
    nkprint("BUS[HIGH] running...\n", 0);

    if (activationCount >= 2) {
      nkprint("BUS[HIGH] tentando lock do bus compartilhado.\n", 0);
      semwait(&sharedResourceMutex);
      nkprint("BUS[HIGH] entrou na secao critica do bus.\n", 0);
      for (volatile long d = 0; d < 350000L; d++)
        ;
      sempost(&sharedResourceMutex);
      nkprint("BUS[HIGH] liberou lock do bus.\n", 0);
    } else {
      nkprint("BUS[HIGH] primeira ativacao sem contencao.\n", 0);
      for (volatile long d = 0; d < 350000L; d++)
        ;
    }

    nkprint("BUS[HIGH] finished.\n", 0);
    rmssleep();
  }
}

void communications_task() {
  static int number1;
  setmyname("COMMS");
  getmynumber(&number1);
  while (1) {
    nkprint("COMMS[MED] running...\n", 0);

    nkprint("COMMS[MED] Alocando memoria para sensor...\n", 0);
    DadosSensor *ds = (DadosSensor *)nkmalloc(sizeof(DadosSensor));
    if (ds != NULL) {
      ds->sensor_id = 42;
      ds->temperatura = 36.5f;
      nkprint("COMMS[MED] Sensor alocado! ID: %d\n", &(ds->sensor_id));

      for (volatile long d = 0; d < 300000L; d++)
        ;

      nkprint("COMMS[MED] Liberando memoria...\n", 0);
      nkfree(ds);
    } else {
      nkprint("COMMS[MED] Erro ao alocar memoria!\n", 0);
    }

    for (volatile long d = 0; d < 600000L; d++)
      ;
    nkprint("COMMS[MED] finished.\n", 0);
    rmssleep();
  }
}

void meteorological_task() {
  int currentPriority;
  nkprint("METEO[LOW] entrou na secao critica longa.\n", 0);
  semwait(&sharedResourceMutex);

  volatile float i = 1.0;
  for (volatile long d = 0; d < 3600000L; d++) {
    i += 0.001;
  }
  nkprint("%f\n", &i);

  currentPriority = Descriptors[TaskRunning].Prio;
  nkprint("METEO[LOW] prioridade efetiva (com PI): %d\n", &currentPriority);

  sempost(&sharedResourceMutex);
  nkprint("METEO[LOW] liberou lock do bus.\n", 0);
}

void polling_function_selector() {
  setmyname("POLLING_SERVER");
  while (1) {
    enum AperiodicEvent event = pop_aperiodic_function();

    if (event != EVENT_NONE) {
      switch (event) {
      case EVENT_P2_SERIAL:
        meteorological_task();
        break;
      }
    } else {
      rmssleep();
    }
  }
}
// GERENCIA DE MEMÓRIA, FIRST FIT
// Configurações do Heap
#define HEAP_SIZE 256 // Tamanho total da RAM simulada para este teste

// Cabeçalho que ficará invisível antes de cada bloco alocado
typedef struct BlockHeader {
  size_t size;
  bool is_free;
  struct BlockHeader *next;
} BlockHeader;

uint8_t heap[HEAP_SIZE];
BlockHeader *head = NULL;

// 1. Inicialização do Heap
void mem_init() {
  // Transforma o array inteiro em um único bloco gigante e livre
  head = (BlockHeader *)heap;
  head->size = HEAP_SIZE - sizeof(BlockHeader);
  head->is_free = true;
  head->next = NULL;
}
// 2. Alocação (First-Fit)
void *mem_malloc(size_t size) {
  if (size == 0)
    return NULL;

  BlockHeader *curr = head;

  // Procura o primeiro bloco livre que seja grande o suficiente
  while (curr != NULL) {
    if (curr->is_free && curr->size >= size) {

      // Se o bloco for muito maior do que o pedido, fatia ele em dois
      if (curr->size >= size + sizeof(BlockHeader) + 1) {
        BlockHeader *novo_bloco =
            (BlockHeader *)((uint8_t *)curr + sizeof(BlockHeader) + size);
        novo_bloco->size = curr->size - size - sizeof(BlockHeader);
        novo_bloco->is_free = true;
        novo_bloco->next = curr->next;

        curr->next = novo_bloco;
        curr->size = size;
      }

      curr->is_free = false;
      // Retorna o ponteiro para os dados (ignorando o cabeçalho)
      return (void *)((uint8_t *)curr + sizeof(BlockHeader));
    }
    curr = curr->next;
  }

  return NULL; // Falta de memória (Out of Memory)
}
// 3. Liberação com Coalescência
void mem_free(void *ptr) {
  if (ptr == NULL)
    return;

  // Recua o ponteiro para acessar o cabeçalho oculto
  BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - sizeof(BlockHeader));
  block->is_free = true;

  // Coalescência: Junta blocos livres vizinhos para curar a fragmentação
  BlockHeader *curr = head;
  while (curr != NULL && curr->next != NULL) {
    if (curr->is_free && curr->next->is_free) {
      curr->size += curr->next->size + sizeof(BlockHeader);
      curr->next = curr->next->next;
    } else {
      curr = curr->next;
    }
  }
}
// 4. Depuração do Mapa de Memória
void print_mem_status() {
  printf("\n--- Mapa do Heap (Tamanho Total: %d bytes) ---\n", HEAP_SIZE);
  BlockHeader *curr = head;
  int index = 0;

  while (curr != NULL) {
    printf("Bloco %d: [%-7s] | Tamanho: %3zu bytes | Endereco: %p\n", index++,
           curr->is_free ? "LIVRE" : "ALOCADO", curr->size, (void *)curr);
    curr = curr->next;
  }
  printf("------------------------------------------------\n");
}

/*************************************************************
 * Funcoes de apoio para o escalonamento rate-monotonic      *
 *************************************************************/

bool validateRMSchedulability() {
  float totalUtilization = 0.0f;
  int taskCount = 0;

  for (int i = 1; i <= NumberTaskAdd; i++) {
    if (Descriptors[i].period == 0) {
      continue;
    }

    float utilization =
        (float)Descriptors[i].execTime / (float)Descriptors[i].period;
    totalUtilization += utilization;
    taskCount++;

    // printf("Task %d: Ci=%d, Ti=%d", i, Descriptors[i].execTime,
    // Descriptors[i].period);
    nkprint("Task %d", &i);
    nkprint(", Ci=%d", &Descriptors[i].execTime);
    nkprint(", Ti=%d", &Descriptors[i].period);
    nkprint(", util=%f\n", &utilization);
  }

  double liu_layland = NUM_TASKS * (pow(2.0, (1 / (double)NUM_TASKS)) - 1.0);
  nkprint("Total utilization: %f\n", &totalUtilization);
  nkprint("RM bound: %f\n", &liu_layland);
  // printf("Total utilization: %f\n", totalUtilization);
  // printf("RM bound: %f\n", &liu_layland);

  if (totalUtilization <= liu_layland) {
    printf("RESULT: SCHEDULABLE (viable)\n");
    return true;
  } else {
    printf("RESULT: NOT SCHEDULABLE (may miss deadlines)\n");
    return false;
  }
}

float getTaskUtilization(int taskId) {
  if (taskId < 0 || taskId > NumberTaskAdd || Descriptors[taskId].period == 0) {
    return 0.0f;
  }
  return (float)Descriptors[taskId].execTime /
         (float)Descriptors[taskId].period;
}

float getTotalSystemUtilization() {
  float total = 0.0f;
  for (int i = 1; i <= NumberTaskAdd; i++) {
    if (Descriptors[i].period > 0) {
      total += getTaskUtilization(i);
    }
  }
  return total;
}

/*************************************************************
 * Setup e criacao de tasks                                  *
 *************************************************************/

int main(void) {
  wdt_disable();
  // Desbloqueia e desabilita o Watchdog do Timer Group 0 (TG0WDT)
  //*(volatile uint32_t *)0x6001f064 = 0x50D83AA1; // Escreve a senha no
  // TIMG_WDTWPROTECT_REG
  //*(volatile uint32_t *)0x6001f048 = 0;          // Desliga o EN no
  // TIMG_WDTCONFIG0_REG
  __asm__ volatile("csrw mtvec, %0" ::"r"((uintptr_t)vector_table | 1));

  nkprint("FakeOS RM Polling Server\n", 0);
  nkprint("Arquitetura ESP32-C3 RISC-V\n", 0);

  seminit(&sharedResourceMutex, 1);
  mem_init();

  taskcreate(&tid0, idle, 0, 0);
  taskcreate(&tid1, bus_manager_task, 5, 1);
  taskcreate(&tid2, communications_task, 9, 2);
  taskcreate_polling_server(&tid3, polling_function_selector);
  // taskcreate(&tid4, meteorological_task, 20, 1);

  printf("[DEBUG] main: calling start(RM)\n");
  start(RM);
  printf("[DEBUG] main: after start(RM), calling systimer_init()\n");

  systimer_init();
  printf("[DEBUG] main: after systimer_init(), calling "
         "get_systimer_baremetal()\n");

  Descriptors[0].last_start_time = get_systimer_baremetal();
  printf("[DEBUG] main: after get_systimer_baremetal(), calling startOS()\n");

  startOS(Descriptors[0].P);

  while (1)
    ;
}

// SISTEMA NKEARDUINO
// RM, APERIODICA, GERENCIA DE MEMORIA, HERANÇA DE PRIORIDADE, TEMPOS, WCET

// WCET -