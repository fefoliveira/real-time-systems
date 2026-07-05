# Medição de Tempo de Execução das Tarefas (WCET / Profiling) no fakeOS

Este documento explica como foi implementada a medição do tempo de CPU de cada tarefa do kernel `fakeOS` no ESP32-C3, utilizando o timer de hardware (`systimer_baremetal`).

---

## 1. O que foi alterado

### A. Estrutura das Tarefas (`TaskDescriptor`)
Foram adicionados dois campos à estrutura de controle das tarefas em `src/nke_polling_sv.c`:
- `last_start_time`: Guarda o timestamp (ticks) de quando a tarefa começou a rodar na CPU mais recentemente.
- `current_run_time`: Acumula o tempo de execução líquido (ticks) da tarefa na ativação atual, descontando o tempo em que ela esteve preemptada por tarefas de maior prioridade.

### B. Algoritmo de Escalonamento (`switchTaskUnsafe`)
O escalonador agora realiza as seguintes ações a cada troca de contexto:
1. **Medição da tarefa que está saindo:**
   - Calcula o tempo decorrido desde a última vez em que ela entrou em execução (`now - last_start_time`).
   - Acumula essa diferença em `current_run_time`.
2. **Exibição do tempo de ativação:**
   - Se a tarefa que está saindo está prestes a bloquear (estado `BLOCKED` ou `BLOCKED_SEM`), significa que ela completou sua ativação ou foi suspensa.
   - O kernel imprime no console o tempo líquido total que a tarefa passou executando na CPU naquela ativação (em ticks e em microssegundos: `ticks / 16`).
   - O acumulador `current_run_time` é zerado para a próxima ativação.
3. **Carregamento da tarefa que está entrando:**
   - Define a nova `last_start_time` como o tempo atual (`get_systimer_baremetal()`).

### C. Nomes de Tarefas
Para tornar os relatórios no console legíveis, adicionamos chamadas para definir os nomes das tarefas em seus respectivos pontos de entrada (`setmyname`):
- `IDLE` (Tarefa ociosa, Tid 0 - seu tempo de execução não é impresso para não poluir o terminal).
- `BUS_MANAGER` (Tarefa de alta prioridade, Tid 1).
- `COMMS` (Tarefa de prioridade média, Tid 2).
- `POLLING_SERVER` (Servidor de polling aperiódico, Tid 3).

---

## 2. Como Compilar, Gravar e Testar

Entre na pasta do código-fonte e execute o comando Makefile para compilar, gravar no ESP32-C3 e abrir o monitor serial:

```bash
cd src
make build flash monitor
```

---

## 3. Exemplo de Saída Esperada no Monitor Serial

Durante a execução normal e após disparar tarefas periódicas ou aperiódicas (pressionando `Enter` no terminal para ativar `METEO` dentro do `POLLING_SERVER`), você verá logs no seguinte formato:

```text
[TASK TIME] Task 1 (BUS_MANAGER) finished activation. Executed for 311000 ticks (19437 us)
[TASK TIME] Task 2 (COMMS) finished activation. Executed for 285432 ticks (17839 us)
[TASK TIME] Task 3 (POLLING_SERVER) finished activation. Executed for 15632 ticks (977 us)
```

Isso permite identificar com precisão o tempo de execução de pior caso (WCET) de cada tarefa no processador ESP32-C3 físico de forma direta e sem overhead de software de monitoramento externo.
