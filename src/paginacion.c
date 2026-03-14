/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "paginacion.h"


// Entradas: ninguna
// Salidas: ninguna
// Descripcion: Simula el delay de cargar una pagina desde disco. En un sistema real esto tomaria milisegundos.
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

// Entradas: ctx (contexto de paginacion), victim_vpn (VPN de la pagina evictada),
//           victim_thread_id (thread dueno de la pagina evictada)
// Salidas: ninguna
// Descripcion: Cuando se evicta una pagina, hay que invalidarla en la tabla de
//              paginas del thread dueno y en todas las TLBs de todos los threads.
//              Esto es necesario porque si una TLB tiene cacheada una traduccion
//              que apunta a un frame que ya fue reasignado, la traduccion seria
//              incorrecta y apuntaria a datos de otra pagina.

static void invalidate_evicted_page(paging_context_t *ctx,
                                    uint64_t victim_vpn,
                                    int victim_thread_id)
{
    //Invalidar en la tabla de paginas del thread dueno
    if (victim_thread_id >= 0 && victim_thread_id < ctx->num_threads) {
        page_table_t *victim_pt = ctx->all_page_tables[victim_thread_id];
        if (victim_pt && victim_vpn < (uint64_t)victim_pt->num_pages) {
            victim_pt->entries[victim_vpn].valid = 0;
            victim_pt->entries[victim_vpn].frame_number = INVALID_FRAME;
        }
    }

    //  Invalidar en TODAS las TLBs de todos los threads
    
    for (int i = 0; i < ctx->num_threads; i++) {
        if (ctx->all_tlbs[i]) {
            tlb_invalidate(ctx->all_tlbs[i], victim_vpn);
        }
    }
}

// Entradas: ctx (contexto), vpn (pagina que necesita frame), config (configuracion),
//           metrics (metricas locales del thread)
// Salidas: numero de frame asignado o INVALID_FRAME si falla
// Descripcion: Maneja un page fault completo:
//               Intenta obtener un frame libre
//              Si no hay libres, evicta la pagina mas antigua (FIFO)
//               Simula la carga desde disco (nanosleep)
//               Actualiza la tabla de paginas y la TLB
static uint64_t handle_page_fault(paging_context_t *ctx, uint64_t vpn,
                                  sim_config_t *config, thread_metrics_t *metrics)
{
    // config se recibe por consistencia de interfaz 
    (void)config;

    // Contar el page fault
    metrics->page_faults++;

    //  Intentar obtener un frame libre
    uint64_t frame = frame_alloc_get(ctx->fa, ctx->thread_id, vpn);

    //  Si no hay frames libres, necesitamos evictar
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
            // Invalidar la pagina evictada en tabla de paginas y TLBs
            invalidate_evicted_page(ctx, victim_info->vpn, victim_info->thread_id);
        }

        // Contar la eviction
        metrics->evictions++;

        // Ahora el frame esta libre, asignarlo a nuestra pagina
        frame = frame_alloc_get(ctx->fa, ctx->thread_id, vpn);

        if (frame == INVALID_FRAME) {
            fprintf(stderr, "Error: frame no disponible despues de eviction\n");
            return INVALID_FRAME;
        }
    }

    //  Simular la carga de la pagina desde disco
    simulate_disk_load();

    // Actualizar la tabla de paginas de este thread
    ctx->page_table->entries[vpn].frame_number = frame;
    ctx->page_table->entries[vpn].valid = 1;

    //  Actualizar la TLB con la nueva traduccion
    tlb_insert(ctx->tlb, vpn, frame);

    return frame;
}

// Entradas: config (configuracion global), thread_id (ID del thread),
//           fa (frame allocator compartido),
//           all_tlbs (arreglo con punteros a las TLBs de todos los threads),
//           all_page_tables (arreglo con punteros a las tablas de todos los threads)
// Salidas: puntero a paging_context_t creado o NULL si falla
// Descripcion: Crea todo lo que un thread necesita para el modo paginacion:
//               Una tabla de paginas con num_pages entradas, todas invalidas
//               Una TLB de tamano tlb_size (0 = deshabilitada)
//               Referencias al frame allocator y a las estructuras de otros threads
paging_context_t *paging_context_create(sim_config_t *config, int thread_id,
                                        frame_allocator_t *fa,
                                        tlb_t **all_tlbs,
                                        page_table_t **all_page_tables)
{
    paging_context_t *ctx = malloc(sizeof(paging_context_t));
    if (!ctx) {
        fprintf(stderr, "Error: no se pudo crear contexto de paginacion\n");
        return NULL;
    }

    /* Vincula el hilo con el asignador global y las TLBs/tablas de todo el sistema 
   para permitir la invalidacion cruzada tras una expulsion de pagina (eviction). */
    ctx->thread_id = thread_id;
    ctx->fa = fa;
    ctx->all_tlbs = all_tlbs;
    ctx->all_page_tables = all_page_tables;
    ctx->num_threads = config->threads;

    // Crear tabla de paginas 
    ctx->page_table = malloc(sizeof(page_table_t));
    if (!ctx->page_table) {
        fprintf(stderr, "Error: no se pudo crear tabla de paginas\n");
        free(ctx);
        return NULL;
    }

    /* Define el numero de paginas y reserva memoria limpia para las entradas de la tabla. 
    Si la asignacion falla, libera los recursos previos para evitar fugas de memoria. */
    ctx->page_table->num_pages = config->num_pages;
    ctx->page_table->entries = calloc(config->num_pages, sizeof(page_table_entry_t));
    if (!ctx->page_table->entries) {
        fprintf(stderr, "Error: no se pudo asignar entradas de tabla de paginas\n");
        free(ctx->page_table);
        free(ctx);
        return NULL;
    }

    // Inicializar todas las paginas como invalidas
    for (int i = 0; i < config->num_pages; i++) {
        ctx->page_table->entries[i].frame_number = INVALID_FRAME;
        ctx->page_table->entries[i].valid = 0;
    }

    // Crear TLB 
    ctx->tlb = tlb_create(config->tlb_size);
    if (!ctx->tlb) {
        fprintf(stderr, "Error: no se pudo crear TLB\n");
        free(ctx->page_table->entries);
        free(ctx->page_table);
        free(ctx);
        return NULL;
    }

    return ctx;
}

// Entradas: ctx (contexto de paginacion del thread),
//           virtual_addr (direccion virtual generada por el workload),
//           config (configuracion global),
//           metrics (metricas locales del thread para registrar hits/misses/faults)
// Salidas: direccion fisica (PA) o INVALID_FRAME si falla
// Descripcion: Ejecuta el flujo completo de traduccion descrito al inicio del archivo.
//              Mide el tiempo de traduccion con clock_gettime para las metricas.
uint64_t paging_translate(paging_context_t *ctx, uint64_t virtual_addr,
                          sim_config_t *config, thread_metrics_t *metrics)
{
    //  Medir tiempo de inicio 
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    //  Descomponer VA en VPN y offset 
    uint64_t page_size = (uint64_t)config->page_size;
    uint64_t vpn = virtual_addr / page_size;
    uint64_t offset = virtual_addr % page_size;

    // Validar que VPN este en rango
    if (vpn >= (uint64_t)config->num_pages) {
        fprintf(stderr, "Error: VPN %lu fuera de rango (max %d)\n",
                vpn, config->num_pages - 1);
        return INVALID_FRAME;
    }

    uint64_t frame = INVALID_FRAME;

    //  Consultar TLB 
    frame = tlb_lookup(ctx->tlb, vpn);

    if (frame != INVALID_FRAME) {
        // TLB HIT: tenemos el frame no necesitamos consultar la tabla
        metrics->tlb_hits++;
    } else {
        // TLB MISS
        metrics->tlb_misses++;

        //  Consultar tabla de paginas 
        if (ctx->page_table->entries[vpn].valid) {
            // Tabla de paginas HIT: la pagina esta en memoria
            frame = ctx->page_table->entries[vpn].frame_number;

            // Actualizar TLB con esta traduccion 
            tlb_insert(ctx->tlb, vpn, frame);
        } else {
            // Tabla de paginas MISS: PAGE FAULT
            // Manejar page fault 
            frame = handle_page_fault(ctx, vpn, config, metrics);

            if (frame == INVALID_FRAME) {
                return INVALID_FRAME;
            }
        }
    }

    // Calcular direccion fisica 
    // PA = frame_number * page_size + offset
    uint64_t physical_addr = frame * page_size + offset;

    //  Medir tiempo de fin y acumular 
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9
                      + (end.tv_nsec - start.tv_nsec);
    metrics->total_translation_time_ns += elapsed_ns;
    metrics->total_ops++;

    return physical_addr;
}

// Entradas: ctx (contexto de paginacion a destruir)
// Salidas: ninguna
// Descripcion: Libera la tabla de paginas (entradas + estructura) la TLB,
//              y la estructura del contexto. NO libera el frame allocator
//              porque es compartido por todos los threads y se libera aparte
//              desde simulator.c despues de hacer join a todos los threads.
void paging_context_destroy(paging_context_t *ctx)
{
    if (!ctx) return;

    // Liberar tabla de paginas
    if (ctx->page_table) {
        if (ctx->page_table->entries) {
            free(ctx->page_table->entries);
        }
        free(ctx->page_table);
    }

    // Liberar TLB
    if (ctx->tlb) {
        tlb_destroy(ctx->tlb);
    }

    // NO libera ctx->fa es compartido
    // NO libera ctx->all_tlbs ni ctx->all_page_tables son arreglos compartidos

    free(ctx);
}
