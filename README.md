# Laboratorio 3: Simulador Concurrente de Memoria Virtual

Autores
Vicente Carrasco (20782358)
Maximiliano Ojeda (205532153)

Simulador de memoria virtual que implementa los esquemas de segmentacion y paginacion de forma concurrente utilizando threads POSIX. Cada thread simula un proceso con su propio espacio de direcciones virtual, compitiendo por memoria fisica compartida.
El simulador incluye Translation Lookaside Buffer (TLB) con politica FIFO, manejo de page faults con delay simulado, eviction de paginas con politica FIFO global y dos modos de ejecucion: SAFE (con mutexes) y UNSAFE (sin proteccion, para observar carreras de datos).


# Requisitos

- Compilador C con soporte para C11 
- Soporte para pthreads 
- Sistema operativo Linux 
- make para compilacion


## Compilacion

## Esto compila todos los archivos fuente:
# make

## Para limpiar archivos compilados:
# make clean


## Ejecucion

## Ejecuta una simulacion de paginacion con parametros por defecto:
# make run

## Segmentacion:
# ./simulator --mode seg --threads 4 --workload uniform --ops-per-thread 5000 --stats


## Paginacion:
# ./simulator --mode page --threads 4 --frames 16 --tlb-size 32 --workload 80-20 --ops-per-thread 5000 --stats


### Modo UNSAFE, carrera de datos:
# ./simulator --mode page --threads 8 --frames 16 --tlb-size 32 --unsafe --stats


## Reproduccion de experimentos

## Este comando ejecuta los 3 experimentos obligatorios en secuencia y genera los archivos out/summary.json correspondientes a cada uno:
# make reproduce


## Experimento 1: Segmentacion

Demuestra que la segmentacion detecta correctamente violaciones de limite.

./simulator --mode seg --threads 1 --workload uniform \
    --ops-per-thread 10000 --segments 4 \
    --seg-limits 1024,2048,4096,8192 --seed 100 --stats


## Experimento 2: Impacto del TLB

Compara rendimiento con y sin TLB usando el mismo workload 80-20 y semilla.

# Sin TLB
./simulator --mode page --threads 1 --workload 80-20 \
    --ops-per-thread 50000 --pages 128 --frames 64 \
    --page-size 4096 --tlb-size 0 --seed 200 --stats

# Con TLB
./simulator --mode page --threads 1 --workload 80-20 \
    --ops-per-thread 50000 --pages 128 --frames 64 \
    --page-size 4096 --tlb-size 32 --seed 200 --stats


## Experimento 3: Thrashing con Multiples Threads


# 1 thread
./simulator --mode page --threads 1 --workload uniform \
    --ops-per-thread 10000 --pages 64 --frames 8 \
    --page-size 4096 --tlb-size 16 --seed 300 --stats

# 8 threads
./simulator --mode page --threads 8 --workload uniform \
    --ops-per-thread 10000 --pages 64 --frames 8 \
    --page-size 4096 --tlb-size 16 --seed 300 --stats


## Salida JSON

Cada ejecucion genera un archivo out/summary.json con la configuracion y metricas en formato JSON, lo que permite reproducir y comparar ejecuciones de forma programatica.