/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include <pthread.h>
#include "simulator.h"



typedef struct {
    int thread_id;      // Thread dueño de la página cargada en este frame
    uint64_t vpn;       // VPN (Virtual Page Number) cargada en este frame
    int occupied;       // 1 = ocupado, 0 = libre
} frame_info_t;


typedef struct fifo_node {
    uint64_t frame_number;
    struct fifo_node *next;
} fifo_node_t;


typedef struct {
    frame_info_t *frames;       // Arreglo de info por frame (tamaño num_frames)
    int num_frames;             // Cantidad total de frames
    int free_count;             // Frames libres actualmente

    fifo_node_t *fifo_head;     // Cabeza de cola FIFO (página más antigua)
    fifo_node_t *fifo_tail;     // Cola FIFO (página más reciente)

    pthread_mutex_t lock;       // Mutex para modo SAFE
    int unsafe;                 // 1 = no usar locks
} frame_allocator_t;


frame_allocator_t *frame_alloc_create(int num_frames, int unsafe);


uint64_t frame_alloc_get(frame_allocator_t *fa, int thread_id, uint64_t vpn);


uint64_t frame_alloc_evict(frame_allocator_t *fa);


frame_info_t *frame_alloc_get_info(frame_allocator_t *fa, uint64_t frame);


void frame_alloc_destroy(frame_allocator_t *fa);

#endif 
