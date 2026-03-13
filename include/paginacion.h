/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */

#ifndef PAGINACION_H
#define PAGINACION_H

#include "simulator.h"
#include "tlb.h"
#include "frame_allocator.h"


typedef struct {
    uint64_t frame_number;  // Frame fisico asignado (o INVALID_FRAME si no esta en memoria)
    int valid;              // 1 = pagina en memoria, 0 = no esta en memoria
} page_table_entry_t;


typedef struct {
    page_table_entry_t *entries;  // Arreglo indexado por VPN
    int num_pages;                // Numero de paginas virtuales
} page_table_t;


typedef struct {
    page_table_t *page_table;       // Tabla de paginas de este thread
    tlb_t *tlb;                     // TLB de este thread
    frame_allocator_t *fa;          // Frame allocator global (compartido)
    int thread_id;                  // ID de este thread

   
    tlb_t **all_tlbs;               // Arreglo de punteros a las TLBs de todos los threads
    page_table_t **all_page_tables; // Arreglo de punteros a las tablas de todos los threads
    int num_threads;                // Numero total de threads
} paging_context_t;


paging_context_t *paging_context_create(sim_config_t *config, int thread_id,
                                        frame_allocator_t *fa,
                                        tlb_t **all_tlbs,
                                        page_table_t **all_page_tables);


uint64_t paging_translate(paging_context_t *ctx, uint64_t virtual_addr,
                          sim_config_t *config, thread_metrics_t *metrics);


void paging_context_destroy(paging_context_t *ctx);

#endif 
