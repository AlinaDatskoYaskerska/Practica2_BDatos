CC = gcc -g
CFLAGS = -Wall -Wextra -pedantic -ansi
LDLIBS = -lodbc -lcurses -lpanel -lmenu -lform

HEADERS = odbc.h bpass.h lmenu.h search.h windows.h loop.h
EXE = menu
OBJ = $(EXE).o bpass.o loop.o search.o windows.o odbc.o

export PGDATABASE := flight
export PGUSER := alumnodb
export PGPASSWORD := alumnodb
export PGCLIENTENCODING := UTF8
export PGHOST := localhost

DBNAME =$(PGDATABASE)
PSQL = psql
CREATEDB = createdb
DROPDB = dropdb --if-exists
PG_DUMP = pg_dump
PG_RESTORE = pg_restore

compile: $(EXE)

all: db-recreate

$(EXE): $(OBJ)
	$(CC) -o $(EXE) $(OBJ) $(LDLIBS)

%.o: %.c $(HEADERS)
	@echo Compiling $<...
	$(CC) $(CFLAGS) -c -o $@ $<

dropdb:
	@echo "Eliminando base de datos $(PGDATABASE)..."
	@$(DROPDB) $(DBNAME) 
	@rm -f *.log

createdb:
	@echo "Creando BBDD"
	@$(CREATEDB) $(DBNAME)

populate:
	@echo "Poblando BBDD desde flight.sql..."
	@cat flight.sql | $(PSQL) $(DBNAME)

db-recreate: dropdb createdb populate

clean:
	rm -f *.o core $(EXE)

run: $(EXE)
	./$(EXE)

valgrind: $(EXE)
	valgrind --leak-check=full ./$(EXE) 2> valgrind.log
	@echo "Salida de Valgrind guardada en valgrind.log"

shell:
	@echo "Conectando a la BBDD"
	@$(PSQL) $(DBNAME)

.PHONY: all dropdb createdb populate db-recreate clean run valgrind shell





