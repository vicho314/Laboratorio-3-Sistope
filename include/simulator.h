/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdint.h>

#define INVALID_FRAME 0xFFFFFFFFFFFFFFFFULL
#define MAX_SEGMENTS 16


typedef struct {
    int page_size;              // Tamaño de página en bytes (ej: 4096)
    int num_pages;              // Número de páginas virtuales
    int tlb_size;               // Tamaño de TLB (0 = deshabilitada)
    int frames;                 // Número de frames físicos disponibles
    int threads;                // Número de threads
    int num_segments;           // Para segmentación: número de segmentos por thread
    int segment_size;           // Para segmentación: tamaño en bytes de cada segmento (default)
    uint32_t segment_limits[MAX_SEGMENTS];  // Límites específicos de cada segmento
    int mode;                   // 0 = paginacion, 1 = segmentacion
} sim_config_t;


typedef struct {
    uint64_t page_faults;           // Número de page faults
    uint64_t evictions;             // Número de evictions
    uint64_t tlb_hits;              // Hits en TLB
    uint64_t tlb_misses;            // Misses en TLB
    uint64_t total_translation_time_ns;  // Tiempo total de traducción en nanosegundos
    uint64_t total_ops;             // Número total de operaciones de traducción
} thread_metrics_t;

#endif
