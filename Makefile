# Autores:
# - Vicente Carrasco (207823589)
# - Maximiliano Ojeda (205532153)
#

CC = gcc
CFLAGS = -std=c11 -pthread -Wall -Wextra
LDFLAGS = -lm
INCLUDES = -I./include
SRCDIR = src
OBJDIR = build
BINDIR = .

# Archivos fuente
SRCS = $(SRCDIR)/simulator.c $(SRCDIR)/paginacion.c $(SRCDIR)/segmentacion.c \
       $(SRCDIR)/frame_allocator.c $(SRCDIR)/tlb.c $(SRCDIR)/workloads.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/simulator


.PHONY: all clean run reproduce help

# Target por defecto
all: $(TARGET)

# Crear directorio de objetos
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Regla de compilación de objetos
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Enlazador
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $@

# Limpiar archivos compilados
clean:
	@echo "Limpiando archivos compilados..."
	@rm -rf $(OBJDIR)
	@rm -f $(TARGET)
	@rm -rf out
	@echo "Limpieza completada"

# Ejecutar un ejemplo simple
run: all
	@mkdir -p out
	@./$(TARGET) --mode page --threads 2 --ops-per-thread 1000 \
		--pages 64 --frames 32 --tlb-size 16 --stats

# Reproduce: ejecuta el script experimento2.sh
reproduce: all
	@bash ./experimento1.sh
	@bash ./experimento2.sh
	@bash ./experimento3.sh

# Ayuda
help:
	@echo ""
	@echo "Laboratorio 3: Simulador de Memoria Virtual"
	@echo "Autores: Vicente Carrasco, Maximiliano Ojeda"
	@echo ""
	@echo "Targets disponibles:"
	@echo ""
	@echo "  make all      - Compila el proyecto (default)"
	@echo "  make clean    - Elimina archivos compilados"
	@echo "  make run      - Ejecuta un ejemplo simple"
	@echo "  make reproduce- Ejecuta 3 experimentos"
	@echo "  make help     - Muestra esta ayuda"
	@echo ""
