/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#include <stdlib.h>
#include "workloads.h"


// Entradas: config (configuracion del sistema)
// Salidas: dirección virtual de 64 bits
// Descripcion: Genera una dirección virtual uniformemente distribuida en todo el espacio de direcciones.
//              Para paginacion: selecciona VPN aleatorio uniforme [0, pages-1], 
//                               luego offset aleatorio [0, page_size-1]
//              Para segmentacion: selecciona segmento aleatorio uniforme [0, num_segments-1],
//                                 luego offset aleatorio uniforme [0, segment_size-1]
uint64_t generate_virtual_address(sim_config_t *config)
{
    uint64_t virtual_addr;

    if (config->mode == 0) {
        // PAGINACION
        // Seleccionar VPN aleatorio uniforme en [0, pages-1]
        uint64_t vpn = rand() % config->num_pages;
        
        // Seleccionar offset aleatorio en [0, page_size-1]
        uint64_t offset = rand() % config->page_size;
        
        // Dirección virtual = VPN * page_size + offset
        virtual_addr = vpn * config->page_size + offset;
    } else {
        // SEGMENTACION
        // Seleccionar segmento aleatorio uniforme en [0, num_segments-1]
        uint64_t segment = rand() % config->num_segments;
        
        // Seleccionar offset aleatorio uniforme en [0, segment_size-1]
        uint64_t offset = rand() % config->segment_size;
        
        // Dirección virtual = segment * segment_size + offset
        virtual_addr = segment * config->segment_size + offset;
    }

    return virtual_addr;
}


// Entradas: config (configuracion del sistema)
// Salidas: dirección virtual de 64 bits
// Descripcion: Genera una dirección virtual con distribución 80-20 (principio de localidad).
//              El 80% de accesos van al 20% de páginas/segmentos, el 20% al resto.
//              Para paginacion: 80% accede a las primeras 20% VPNs, 20% al resto
//              Para segmentacion: 80% accede a los primeros 20% segmentos, 20% al resto
uint64_t generate_virtual_address_80_20(sim_config_t *config)
{
    uint64_t virtual_addr;

    if (config->mode == 0) {
        // PAGINACION con distribución 80-20
        // Calcular el límite del 20%
        int threshold_20_percent = (config->num_pages * 20) / 100;
        if (threshold_20_percent == 0) threshold_20_percent = 1;
        
        uint64_t vpn;
        
        // 80% de probabilidad: acceder a las primeras 20% VPNs
        if ((rand() % 100) < 80) {
            vpn = rand() % threshold_20_percent;
        } else {
            // 20% de probabilidad: acceder al resto de VPNs
            vpn = threshold_20_percent + (rand() % (config->num_pages - threshold_20_percent));
        }
        
        // Seleccionar offset aleatorio en [0, page_size-1]
        uint64_t offset = rand() % config->page_size;
        
        // Dirección virtual = VPN * page_size + offset
        virtual_addr = vpn * config->page_size + offset;
    } else {
        // SEGMENTACION con distribución 80-20
        // Calcular el límite del 20%
        int threshold_20_percent = (config->num_segments * 20) / 100;
        if (threshold_20_percent == 0) threshold_20_percent = 1;
        
        uint64_t segment;
        
        // 80% de probabilidad: acceder a los primeros 20% segmentos
        if ((rand() % 100) < 80) {
            segment = rand() % threshold_20_percent;
        } else {
            // 20% de probabilidad: acceder al resto de segmentos
            segment = threshold_20_percent + (rand() % (config->num_segments - threshold_20_percent));
        }
        
        // Seleccionar offset aleatorio uniforme en [0, segment_size-1]
        uint64_t offset = rand() % config->segment_size;
        
        // Dirección virtual = segment * segment_size + offset
        virtual_addr = segment * config->segment_size + offset;
    }

    return virtual_addr;
}
