# Autores:
# - Vicente Carrasco (207823589)
# - Maximiliano Ojeda (205532153)
#

echo " EXPERIMENTO 2: Impacto del TLB"
echo ""

# Crear directorio de salida si no existe
mkdir -p out

# Configuracion comun 
# Ambas ejecuciones usan exactamente los mismos parametros excepto --tlb-size
# Esto asegura que la unica variable que cambia es la TLB
COMMON="--mode page --threads 1 --workload 80-20 --ops-per-thread 50000 \
--pages 128 --frames 64 --page-size 4096 --tlb-policy fifo \
--evict-policy fifo --seed 200 --stats"

#  Ejecucion 1: Sin TLB 
echo ">>> Ejecucion 1: Sin TLB (--tlb-size 0)"
echo "Comando: ./simulator $COMMON --tlb-size 0"
echo ""
./simulator $COMMON --tlb-size 0

# Guardar JSON del experimento sin TLB
if [ -f out/summary.json ]; then
    cp out/summary.json out/exp2_sin_tlb.json
    echo ""
    echo "[OK] Resultados guardados en out/exp2_sin_tlb.json"
fi

echo ""
echo "----------------------------------------"
echo ""

#  Ejecucion 2: Con TLB 
echo ">>> Ejecucion 2: Con TLB (--tlb-size 32)"
echo "Comando: ./simulator $COMMON --tlb-size 32"
echo ""
./simulator $COMMON --tlb-size 32

# Guardar JSON del experimento con TLB
if [ -f out/summary.json ]; then
    cp out/summary.json out/exp2_con_tlb.json
    echo ""
    echo "[OK] Resultados guardados en out/exp2_con_tlb.json"
fi

echo " ANALISIS EXPERIMENTO 2"
echo "----------------------------------------"
echo "Comparacion esperada:"
echo " - Hit Rate: 0% (Sin TLB) vs >70% (Con TLB). La localidad 80-20"
echo "   hace que las traducciones frecuentes se queden en cache."
echo " - Tiempo: El acceso a TLB es mucho mas rapido que a la Tabla de Paginas."
echo " - Page Faults: Se mantienen iguales. La TLB solo acelera la"
echo "   traduccion VPN->PA, no evita la carga desde disco."
echo "----------------------------------------"

