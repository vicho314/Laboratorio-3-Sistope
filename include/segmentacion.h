/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#ifndef SEGMENTACION_H
#define SEGMENTACION_H

#include "simulator.h"
#include "tlb.h"
#include "frame_allocator.h"


typedef struct {
    uint64_t base;              // Dirección base del segmento
    uint64_t limit;             // Límite (tamaño) del segmento
    int valid;                  // 1 = segmento válido en memoria, 0 = no está en memoria
} segment_table_entry_t;


typedef struct {
    segment_table_entry_t *entries;  // Arreglo indexado por número de segmento
    int num_segments;                // Número de segmentos virtuales
} segment_table_t;


typedef struct {
    segment_table_t *segment_table;       // Tabla de segmentos de este thread
    frame_allocator_t *fa;               // Frame allocator global (compartido)
    int thread_id;                       // ID de este thread

    segment_table_t **all_segment_tables; // Arreglo de punteros a las tablas de todos los threads
    int num_threads;                     // Número total de threads
} segmentation_context_t;


segmentation_context_t *segmentation_context_create(sim_config_t *config, int thread_id,
                                                    frame_allocator_t *fa,
                                                    segment_table_t **all_segment_tables);


uint64_t segmentation_translate(segmentation_context_t *ctx, uint64_t virtual_addr,
                                sim_config_t *config, thread_metrics_t *metrics);


void segmentation_context_destroy(segmentation_context_t *ctx);

#endif // SEGMENTACION_H
