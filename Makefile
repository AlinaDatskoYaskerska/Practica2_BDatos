# Compilación
CC = gcc -g
CFLAGS = -Wall -Wextra -pedantic -ansi
LDLIBS = -lodbc -lcurses -lpanel -lmenu -lform

# Archivos
HEADERS = odbc.h bpass.h lmenu.h search.h windows.h loop.h
EXE = menu
OBJ = $(EXE).o bpass.o loop.o search.o windows.o odbc.o

# Variables de base de datos
export PGDATABASE := flight
export PGUSER := alumnodb
export PGPASSWORD := alumnodb
export PGCLIENTENCODING := UTF8
export PGHOST := localhost

PSQL = psql
CREATEDB = createdb
DROPDB = dropdb --if-exists

# Objetivos

all: $(EXE) db-recreate

# Compilar el programa
$(EXE): $(OBJ)
	$(CC) -o $(EXE) $(OBJ) $(LDLIBS)

# Compilar archivos .c
%.o: %.c $(HEADERS)
	@echo Compiling $<...
	$(CC) $(CFLAGS) -c -o $@ $<

# Eliminar base de datos
dropdb:
	@echo "Eliminando base de datos $(PGDATABASE)..."
	@$(DROPDB) $(PGDATABASE) || true
	@rm -f *.log

# Crear base de datos
createdb:
	@echo "Creando base de datos $(PGDATABASE)..."
	@$(CREATEDB) $(PGDATABASE)

# Poblar base de datos desde flight.sql
populate:
	@echo "Poblando base de datos $(PGDATABASE) desde flight.sql..."
	@cat flight.sql | $(PSQL) $(PGDATABASE)

# Recrear base de datos (borrar, crear, poblar)
db-recreate: dropdb createdb populate

# Limpiar archivos compilados
clean:
	rm -f *.o core $(EXE)

# Ejecutar programa
run: $(EXE)
	./$(EXE)

# Ejecutar con valgrind
valgrind: $(EXE)
	valgrind --leak-check=full ./$(EXE)

# Abrir shell de psql
shell:
	@echo "Conectando a la base de datos $(PGDATABASE)..."
	@$(PSQL) $(PGDATABASE)

.PHONY: all dropdb createdb populate db-recreate clean run valgrind shell




