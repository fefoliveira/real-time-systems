#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Configurações do Heap
#define HEAP_SIZE 1024  // Tamanho total da RAM simulada para este teste

// Cabeçalho que ficará invisível antes de cada bloco alocado
typedef struct BlockHeader {
    size_t size;
    bool is_free;
    struct BlockHeader* next;
} BlockHeader;

uint8_t heap[HEAP_SIZE];
BlockHeader* head = NULL;

// Estrutura de exemplo para simular os dados de uma tarefa
typedef struct {
    int sensor_id;
    float temperatura;
} DadosSensor;

// 1. Inicialização do Heap
void mem_init() {
    // Transforma o array inteiro em um único bloco gigante e livre
    head = (BlockHeader*)heap;
    head->size = HEAP_SIZE - sizeof(BlockHeader);
    head->is_free = true;
    head->next = NULL;
}

// 2. Alocação (First-Fit)
void* mem_malloc(size_t size) {
    if (size == 0) return NULL;

    BlockHeader* curr = head;

    // Procura o primeiro bloco livre que seja grande o suficiente
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            
            // Se o bloco for muito maior do que o pedido, fatia ele em dois
            if (curr->size >= size + sizeof(BlockHeader) + 1) {
                BlockHeader* novo_bloco = (BlockHeader*)((uint8_t*)curr + sizeof(BlockHeader) + size);
                novo_bloco->size = curr->size - size - sizeof(BlockHeader);
                novo_bloco->is_free = true;
                novo_bloco->next = curr->next;
                
                curr->next = novo_bloco;
                curr->size = size;
            }
            
            curr->is_free = false;
            // Retorna o ponteiro para os dados (ignorando o cabeçalho)
            return (void*)((uint8_t*)curr + sizeof(BlockHeader));
        }
        curr = curr->next;
    }
    
    return NULL; // Falta de memória (Out of Memory)
}

// 3. Liberação com Coalescência
void mem_free(void* ptr) {
    if (ptr == NULL) return;
    
    // Recua o ponteiro para acessar o cabeçalho oculto
    BlockHeader* block = (BlockHeader*)((uint8_t*)ptr - sizeof(BlockHeader));
    block->is_free = true;

    // Coalescência: Junta blocos livres vizinhos para curar a fragmentação
    BlockHeader* curr = head;
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
    BlockHeader* curr = head;
    int index = 0;
    
    while (curr != NULL) {
        printf("Bloco %d: [%-7s] | Tamanho: %3zu bytes | Endereco: %p\n", 
                index++, 
                curr->is_free ? "LIVRE" : "ALOCADO", 
                curr->size, 
                (void*)curr);
        curr = curr->next;
    }
    printf("------------------------------------------------\n");
}

int main() {
    printf("Inicializando Heap...\n");
    mem_init();
    print_mem_status();

    printf("\nPedindo %zu bytes para Sensor 1...\n", sizeof(DadosSensor));
    DadosSensor* s1 = (DadosSensor*)mem_malloc(sizeof(DadosSensor));
    print_mem_status();

    printf("\nPedindo 50 bytes genéricos...\n");
    void* buffer = mem_malloc(50);
    print_mem_status();

    if (s1 != NULL) {
        s1->sensor_id = 100;
        s1->temperatura = 25.4f;
        printf("\nSensor 1 gravado: ID=%d, Temp=%.1f\n", s1->sensor_id, s1->temperatura);
    }

    printf("\nLiberando Sensor 1 (Notar a união de blocos na proxima alocacao)...\n");
    mem_free(s1);
    print_mem_status();

    printf("\nLiberando Buffer de 50 bytes (Coalescencia curando o Heap)...\n");
    mem_free(buffer);
    print_mem_status();

    return 0;
}