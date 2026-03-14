# Autores:
# - Vicente Carrasco (207823589)
# - Maximiliano Ojeda (205532153)
#
echo " EXPERIMENTO 3: Thrashing"
echo ""

# Crear directorio de salida si no existe
mkdir -p out

# Configuracion comun
# Solo cambia --threads entre ambas ejecuciones
# 8 frames es muy poco para 64 paginas por thread y peor con 8 threads
COMMON="--mode page --workload uniform --ops-per-thread 10000 \
--pages 64 --frames 8 --page-size 4096 --tlb-size 16 \
--tlb-policy fifo --evict-policy fifo --seed 300 --stats"

# Ejecucion 1: 1 thread 
echo ">>> Ejecucion 1: 1 thread"
echo "Comando: ./simulator $COMMON --threads 1"
echo ""
./simulator $COMMON --threads 1

# Guardar JSON
if [ -f out/summary.json ]; then
    cp out/summary.json out/exp3_1thread.json
    echo ""
    echo "[OK] Resultados guardados en out/exp3_1thread.json"
fi

echo ""
echo "----------------------------------------"
echo ""

#  Ejecucion 2: 8 threads 
echo ">>> Ejecucion 2: 8 threads"
echo "Comando: ./simulator $COMMON --threads 8"
echo ""
./simulator $COMMON --threads 8

# Guardar JSON
if [ -f out/summary.json ]; then
    cp out/summary.json out/exp3_8threads.json
    echo ""
    echo "[OK] Resultados guardados en out/exp3_8threads.json"
fi

echo " ANALISIS EXPERIMENTO 3"
echo "----------------------------------------"
echo "Comparacion esperada (1 vs 8 threads):"
echo " - Colapso: 8 hilos compitiendo por solo 8 frames globales genera"
echo "   thrashing severo (512 paginas virtuales vs 8 marcos fisicos)."
echo " - Rendimiento: El throughput cae drasticamente porque el sistema"
echo "   pasa mas tiempo en nanosleep (disco) que traduciendo."
echo " - Evictions: El numero de expulsiones se dispara ya que cada hilo"
echo "   quita la pagina que el otro acaba de cargar."
echo "----------------------------------------"

