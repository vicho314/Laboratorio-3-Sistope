/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "segmentacion.h"


// Entradas: ninguna
// Salidas: ninguna
// Descripcion: Simula el delay de cargar un segmento desde disco. En un sistema real esto tomaria milisegundos.
//              Usamos nanosleep con un delay aleatorio entre 1 y 5 milisegundos
static void simulate_disk_load(void)
{
    // Generar delay aleatorio entre 1ms y 5ms
    // rand() % 5 da 0-4, le sumamos 1 para obtener 1-5
    long delay_ms = (rand() % 5) + 1;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = delay_ms * 1000000L;  // Convertir milisegundos a nanosegundos

    nanosleep(&ts, NULL);
}

// Entradas: ctx (contexto de segmentacion), victim_segment (segmento evictado),
//           victim_thread_id (thread dueno del segmento evictado)
// Salidas: ninguna
// Descripcion: Cuando se evicta un segmento, hay que invalidarlo en la tabla de
//              segmentos del thread dueno.
static void invalidate_evicted_segment(segmentation_context_t *ctx,
                                       uint64_t victim_segment,
                                       int victim_thread_id)
{
    // Invalidar en la tabla de segmentos del thread dueno
    if (victim_thread_id >= 0 && victim_thread_id < ctx->num_threads) {
        segment_table_t *victim_st = ctx->all_segment_tables[victim_thread_id];
        if (victim_st && victim_segment < (uint64_t)victim_st->num_segments) {
            victim_st->entries[victim_segment].valid = 0;
            victim_st->entries[victim_segment].base = 0;
            victim_st->entries[victim_segment].limit = 0;
        }
    }
}

// Entradas: ctx (contexto), segment_id (segmento que necesita frame), config (configuracion),
//           metrics (metricas locales del thread)
// Salidas: numero de frame asignado o INVALID_FRAME si falla
// Descripcion: Maneja un segmento fault completo:
//               Intenta obtener un frame libre
//              Si no hay libres, evicta el segmento mas antiguo (FIFO)
//               Simula la carga desde disco (nanosleep)
//               Actualiza la tabla de segmentos y la TLB
static uint64_t handle_segment_fault(segmentation_context_t *ctx, uint64_t segment_id,
                                     sim_config_t *config, thread_metrics_t *metrics)
{
    // config se recibe por consistencia de interfaz 
    (void)config;

    // Intentar obtener un frame libre
    uint64_t frame = frame_alloc_get(ctx->fa, ctx->thread_id, segment_id);

    // Si no hay frames libres, necesitamos evictar
    if (frame == INVALID_FRAME) {
        // Seleccionar victima con FIFO
        uint64_t victim_frame = frame_alloc_evict(ctx->fa);

        if (victim_frame == INVALID_FRAME) {
            // No deberia pasar si no hay libres, debe haber ocupados para evictar
            fprintf(stderr, "Error: no se pudo evictar ningun frame\n");
            return INVALID_FRAME;
        }

        // Obtener info de la victima 
        frame_info_t *victim_info = frame_alloc_get_info(ctx->fa, victim_frame);

        if (victim_info) {
            // Invalidar el segmento evictado en tabla de segmentos
            invalidate_evicted_segment(ctx, victim_info->vpn, victim_info->thread_id);
        }

        // Ahora el frame esta libre, asignarlo a nuestro segmento
        frame = frame_alloc_get(ctx->fa, ctx->thread_id, segment_id);

        if (frame == INVALID_FRAME) {
            fprintf(stderr, "Error: frame no disponible despues de eviction\n");
            return INVALID_FRAME;
        }
    }

    // Simular la carga del segmento desde disco
    simulate_disk_load();

    // Actualizar la tabla de segmentos de este thread
    // Asignamos base como frame * page_size (simulación simplificada)
    ctx->segment_table->entries[segment_id].base = frame * config->page_size;
    ctx->segment_table->entries[segment_id].limit = config->page_size;  // Tamaño del segmento
    ctx->segment_table->entries[segment_id].valid = 1;

    return frame;
}

// Entradas: config (configuracion global), thread_id (ID del thread),
//           fa (frame allocator compartido),
//           all_tlbs (arreglo con punteros a las TLBs de todos los threads),
//           all_segment_tables (arreglo con punteros a las tablas de todos los threads)
// Salidas: puntero a segmentation_context_t creado o NULL si falla
// Descripcion: Crea todo lo que un thread necesita para el modo segmentacion:
//               Una tabla de segmentos con num_segments entradas, todas invalidas
//               Una TLB de tamano tlb_size (0 = deshabilitada)
//               Referencias al frame allocator y a las estructuras de otros threads
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
    ctx->segment_table->num_segments = config->num_pages;
    ctx->segment_table->entries = calloc(config->num_pages, sizeof(segment_table_entry_t));
    if (!ctx->segment_table->entries) {
        fprintf(stderr, "Error: no se pudo asignar entradas de tabla de segmentos\n");
        free(ctx->segment_table);
        free(ctx);
        return NULL;
    }

    // Inicializar todos los segmentos como invalidos
    for (int i = 0; i < config->num_pages; i++) {
        ctx->segment_table->entries[i].base = 0;
        ctx->segment_table->entries[i].limit = 0;
        ctx->segment_table->entries[i].valid = 0;
    }

    return ctx;
}

// Entradas: ctx (contexto de segmentacion del thread),
//           virtual_addr (direccion virtual generada por el workload),
//           config (configuracion global),
//           metrics (metricas locales del thread para registrar hits/misses/faults)
// Salidas: direccion fisica (PA) o INVALID_FRAME si falla
// Descripcion: Ejecuta el flujo completo de traduccion de segmentacion.
//              Extrae el numero de segmento y offset de la direccion virtual,
//              consulta la tabla de segmentos, y calcula la direccion fisica.
//              Mide el tiempo de traduccion con clock_gettime para las metricas.
uint64_t segmentation_translate(segmentation_context_t *ctx, uint64_t virtual_addr,
                                sim_config_t *config, thread_metrics_t *metrics)
{
    // Medir tiempo de inicio 
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // En segmentacion, el numero de segmento puede extraerse de los bits mas altos
    // Para simplificar, usamos division por page_size similar a paginacion
    uint64_t page_size = (uint64_t)config->page_size;
    uint64_t segment_id = virtual_addr / page_size;
    uint64_t offset = virtual_addr % page_size;

    // Validar que segment_id este en rango
    if (segment_id >= (uint64_t)config->num_pages) {
        fprintf(stderr, "Error: segment_id %lu fuera de rango (max %d)\n",
                segment_id, config->num_pages - 1);
        return INVALID_FRAME;
    }

    // Consultar tabla de segmentos 
    if (ctx->segment_table->entries[segment_id].valid) {
        // Tabla de segmentos HIT: el segmento esta en memoria

        // Validar que el offset este dentro del limite del segmento
        if (offset >= ctx->segment_table->entries[segment_id].limit) {
            // Contar como segfault (violación de límite)
            metrics->page_faults++;
            fprintf(stderr, "Error: offset %lu fuera de los limites del segmento %lu\n",
                    offset, segment_id);
            return INVALID_FRAME;
        }
    } else {
        // Tabla de segmentos MISS: cargar el segmento
        uint64_t frame = handle_segment_fault(ctx, segment_id, config, metrics);

        if (frame == INVALID_FRAME) {
            return INVALID_FRAME;
        }
    }

    // Calcular direccion fisica 
    // PA = base + offset
    uint64_t physical_addr = ctx->segment_table->entries[segment_id].base + offset;

    // Medir tiempo de fin y acumular 
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9
                      + (end.tv_nsec - start.tv_nsec);
    metrics->total_translation_time_ns += elapsed_ns;
    metrics->total_ops++;

    return physical_addr;
}

// Entradas: ctx (contexto de segmentacion a destruir)
// Salidas: ninguna
// Descripcion: Libera la tabla de segmentos (entradas + estructura)
//              y la estructura del contexto. NO libera el frame allocator
//              porque es compartido por todos los threads y se libera aparte
//              desde simulator.c despues de hacer join a todos los threads.
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
