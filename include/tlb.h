/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */
#ifndef TLB_H
#define TLB_H

#include "simulator.h"


typedef struct {
    uint64_t vpn;           // Virtual Page Number
    uint64_t frame_number;  // Frame fisico correspondiente
    int valid;              // 1 = entrada valida, 0 = vacia
} tlb_entry_t;


typedef struct {
    tlb_entry_t *entries;   // Arreglo de entradas (tamano: size)
    int size;               // Tamano maximo de la TLB (0 = deshabilitada)
    int next_index;         // Indice donde se insertara la proxima entrada (FIFO)
    int count;              // Numero de entradas validas actuales
} tlb_t;


tlb_t *tlb_create(int size);


uint64_t tlb_lookup(tlb_t *tlb, uint64_t vpn);


void tlb_insert(tlb_t *tlb, uint64_t vpn, uint64_t frame);


void tlb_invalidate(tlb_t *tlb, uint64_t vpn);


void tlb_destroy(tlb_t *tlb);

#endif 
