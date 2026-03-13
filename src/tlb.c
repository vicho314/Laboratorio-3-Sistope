/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tlb.h"


// Entradas: size (tamano de la TLB)
// Salidas: puntero a tlb_t creada o NULL si falla malloc
// Descripcion: Reserva memoria para la estructura y el arreglo de entradas
//              Todas las entradas comienzan con valid=0 (vacias)
//              Si size es 0, se crea la estructura pero sin arreglo (TLB deshabilitada)
tlb_t *tlb_create(int size)
{
    tlb_t *tlb = malloc(sizeof(tlb_t));
    if (!tlb) {
        fprintf(stderr, "Error: no se pudo crear TLB\n");
        return NULL;
    }

    tlb->size = size;
    tlb->next_index = 0;
    tlb->count = 0;

    if (size > 0) {
        // Reservar arreglo de entradas e inicializar a cero (valid=0)
        tlb->entries = calloc(size, sizeof(tlb_entry_t));
        if (!tlb->entries) {
            fprintf(stderr, "Error: no se pudo asignar arreglo TLB\n");
            free(tlb);
            return NULL;
        }
    } else {
        // TLB deshabilitada no se necesita arreglo
        tlb->entries = NULL;
    }

    return tlb;
}

// Entradas: tlb (TLB donde buscar), vpn (pagina virtual a buscar)
// Salidas: frame_number si la VPN esta en la TLB (hit) INVALID_FRAME si no (miss)
// Descripcion: Recorre todas las entradas de la TLB buscando una que tenga
//              valid==1 y vpn igual al buscado. Es una busqueda lineal porque
//              la TLB es pequena (tipicamente 16-64 entradas).
//              Si la TLB esta deshabilitada (size==0) siempre retorna miss.
uint64_t tlb_lookup(tlb_t *tlb, uint64_t vpn)
{
    // Si TLB deshabilitada siempre miss
    if (!tlb || tlb->size == 0) {
        return INVALID_FRAME;
    }

    // Busqueda lineal en todas las entradas
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            // HIT: encontramos la traduccion en cache
            return tlb->entries[i].frame_number;
        }
    }

    // MISS: la VPN no esta en la TLB
    return INVALID_FRAME;
}

// Entradas: tlb (TLB donde insertar), vpn (pagina virtual), frame (frame fisico)
// Salidas: ninguna
// Descripcion: Inserta una nueva traduccion VPN->frame en la TLB
//              Primero verifica si la VPN ya existe. Si no existe usa next_index para insertar
//                 Si la TLB no esta llena, ocupa la siguiente posicion libre
//                 Si esta llena, sobreescribe la mas antigua 
//              next_index avanza circularmente despues de cada insercion
void tlb_insert(tlb_t *tlb, uint64_t vpn, uint64_t frame)
{
    // Si TLB deshabilitada, no hacer nada
    if (!tlb || tlb->size == 0) {
        return;
    }

    // Verificar si la VPN ya existe en la TLB para actualizarla
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            // Ya existe solo actualizar el frame
            tlb->entries[i].frame_number = frame;
            return;
        }
    }

    // No existe insertar en la posicion next_index 
    tlb->entries[tlb->next_index].vpn = vpn;
    tlb->entries[tlb->next_index].frame_number = frame;
    tlb->entries[tlb->next_index].valid = 1;

    // Avanzar next_index circularmente
    tlb->next_index = (tlb->next_index + 1) % tlb->size;

    // Actualizar contador
    if (tlb->count < tlb->size) {
        tlb->count++;
    }
}

// Entradas: tlb (TLB donde invalidar), vpn (pagina virtual a invalidar)
// Salidas: ninguna
// Descripcion: Busca la VPN en la TLB y la marca como invalida (valid=0).
//              Se usa cuando una pagina es evictada de memoria fisica:
//              si la TLB sigue apuntando a un frame que ya no tiene esa pagina,
//              produciria una traduccion incorrecta. Por eso hay que invalidar.
//              Si la VPN no esta en la TLB no hace nada.
void tlb_invalidate(tlb_t *tlb, uint64_t vpn)
{
    // Si TLB deshabilitada no hacer nada
    if (!tlb || tlb->size == 0) {
        return;
    }

    // Buscar la entrada con esa VPN y marcarla invalida
    for (int i = 0; i < tlb->size; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            tlb->entries[i].valid = 0;
            return;  // Solo puede haber una entrada por VPN
        }
    }
}

// Entradas: tlb (TLB a destruir)
// Salidas: ninguna
// Descripcion: Libera el arreglo de entradas y la estructura principal
void tlb_destroy(tlb_t *tlb)
{
    if (!tlb) return;

    if (tlb->entries) {
        free(tlb->entries);
    }
    free(tlb);
}
