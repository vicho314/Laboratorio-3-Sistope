/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "simulator.h"
#include "paginacion.h"
#include "segmentacion.h"
#include "frame_allocator.h"
#include "tlb.h"
#include "workloads.h"


// Estructura para pasar argumentos a los threads
typedef struct {
    int thread_id;
    sim_config_t *config;
    void *context;  // paging_context_t* o segmentation_context_t*
    uint64_t ops_per_thread;
    int workload_type;  // 0 = uniform, 1 = 80-20
    thread_metrics_t *metrics;
} thread_args_t;


// Función que ejecuta cada thread
void *thread_worker(void *arg)
{
    thread_args_t *args = (thread_args_t *)arg;
    
    for (uint64_t i = 0; i < args->ops_per_thread; i++) {
        uint64_t virtual_addr;
        
        // Generar dirección virtual según workload type
        if (args->workload_type == 0) {
            virtual_addr = generate_virtual_address(args->config);
        } else {
            virtual_addr = generate_virtual_address_80_20(args->config);
        }
        
        // Traducir según modo
        if (args->config->mode == 0) {
            // PAGINACION
            paging_context_t *pctx = (paging_context_t *)args->context;
            paging_translate(pctx, virtual_addr, args->config, args->metrics);
        } else {
            // SEGMENTACION
            segmentation_context_t *sctx = (segmentation_context_t *)args->context;
            segmentation_translate(sctx, virtual_addr, args->config, args->metrics);
        }
    }
    
    free(args);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    // Valores por defecto
    sim_config_t config = {
        .page_size = 4096,
        .num_pages = 64,
        .tlb_size = 16,
        .frames = 32,
        .threads = 1,
        .num_segments = 4,
        .segment_size = 4096,
        .mode = -1  // Obligatorio especificar
    };
    
    int ops_per_thread = 1000;
    int workload_type = 0;  // 0 = uniform, 1 = 80-20
    int seed = 42;
    int unsafe_mode = 0;
    int print_stats = 0;
    
    // Si no hay argumentos, imprimir uso
    if (argc < 2) {
        fprintf(stderr, "Uso: %s [opciones]\n", argv[0]);
        fprintf(stderr, "  --mode {seg|page} (obligatorio)\n");
        fprintf(stderr, "  --threads INT (default: 1)\n");
        fprintf(stderr, "  --ops-per-thread INT (default: 1000)\n");
        fprintf(stderr, "  --workload {uniform|80-20} (default: uniform)\n");
        fprintf(stderr, "  --seed INT (default: 42)\n");
        fprintf(stderr, "  --unsafe (deshabilita locks)\n");
        fprintf(stderr, "  --stats (imprime estadísticas detalladas)\n");
        fprintf(stderr, "\nFlags específicos de segmentación:\n");
        fprintf(stderr, "  --segments INT (default: 4)\n");
        fprintf(stderr, "  --seg-limits CSV (default: todos 4096)\n");
        fprintf(stderr, "\nFlags específicos de paginación:\n");
        fprintf(stderr, "  --pages INT (default: 64)\n");
        fprintf(stderr, "  --frames INT (default: 32)\n");
        fprintf(stderr, "  --page-size INT (default: 4096)\n");
        fprintf(stderr, "  --tlb-size INT (default: 16)\n");
        return 1;
    }
    
    // Procesar opciones con getopt
    int opt;
    struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"threads", required_argument, 0, 't'},
        {"ops-per-thread", required_argument, 0, 'o'},
        {"workload", required_argument, 0, 'w'},
        {"seed", required_argument, 0, 's'},
        {"unsafe", no_argument, 0, 'u'},
        {"stats", no_argument, 0, 'S'},
        {"segments", required_argument, 0, 'g'},
        {"seg-limits", required_argument, 0, 'l'},
        {"pages", required_argument, 0, 'p'},
        {"frames", required_argument, 0, 'f'},
        {"page-size", required_argument, 0, 'P'},
        {"tlb-size", required_argument, 0, 'T'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "m:t:o:w:s:uS", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':  // --mode
                if (strcmp(optarg, "seg") == 0) {
                    config.mode = 1;
                } else if (strcmp(optarg, "page") == 0) {
                    config.mode = 0;
                } else {
                    fprintf(stderr, "Error: mode debe ser 'seg' o 'page'\n");
                    return 1;
                }
                break;
            case 't':
                config.threads = atoi(optarg);
                break;
            case 'o':
                ops_per_thread = atoi(optarg);
                break;
            case 'w':
                if (strcmp(optarg, "uniform") == 0) {
                    workload_type = 0;
                } else if (strcmp(optarg, "80-20") == 0) {
                    workload_type = 1;
                } else {
                    fprintf(stderr, "Error: workload debe ser 'uniform' o '80-20'\n");
                    return 1;
                }
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'u':
                unsafe_mode = 1;
                break;
            case 'S':
                print_stats = 1;
                break;
            case 'g':  // --segments
                config.num_segments = atoi(optarg);
                break;
            case 'l':  // --seg-limits (se ignora por ahora, todos 4096)
                // TODO: parsear lista CSV de límites
                break;
            case 'p':  // --pages
                config.num_pages = atoi(optarg);
                break;
            case 'f':  // --frames
                config.frames = atoi(optarg);
                break;
            case 'P':  // --page-size
                config.page_size = atoi(optarg);
                break;
            case 'T':  // --tlb-size
                config.tlb_size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Opción desconocida\n");
                return 1;
        }
    }
    
    // Validar que el modo fue especificado
    if (config.mode == -1) {
        fprintf(stderr, "Error: debe especificar --mode {seg|page}\n");
        return 1;
    }
    
    // Inicializar semilla
    srand(seed);
    
    // Crear frame allocator (compartido por todos los threads)
    frame_allocator_t *fa = frame_alloc_create(config.frames, unsafe_mode);
    if (!fa) {
        fprintf(stderr, "Error: no se pudo crear frame allocator\n");
        return 1;
    }
    
    // Crear arrays de contextos para invalidación cruzada
    void **contexts = malloc(config.threads * sizeof(void *));
    thread_metrics_t *all_metrics = malloc(config.threads * sizeof(thread_metrics_t));
    
    if (!contexts || !all_metrics) {
        fprintf(stderr, "Error: no se pudo asignar memoria para contextos\n");
        return 1;
    }
    
    // Inicializar métricas
    for (int i = 0; i < config.threads; i++) {
        memset(&all_metrics[i], 0, sizeof(thread_metrics_t));
    }
    
    pthread_t *threads = malloc(config.threads * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "Error: no se pudo asignar memoria para threads\n");
        return 1;
    }
    
    // Medir tiempo total de ejecución
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (config.mode == 0) {
        // PAGINACION
        // Para paginación, necesitamos TLB si tlb_size > 0
        tlb_t **all_tlbs = malloc(config.threads * sizeof(tlb_t *));
        page_table_t **all_page_tables = malloc(config.threads * sizeof(page_table_t *));
        
        if (!all_tlbs || !all_page_tables) {
            fprintf(stderr, "Error: no se pudo asignar memoria\n");
            return 1;
        }
        
        // Crear contextos de paginación para cada thread
        for (int i = 0; i < config.threads; i++) {
            paging_context_t *pctx = paging_context_create(&config, i, fa, all_tlbs, all_page_tables);
            if (!pctx) {
                fprintf(stderr, "Error: no se pudo crear contexto de paginación para thread %d\n", i);
                return 1;
            }
            contexts[i] = (void *)pctx;
            all_tlbs[i] = pctx->tlb;
            all_page_tables[i] = pctx->page_table;
        }
        
        // Crear threads de trabajo
        for (int i = 0; i < config.threads; i++) {
            thread_args_t *args = malloc(sizeof(thread_args_t));
            args->thread_id = i;
            args->config = &config;
            args->context = contexts[i];
            args->ops_per_thread = ops_per_thread;
            args->workload_type = workload_type;
            args->metrics = &all_metrics[i];
            
            if (pthread_create(&threads[i], NULL, thread_worker, args) != 0) {
                fprintf(stderr, "Error: no se pudo crear thread %d\n", i);
                return 1;
            }
        }
        
        // Esperar a que terminen todos los threads
        for (int i = 0; i < config.threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
        // Liberar contextos de paginación
        for (int i = 0; i < config.threads; i++) {
            paging_context_destroy((paging_context_t *)contexts[i]);
        }
        
        free(all_tlbs);
        free(all_page_tables);
        
    } else if (config.mode == 1) {
        // SEGMENTACION
        segment_table_t **all_segment_tables = malloc(config.threads * sizeof(segment_table_t *));
        
        if (!all_segment_tables) {
            fprintf(stderr, "Error: no se pudo asignar memoria\n");
            return 1;
        }
        
        // Crear contextos de segmentación para cada thread
        for (int i = 0; i < config.threads; i++) {
            segmentation_context_t *sctx = segmentation_context_create(&config, i, fa, all_segment_tables);
            if (!sctx) {
                fprintf(stderr, "Error: no se pudo crear contexto de segmentación para thread %d\n", i);
                return 1;
            }
            contexts[i] = (void *)sctx;
            all_segment_tables[i] = sctx->segment_table;
        }
        
        // Crear threads de trabajo
        for (int i = 0; i < config.threads; i++) {
            thread_args_t *args = malloc(sizeof(thread_args_t));
            args->thread_id = i;
            args->config = &config;
            args->context = contexts[i];
            args->ops_per_thread = ops_per_thread;
            args->workload_type = workload_type;
            args->metrics = &all_metrics[i];
            
            if (pthread_create(&threads[i], NULL, thread_worker, args) != 0) {
                fprintf(stderr, "Error: no se pudo crear thread %d\n", i);
                return 1;
            }
        }
        
        // Esperar a que terminen todos los threads
        for (int i = 0; i < config.threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
        // Liberar contextos de segmentación
        for (int i = 0; i < config.threads; i++) {
            segmentation_context_destroy((segmentation_context_t *)contexts[i]);
        }
        
        free(all_segment_tables);
    } else {
        fprintf(stderr, "Error: modo no válido\n");
        return 1;
    }
    
    // Detener medición de tiempo
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double runtime_sec = (end_time.tv_sec - start_time.tv_sec) + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    // Sumar métricas totales de todos los threads
    uint64_t total_translations = 0;
    uint64_t total_segfaults = 0;
    uint64_t total_time_ns = 0;
    uint64_t total_tlb_hits = 0;
    uint64_t total_tlb_misses = 0;
    uint64_t total_evictions = 0;
    
    for (int i = 0; i < config.threads; i++) {
        total_translations += all_metrics[i].total_ops;
        total_segfaults += all_metrics[i].page_faults;  // page_faults = segfaults en segmentación
        total_time_ns += all_metrics[i].total_translation_time_ns;
        total_tlb_hits += all_metrics[i].tlb_hits;
        total_tlb_misses += all_metrics[i].tlb_misses;
        total_evictions += all_metrics[i].evictions;
    }
    
    // Calcular promedios
    double avg_translation_time_ns = 0;
    double throughput_ops_sec = 0;
    double hit_rate = 0.0;
    
    if (total_translations > 0) {
        avg_translation_time_ns = (double)total_time_ns / total_translations;
    }
    
    if (runtime_sec > 0) {
        throughput_ops_sec = total_translations / runtime_sec;
    }
    
    if (config.mode == 0) {
        // Calcular hit_rate solo para paginación (que tiene TLB)
        uint64_t total_tlb_accesses = total_tlb_hits + total_tlb_misses;
        if (total_tlb_accesses > 0) {
            hit_rate = (double)total_tlb_hits / total_tlb_accesses * 100.0;
        }
    }
    
    // Crear directorio out si no existe
    mkdir("out", 0755);
    
    // Escribir archivo JSON con resumen
    FILE *json_file = fopen("out/summary.json", "w");
    if (json_file) {
        fprintf(json_file, "{\n");
        fprintf(json_file, "  \"mode\": \"%s\",\n", config.mode == 0 ? "paging" : "segmentation");
        fprintf(json_file, "  \"config\": {\n");
        fprintf(json_file, "    \"threads\": %d,\n", config.threads);
        fprintf(json_file, "    \"ops_per_thread\": %d,\n", ops_per_thread);
        fprintf(json_file, "    \"workload\": \"%s\",\n", workload_type == 0 ? "uniform" : "80-20");
        fprintf(json_file, "    \"seed\": %u,\n", seed);
        fprintf(json_file, "    \"unsafe_mode\": %s",
                unsafe_mode ? "true" : "false");
        
        if (config.mode == 0) {
            fprintf(json_file, ",\n    \"page_size\": %u,\n", config.page_size);
            fprintf(json_file, "    \"num_pages\": %u,\n", config.num_pages);
            fprintf(json_file, "    \"tlb_size\": %u,\n", config.tlb_size);
            fprintf(json_file, "    \"frames\": %u\n", config.frames);
        } else {
            fprintf(json_file, ",\n    \"num_segments\": %u,\n", config.num_segments);
            fprintf(json_file, "    \"segment_size\": %u\n", config.segment_size);
        }
        
        fprintf(json_file, "  },\n");
        fprintf(json_file, "  \"metrics\": {\n");
        fprintf(json_file, "    \"total_operations\": %lu,\n", total_translations);
        fprintf(json_file, "    \"page_faults\": %lu,\n", total_segfaults);
        fprintf(json_file, "    \"evictions\": %lu,\n", total_evictions);
        
        if (config.mode == 0) {
            fprintf(json_file, "    \"tlb_hits\": %lu,\n", total_tlb_hits);
            fprintf(json_file, "    \"tlb_misses\": %lu,\n", total_tlb_misses);
            fprintf(json_file, "    \"hit_rate_percent\": %.2f,\n", hit_rate);
        }
        
        fprintf(json_file, "    \"avg_translation_time_ns\": %.2f,\n", avg_translation_time_ns);
        fprintf(json_file, "    \"throughput_ops_sec\": %.2f\n", throughput_ops_sec);
        fprintf(json_file, "  },\n");
        fprintf(json_file, "  \"runtime_sec\": %.6f\n", runtime_sec);
        fprintf(json_file, "}\n");
        fclose(json_file);
    } else {
        fprintf(stderr, "Warning: no se pudo crear archivo out/summary.json\n");
    }
    
    // Imprimir resultados
    if (print_stats) {
        printf("\n============== ESTADÍSTICAS DE SIMULACIÓN ==============\n");
        printf("Modo: %s\n", config.mode == 0 ? "Paginación" : "Segmentación");
        printf("Threads: %d\n", config.threads);
        printf("Ops per thread: %d\n", ops_per_thread);
        printf("Workload: %s\n", workload_type == 0 ? "uniform" : "80-20");
        printf("Seed: %d\n", seed);
        printf("Unsafe: %s\n", unsafe_mode ? "sí" : "no");
        printf("\nMétricas totales:\n");
        printf("  translations_ok: %lu\n", total_translations);
        printf("  segfaults: %lu\n", total_segfaults);
        printf("  avg_translation_time_ns: %.2f\n", avg_translation_time_ns);
        printf("  throughput_ops_sec: %.2f\n", throughput_ops_sec);
        
        printf("\nMétricas por thread:\n");
        for (int i = 0; i < config.threads; i++) {
            printf("  Thread %d:\n", i);
            printf("    translations_ok: %lu\n", all_metrics[i].total_ops);
            printf("    segfaults: %lu\n", all_metrics[i].page_faults);
            if (all_metrics[i].total_ops > 0) {
                double avg = (double)all_metrics[i].total_translation_time_ns / all_metrics[i].total_ops;
                printf("    avg_translation_time_ns: %.2f\n", avg);
            }
        }
        printf("======================================================\n\n");
    } else {
        // Formato compacto
        printf("%lu %lu %.2f %.2f\n", total_translations, total_segfaults, 
               avg_translation_time_ns, throughput_ops_sec);
    }
    
    // Liberar recursos
    frame_alloc_destroy(fa);
    free(contexts);
    free(all_metrics);
    free(threads);
    
    return 0;
}
