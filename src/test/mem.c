#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
// Configurações do Pool
#define POOL_BLOCK_SIZE 32  // Tamanho de cada bloco (bytes)
#define POOL_NUM_BLOCKS 16  // Quantidade total de blocos

// Estrutura do Bloco (Usa Union para economizar RAM)
typedef union PoolBlock {
    uint8_t data[POOL_BLOCK_SIZE]; // Usado quando alocado para a tarefa
    union PoolBlock* next;         // Usado quando está livre na fila
} PoolBlock;

// Variáveis Globais do Kernel
PoolBlock memory_pool[POOL_NUM_BLOCKS];
PoolBlock* free_list_head = NULL;

// 1. Inicialização (Chamar no main antes do startOS)
void mempool_init() {
    for (int i = 0; i < POOL_NUM_BLOCKS - 1; i++) {
        memory_pool[i].next = &memory_pool[i + 1];
    }
    memory_pool[POOL_NUM_BLOCKS - 1].next = NULL;
    free_list_head = &memory_pool[0];
}

// 2. Alocação (O(1))
void* mempool_alloc() {
    if (free_list_head == NULL) {
        return NULL; // Erro: Out of Memory
    }
    
    // Retira o primeiro bloco livre da lista
    PoolBlock* block = free_list_head;
    free_list_head = free_list_head->next;
    
    return (void*)block;
}

// 3. Liberação (O(1))
void mempool_free(void* ptr) {
    if (ptr == NULL) return;
    
    PoolBlock* block = (PoolBlock*)ptr;
    
    // Devolve o bloco para o topo da lista de livres
    block->next = free_list_head;
    free_list_head = block;
}

// 1. Defina a estrutura de dados que a tarefa precisa usar
// Atenção: O tamanho desta struct NÃO PODE ser maior que POOL_BLOCK_SIZE (ex: 32 bytes)
typedef struct {
    int sensor_id;
    float temperature;
    char status;
} SensorData;

void tarefa_produtora(int new_sensor_id, float new_temperature, char new_status) {
    // 2. Solicita um bloco livre ao SO
    void* ptr = mempool_alloc();
    
    if (ptr != NULL) {
        // 3. Faz o cast do ponteiro genérico para o tipo da sua estrutura
        SensorData* meu_dado = (SensorData*)ptr;
        
        // 4. Acessa e altera os dados livremente
        meu_dado->sensor_id = new_sensor_id; // 10
        meu_dado->temperature = new_temperature; // 24.5f
        meu_dado->status = new_status;   // ex. 'A'
        
        // 5. Opcional: Aqui você enviaria 'meu_dado' para outra tarefa 
        // usando uma fila (Message Queue).
       mempool_free(ptr); 
        // Se não for enviar para ninguém, deve liberar a memória
        // mempool_free(meu_dado);
    }
}
void print_mempool_status() {
    bool is_free[POOL_NUM_BLOCKS];
    
    for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
        is_free[i] = false;
    }

    PoolBlock* current = free_list_head;
    int free_count = 0;
    
    while (current != NULL) {
        int index = current - memory_pool;
        if (index >= 0 && index < POOL_NUM_BLOCKS) {
            is_free[index] = true;
            free_count++;
        }
        current = current->next;
    }

    int alloc_count = POOL_NUM_BLOCKS - free_count;
    
    printf("\n--- Status do Memory Pool ---\n");
    printf("Blocos Livres: %d | Alocados: %d\n", free_count, alloc_count);

    for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (is_free[i]) {
            printf("Bloco %d: LIVRE\n", i);
        } else {
            printf("Bloco %d: ALOCADO\n", i);
        }
    }
    printf("-----------------------------\n");
}
int main (){
    PoolBlock* bloco1 = NULL; 
    PoolBlock* bloco2 = NULL;
    PoolBlock* bloco3 = NULL;
    PoolBlock* bloco4 = NULL;
    PoolBlock* bloco5 = NULL;

    
    print_mempool_status();
    mempool_init();
    print_mempool_status();
    
    bloco1 = mempool_alloc();
    print_mempool_status();
    
    bloco2 = mempool_alloc();
    print_mempool_status();
    
    bloco3 = mempool_alloc();
    print_mempool_status();
    
    mempool_free(bloco2);    
    print_mempool_status();

    mempool_free(bloco1); 
    print_mempool_status();

    mempool_alloc(bloco4);
    print_mempool_status();

    mempool_alloc(bloco5);
    print_mempool_status();
    /*    
    print_mempool_status();
    tarefa_produtora(3, 114.4, 'A');
    print_mempool_status();
    tarefa_produtora(4, 113.1, 'A');
    print_mempool_status();
    tarefa_produtora(6, 112.3, 'A');
    print_mempool_status();
    tarefa_produtora(1, 116.0, 'A');
    print_mempool_status();
    tarefa_produtora(3, 113.2, 'A');
    tarefa_produtora(6, 109.6, 'A');
    tarefa_produtora(3, 122.5, 'A');
    tarefa_produtora(4, 140.3, 'A');
    tarefa_produtora(10, 141.2, 'A');
    //mempool_free(bloco1);
    */
    }