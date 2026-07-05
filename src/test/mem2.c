#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Configurações do Pool
#define POOL_BLOCK_SIZE 32
#define POOL_NUM_BLOCKS 5  // Reduzido para 5 para facilitar a visualização no terminal

typedef union PoolBlock {
    uint8_t data[POOL_BLOCK_SIZE];
    union PoolBlock* next;
} PoolBlock;

PoolBlock memory_pool[POOL_NUM_BLOCKS];
PoolBlock* free_list_head = NULL;

// Estrutura de exemplo para simular os dados de uma tarefa
typedef struct {
    int sensor_id;
    float temperatura;
} DadosSensor;

// 1. Inicialização
void mempool_init() {
    for (int i = 0; i < POOL_NUM_BLOCKS - 1; i++) {
        memory_pool[i].next = &memory_pool[i + 1];
    }
    memory_pool[POOL_NUM_BLOCKS - 1].next = NULL;
    free_list_head = &memory_pool[0];
}

// 2. Alocação
void* mempool_alloc() {
    if (free_list_head == NULL) return NULL;
    
    PoolBlock* block = free_list_head;
    free_list_head = free_list_head->next;
    return (void*)block;
}

// 3. Liberação
void mempool_free(void* ptr) {
    if (ptr == NULL) return;
    
    PoolBlock* block = (PoolBlock*)ptr;
    block->next = free_list_head;
    free_list_head = block;
}

// 4. Status (agora usando printf padrão do C)
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

int main() {
    printf("Inicializando o sistema...\n");
    mempool_init();
    print_mempool_status();

    printf("\nAlocando 2 blocos para sensores...\n");
    DadosSensor* s1 = (DadosSensor*)mempool_alloc();
    DadosSensor* s2 = (DadosSensor*)mempool_alloc();

    if (s1 != NULL && s2 != NULL) {
        s1->sensor_id = 100;
        s1->temperatura = 25.4f;
        
        s2->sensor_id = 101;
        s2->temperatura = 28.1f;
        
        printf("Sensor 1 gravado: ID=%d, Temp=%.1f\n", s1->sensor_id, s1->temperatura);
        printf("Sensor 2 gravado: ID=%d, Temp=%.1f\n", s2->sensor_id, s2->temperatura);
    }

    print_mempool_status();

    printf("\nLiberando o bloco do Sensor 1...\n");
    mempool_free(s1);
    
    print_mempool_status();

    return 0;
}