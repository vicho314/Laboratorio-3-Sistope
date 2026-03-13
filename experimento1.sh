# Autores:
# - Vicente Carrasco (20782358)
# - Maximiliano Ojeda (205532153)
#

echo " EXPERIMENTO 1: Segmentación con Segfaults Controlados"
echo ""

# Crear directorio de salida si no existe
mkdir -p out

# Configuracion comun 
COMMON="--mode seg --threads 1 --workload uniform \
--ops-per-thread 50000 --segments 4 \
--seg-limits 1024,2048,4096,8192 --seed 100 --stats"

#  Ejecucion 1
echo ">>> Ejecucion : "
echo "Comando: ./simulator $COMMON "
echo ""
./simulator $COMMON

# Guardar JSON del experimento
if [ -f out/summary.json ]; then
    cp out/summary.json out/exp1.json
    echo ""
    echo "[OK] Resultados guardados en out/exp1.json"
fi

echo ""
echo "----------------------------------------"
echo ""


echo " ANALISIS EXPERIMENTO 1"
echo "----------------------------------------"
echo "Comparacion esperada:"
echo ""
echo "----------------------------------------"

