/* Autores:
 * - Vicente Carrasco (20782358)
 * - Maximiliano Ojeda (205532153)
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame_allocator.h"


// Entradas: fa (frame allocator)
// Salidas: ninguna
// Descripción: Adquiere el lock si estamos en modo SAFE.
//              En modo UNSAFE no hace nada (permite carreras de datos).
static void fa_lock(frame_allocator_t *fa)
{
    if (!fa->unsafe) {
        pthread_mutex_lock(&fa->lock);
    }
}

// Entradas: fa (frame allocator)
// Salidas: ninguna
// Descripción: Libera el lock si estamos en modo SAFE
static void fa_unlock(frame_allocator_t *fa)
{
    if (!fa->unsafe) {
        pthread_mutex_unlock(&fa->lock);
    }
}

// Entradas: fa (frame allocator), frame_number (frame a agregar a la cola)
// Salidas: ninguna
// Descripción: Agrega un frame al final de la cola FIFO.
//              Se llama cuando se asigna un frame a una página.
static void fifo_push(frame_allocator_t *fa, uint64_t frame_number)
{
    // Crear nuevo nodo para la cola
    fifo_node_t *node = malloc(sizeof(fifo_node_t));
    if (!node) {
        fprintf(stderr, "Error: no se pudo asignar memoria para nodo FIFO\n");
        return;
    }
    node->frame_number = frame_number;
    node->next = NULL;

    // Insertar al final de la lista enlazada
    if (fa->fifo_tail == NULL) {
        // Cola vacía el nuevo nodo es cabeza y cola a la vez
        fa->fifo_head = node;
        fa->fifo_tail = node;
    } else {
        // Agregar después del último y actualizar tail
        fa->fifo_tail->next = node;
        fa->fifo_tail = node;
    }
}

// Entradas: fa (frame allocator)
// Salidas: número de frame sacado de la cabeza o INVALID_FRAME si vacía
// Descripción: Saca y retorna el frame más antiguo de la cola FIFO.
static uint64_t fifo_pop(frame_allocator_t *fa)
{
    if (fa->fifo_head == NULL) {
        return INVALID_FRAME;
    }

    // Sacar el nodo de la cabeza 
    fifo_node_t *old_head = fa->fifo_head;
    uint64_t frame_number = old_head->frame_number;

    // Avanzar la cabeza al siguiente
    fa->fifo_head = old_head->next;

    // Si la cola quedó vacía limpiar también el tail
    if (fa->fifo_head == NULL) {
        fa->fifo_tail = NULL;
    }

    free(old_head);
    return frame_number;
}


// Entradas: num_frames (total de frames físicos), unsafe (1=sin locks)
// Salidas: puntero al frame_allocator creado o NULL si falla
// Descripción: Crea el frame allocator. Inicializa todos los frames como
//              libres (occupied=0) y la cola FIFO vacia
frame_allocator_t *frame_alloc_create(int num_frames, int unsafe)
{
    frame_allocator_t *fa = malloc(sizeof(frame_allocator_t));
    if (!fa) {
        fprintf(stderr, "Error: no se pudo crear frame allocator\n");
        return NULL;
    }

    // Asignar arreglo de información por frame
    fa->frames = calloc(num_frames, sizeof(frame_info_t));
    if (!fa->frames) {
        fprintf(stderr, "Error: no se pudo asignar arreglo de frames\n");
        free(fa);
        return NULL;
    }

    fa->num_frames = num_frames;
    fa->free_count = num_frames;    // Todos los frames comienzan libres

    // Inicializar cada frame como libre
    for (int i = 0; i < num_frames; i++) {
        fa->frames[i].thread_id = -1;
        fa->frames[i].vpn = INVALID_FRAME;
        fa->frames[i].occupied = 0;
    }

    // Cola FIFO vacía 
    fa->fifo_head = NULL;
    fa->fifo_tail = NULL;

    // Configurar sincronización
    fa->unsafe = unsafe;
    if (!unsafe) {
        pthread_mutex_init(&fa->lock, NULL);
    }

    return fa;
}

// Entradas: fa (frame allocator), thread_id (quién pide), vpn (página a cargar)
// Salidas: número de frame asignado o INVALID_FRAME si no hay libres
// Descripción: Recorre el arreglo de frames buscando uno libre 
//              Si lo encuentra lo marca como ocupado guarda quién es el dueño
//              (thread_id + vpn) lo agrega a la cola FIFO y retorna su número
//              Si no hay libres retorna INVALID_FRAME para que el caller
//              invoque frame_alloc_evict() y luego reintente.
uint64_t frame_alloc_get(frame_allocator_t *fa, int thread_id, uint64_t vpn)
{
    fa_lock(fa);

    // Buscar un frame libre recorriendo el arreglo
    uint64_t frame = INVALID_FRAME;
    for (int i = 0; i < fa->num_frames; i++) {
        if (!fa->frames[i].occupied) {
            // Encontramos un frame libre marcarlo como ocupado
            fa->frames[i].occupied = 1;
            fa->frames[i].thread_id = thread_id;
            fa->frames[i].vpn = vpn;
            fa->free_count--;

            // Agregar a la cola FIFO 
            fifo_push(fa, (uint64_t)i);

            frame = (uint64_t)i;
            break;
        }
    }

    fa_unlock(fa);
    return frame;
}

// Entradas: fa (frame allocator)
// Salidas: número de frame de la víctima
// Descripción: Selecciona la página víctima usando FIFO saca la más antigua
//              de la cola. Marca ese frame como libre 
//              El caller es responsable de
//                Invalidar la entrada en la tabla de páginas del thread dueño
//                Invalidar la entrada en las TLBs correspondientes
//              Despues de invalidar el caller debe llamar frame_alloc_get()
//              para reasignar el frame a la nueva página
uint64_t frame_alloc_evict(frame_allocator_t *fa)
{
    fa_lock(fa);

    // Sacar el frame más antiguo de la cola FIFO
    uint64_t victim_frame = fifo_pop(fa);

    if (victim_frame != INVALID_FRAME) {
        // Marcar el frame como libre
        fa->frames[victim_frame].occupied = 0;
        fa->free_count++;
        // No limpiamos thread_id ni vpn aquí porque frame_alloc_get_info()
        // los necesita para saber a quién invalidar
    }

    fa_unlock(fa);
    return victim_frame;
}

// Entradas: fa (frame allocator), frame (número de frame a consultar)
// Salidas: puntero a frame_info_t con la info del frame
// Descripción: Retorna la información del frame .
//              Útil después de evictar para saber qué thread y VPN invalidar.
//              No toma lock porque se usa justo después de evict
//              y los datos del frame evictado no cambian hasta que se reasigne.
frame_info_t *frame_alloc_get_info(frame_allocator_t *fa, uint64_t frame)
{
    if (frame >= (uint64_t)fa->num_frames) {
        return NULL;
    }
    return &fa->frames[frame];
}

// Entradas: fa (frame allocator a destruir)
// Salidas: ninguna
// Descripción: Libera toda la memoria del frame allocator:
//              el arreglo de frames, todos los nodos de la cola FIFO,
//              destruye el mutex y libera la estructura principal.
void frame_alloc_destroy(frame_allocator_t *fa)
{
    if (!fa) return;

    // Liberar todos los nodos restantes de la cola FIFO
    fifo_node_t *current = fa->fifo_head;
    while (current) {
        fifo_node_t *next = current->next;
        free(current);
        current = next;
    }

    // Destruir mutex si estamos en modo SAFE
    if (!fa->unsafe) {
        pthread_mutex_destroy(&fa->lock);
    }

    // Liberar arreglo de frames y la estructura principal
    free(fa->frames);
    free(fa);
}
