/* Autores:
 * - Vicente Carrasco (207823589)
 * - Maximiliano Ojeda (205532153)
 * */

#ifndef WORKLOADS_H
#define WORKLOADS_H

#include "simulator.h"


// Genera una dirección virtual uniformente distribuida en el espacio de direcciones
// Para paginación: VPN aleatorio uniforme, offset aleatorio uniforme
// Para segmentación: segmento aleatorio uniforme, offset aleatorio uniforme en [0, limit-1]
uint64_t generate_virtual_address(sim_config_t *config);


// Genera una dirección virtual con distribución 80-20 (principio de localidad)
// Simula que el 80% de accesos van al 20% de páginas/segmentos
// Para paginación: 80% accede a las primeras 20% VPNs, 20% al resto
// Para segmentación: 80% accede a los primeros 20% segmentos, 20% al resto
uint64_t generate_virtual_address_80_20(sim_config_t *config);


#endif
