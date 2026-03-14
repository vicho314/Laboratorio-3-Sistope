/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "segmentacion.h"


// Entradas: ninguna
// Salidas: nada
// Descripcion: En segmentacion, los segmentos estan siempre en memoria.
//              La tabla de segmentos es una simple asignacion de direcciones base
//              cada segmento tiene una base (inicio) y limite (tamaño)
//              Este modelo es mucho mas simple que paginacion porque no necesita
//              frame allocator ni manejo de evictaciones.

// Entradas: config (configuracion global), thread_id (ID del thread),
//           fa (no usado en segmentacion, presente por consistencia de interfaz),
//           all_segment_tables (arreglo con punteros a las tablas de todos los threads)
// Salidas: puntero a segmentation_context_t creado o NULL si falla
// Descripcion: Crea la tabla de segmentos para este thread.
//               En segmentacion no hay frames ni TLB, solo un mapeo base->limite
//               para cada segmento. Los segmentos se asignan secuencialmente en memoria.
segmentation_context_t *segmentation_context_create(sim_config_t *config, int thread_id,
                                                    frame_allocator_t *fa,
                                                    segment_table_t **all_segment_tables)
{
    segmentation_context_t *ctx = malloc(sizeof(segmentation_context_t));
    if (!ctx) {
        fprintf(stderr, "Error: no se pudo crear contexto de segmentacion\n");
        return NULL;
    }

    /* Vincula el hilo con el asignador global y las tablas de todo el sistema 
   para permitir la invalidacion cruzada tras una expulsion de segmento (eviction). */
    ctx->thread_id = thread_id;
    ctx->fa = fa;
    ctx->all_segment_tables = all_segment_tables;
    ctx->num_threads = config->threads;

    // Crear tabla de segmentos
    ctx->segment_table = malloc(sizeof(segment_table_t));
    if (!ctx->segment_table) {
        fprintf(stderr, "Error: no se pudo crear tabla de segmentos\n");
        free(ctx);
        return NULL;
    }

    /* Define el numero de segmentos y reserva memoria limpia para las entradas de la tabla. 
    Si la asignacion falla, libera los recursos previos para evitar fugas de memoria. */
    ctx->segment_table->num_segments = config->num_segments;
    ctx->segment_table->entries = calloc(config->num_segments, sizeof(segment_table_entry_t));
    if (!ctx->segment_table->entries) {
        fprintf(stderr, "Error: no se pudo asignar entradas de tabla de segmentos\n");
        free(ctx->segment_table);
        free(ctx);
        return NULL;
    }

    // Inicializar todos los segmentos con sus bases y limites
    // En segmentacion, los segmentos se asignan secuencialmente en memoria virtual
    uint64_t current_base = 0;
    for (int i = 0; i < config->num_segments; i++) {
        ctx->segment_table->entries[i].base = current_base;
        ctx->segment_table->entries[i].limit = config->segment_limits[i];
        ctx->segment_table->entries[i].valid = 1;  // Siempre valido en segmentacion
        current_base += config->segment_limits[i];  // Siguiente base
    }

    return ctx;
}

// Entradas: ctx (contexto de segmentacion del thread),
//           virtual_addr (direccion virtual generada por el workload),
//           config (configuracion global),
//           metrics (metricas del thread)
// Salidas: direccion fisica (PA) o INVALID_FRAME si falla
// Descripcion: Traduce una direccion virtual a fisica usando segmentacion.
//              VA = numero_segmento + offset. Los segmentos siempre estan
//              en memoria (valido = 1). Se valida que offset < limite.
uint64_t segmentation_translate(segmentation_context_t *ctx, uint64_t virtual_addr,
                                sim_config_t *config, thread_metrics_t *metrics)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Extraer numero de segmento y offset
    uint64_t page_size = (uint64_t)config->page_size;
    uint64_t segment_id = virtual_addr / page_size;
    uint64_t offset = virtual_addr % page_size;

    // Validar que segment_id este en rango
    if (segment_id >= (uint64_t)config->num_segments) {
        fprintf(stderr, "Error: segment_id %lu fuera de rango (max %d)\n",
                segment_id, config->num_segments - 1);
        metrics->page_faults++;
        return INVALID_FRAME;
    }

    // En segmentacion los segmentos siempre estan en memoria
    segment_table_entry_t *seg_entry = &ctx->segment_table->entries[segment_id];

    // Validar que offset este dentro del limite del segmento
    if (offset >= seg_entry->limit) {
        // Segmentation fault: violacion de limite
        metrics->page_faults++;
        fprintf(stderr, "Error: offset %lu >= limite %lu del segmento %lu\n",
                offset, seg_entry->limit, segment_id);
        return INVALID_FRAME;
    }

    // Calcular direccion fisica: PA = base + offset
    uint64_t physical_addr = seg_entry->base + offset;

    // Medir tiempo
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    metrics->total_translation_time_ns += (uint64_t)elapsed_ns;
    metrics->total_ops++;

    return physical_addr;
}

// Entradas: ctx (contexto de segmentacion a destruir)
// Salidas: ninguna
// Descripcion: Libera la tabla de segmentos y la estructura del contexto.
void segmentation_context_destroy(segmentation_context_t *ctx)
{
    if (!ctx) return;

    // Liberar tabla de segmentos
    if (ctx->segment_table) {
        if (ctx->segment_table->entries) {
            free(ctx->segment_table->entries);
        }
        free(ctx->segment_table);
    }

    // NO libera ctx->fa es compartido
    // NO libera ctx->all_segment_tables es arreglo compartido

    free(ctx);
}
