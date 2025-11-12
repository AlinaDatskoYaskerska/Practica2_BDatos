/*
* Created by roberto on 3/5/21.
*/
#include "search.h"
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>

/* Query principal */
static const char *query = 
    "WITH trasbordos AS ( SELECT DISTINCT f1.flight_id AS primer_id, f2.flight_id AS segundo_id, f1.scheduled_departure AS primer_departure, f2.scheduled_departure AS segundo_departure, f1.scheduled_arrival AS primer_arrival, f2.scheduled_arrival AS segundo_arrival, 1 AS conexion, LEAST( (SELECT COUNT(*) FROM seats s1 LEFT JOIN boarding_passes b1 ON b1.flight_id = f1.flight_id AND b1.seat_no = s1.seat_no WHERE s1.aircraft_code = f1.aircraft_code AND b1.seat_no IS NULL), (SELECT COUNT(*) FROM seats s2 LEFT JOIN boarding_passes b2 ON b2.flight_id = f2.flight_id AND b2.seat_no = s2.seat_no WHERE s2.aircraft_code = f2.aircraft_code AND b2.seat_no IS NULL) ) AS asientos_vacios, f1.aircraft_code AS primer_code, f2.aircraft_code AS segundo_code FROM flights f1 JOIN flights f2 ON f1.arrival_airport = f2.departure_airport WHERE (f2.scheduled_arrival - f1.scheduled_departure) > INTERVAL '0 seconds' AND (f2.scheduled_arrival - f1.scheduled_departure) <= INTERVAL '1 day' AND f1.scheduled_arrival < f2.scheduled_departure AND f1.departure_airport = ? AND f2.arrival_airport = ? AND DATE(f1.scheduled_arrival) = ? AND f1.status <> 'Cancelled' AND f2.status <> 'Cancelled' GROUP BY f1.flight_id, f2.flight_id ), directos AS ( SELECT flights.flight_id AS primer_id, flights.flight_id AS segundo_id, flights.scheduled_departure AS primer_departure, flights.scheduled_departure AS segundo_departure, flights.scheduled_arrival AS primer_arrival, flights.scheduled_arrival AS segundo_arrival, 0 AS conexion, COUNT(seats.seat_no) FILTER (WHERE boarding_passes.seat_no IS NULL) AS asientos_vacios, flights.aircraft_code AS primer_code, flights.aircraft_code AS segundo_code FROM flights JOIN seats ON flights.aircraft_code = seats.aircraft_code LEFT JOIN boarding_passes ON (flights.flight_id = boarding_passes.flight_id AND seats.seat_no = boarding_passes.seat_no AND flights.status <> 'Cancelled') WHERE flights.departure_airport = ? AND flights.arrival_airport = ? AND DATE(flights.scheduled_arrival) = ? AND (flights.scheduled_arrival - flights.scheduled_departure) > INTERVAL '0 seconds' AND (flights.scheduled_arrival - flights.scheduled_departure) <= INTERVAL '1 day' AND flights.status <> 'Cancelled' GROUP BY flights.flight_id ), total_vuelos AS ( SELECT trasbordos.primer_id, trasbordos.segundo_id, trasbordos.primer_departure, trasbordos.segundo_departure, trasbordos.primer_arrival, trasbordos.segundo_arrival, trasbordos.asientos_vacios, trasbordos.conexion, trasbordos.primer_code, trasbordos.segundo_code FROM trasbordos UNION ALL SELECT directos.primer_id, directos.segundo_id, directos.primer_departure, directos.segundo_departure, directos.primer_arrival, directos.segundo_arrival, directos.asientos_vacios, directos.conexion, directos.primer_code, directos.segundo_code FROM directos ) SELECT * FROM total_vuelos WHERE total_vuelos.asientos_vacios > 0 ORDER BY (total_vuelos.segundo_arrival - total_vuelos.primer_departure);";

/* Query de comprobacion para ver si el aeropuerto existe */
static const char *query_combrobacion =
    "SELECT airport_code FROM airports_data WHERE airport_code = ?;";

/**
 * @param from form field from
 * @param to form field to
 * @param date form field date
 * @param n_choices fill this with the number of results
 * @param choices fill this with the actual results
 * @param choices_msg aqui es donde guardamos la informacion extra que se tiene que imprimir en Msg
 * @param max_length output win maximum width
 * @param max_rows output win maximum number of rows
 * 
 * @author Lucas Manuel Blanco Rodriguez
  */
void    results_search(char * from, char *to, char *date,
                       int * n_choices, char *** choices,
                       char *** choices_msg,
                       int max_length)
{
    int i=0;
    int t=0;
    /* Usamos este array para copiar todos los datos obtenidos aqui primero y luego en choices y choices_msg */
    char query_result_set[1024][1024];
    int n_rows = 0;

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret; /* ODBC API return status */
    SQLCHAR primer_id[256], segundo_id[256], 
            primer_departure[256], segundo_departure[256],
            primer_arrival[256], segundo_arrival[256], 
            asientos_vacios[256], conexion[16],
            primer_code[256], segundo_code[256],
            comprobacion[256]; /* Todos los char necesarios para las queries */

    if (!from || !to || !date || from[0] == '\0' || to[0] == '\0' || date[0] == '\0' || from[0] == ' ' || to[0] == ' ' || date[0] == ' ') {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Debes de escribir algo, hay campos vacíos.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        *n_choices = 0;
        return;
    }

    /* Paso de fecha YYYY/MM/DD a YYYY-MM-DD para que sea compatible con PostgreSql*/
    date[4] = '-';
    date[7] = '-';

    /* CONNECT */
    ret = odbc_connect(&env, &dbc);
    if (!SQL_SUCCEEDED(ret)) {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al conectar con la base de datos.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        *n_choices = 0;
        return;
    }

    /* Allocate a statement handle */
    /* Lo allocamos primero para la query de comprobacion */
    /* Seguimos el orden que viene en los ejemplos: Alloc, Prepare, BindParameter, Execute, BindCol y Fetch*/
    /* Comprobamos primero si el aeropuerto de salida existe */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ret = SQLPrepare(stmt, (SQLCHAR *)query_combrobacion, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(from), 0, from, 0, NULL); 
    ret = SQLExecute(stmt);
    SQLBindCol(stmt, 1, SQL_C_CHAR, comprobacion, sizeof(comprobacion), NULL);

    ret = SQLFetch(stmt);
    if (ret == SQL_NO_DATA) {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "El aeropuerto de salida no existe.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    SQLCloseCursor(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    /* Allocate a statement handle */
    /* Repetimos para el aeropuerto de llegada */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ret = SQLPrepare(stmt, (SQLCHAR *)query_combrobacion, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(to), 0, to, 0, NULL);
    ret = SQLExecute(stmt);
    SQLBindCol(stmt, 1, SQL_C_CHAR, comprobacion, sizeof(comprobacion), NULL);

    ret = SQLFetch(stmt);
    if (ret == SQL_NO_DATA) {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "El aeropuerto de llegada no existe.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    /* Allocate a statement handle */
    /* Preparamos la consulta principal */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ret = SQLPrepare(stmt, (SQLCHAR *)query, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLPrepare", stmt, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al preparar la consulta de vuelos.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* A cada Prepared Statement le unimos su parámetro */
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(from), 0, from, 0, NULL);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(to), 0, to, 0, NULL);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(date), 0, date, 0, NULL);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(from), 0, from, 0, NULL);
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(to), 0, to, 0, NULL);
    SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, strlen(date), 0, date, 0, NULL);

    /* Si da error puede ser porque el formato de la fecha es incorrecto */
    /* Si el usuario lo ha introducido mal, SQL_SUCCEEDED muestra error */
    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLExecute", stmt, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Fecha no válida o error en los datos proporcionados.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* A cada columna le unimos su char */
    SQLBindCol(stmt, 1, SQL_C_CHAR, primer_id, sizeof(primer_id), NULL);
    SQLBindCol(stmt, 2, SQL_C_CHAR, segundo_id, sizeof(segundo_id), NULL);
    SQLBindCol(stmt, 3, SQL_C_CHAR, primer_departure, sizeof(primer_departure), NULL);
    SQLBindCol(stmt, 4, SQL_C_CHAR, segundo_departure, sizeof(segundo_departure), NULL);
    SQLBindCol(stmt, 5, SQL_C_CHAR, primer_arrival, sizeof(primer_arrival), NULL);
    SQLBindCol(stmt, 6, SQL_C_CHAR, segundo_arrival, sizeof(segundo_arrival), NULL);
    SQLBindCol(stmt, 7, SQL_C_CHAR, asientos_vacios, sizeof(asientos_vacios), NULL);
    SQLBindCol(stmt, 8, SQL_C_CHAR, conexion, sizeof(conexion), NULL);
    SQLBindCol(stmt, 9, SQL_C_CHAR, primer_code, sizeof(primer_code), NULL);
    SQLBindCol(stmt, 10, SQL_C_CHAR, segundo_code, sizeof(segundo_code), NULL);

    /* Codemos la primera fila y vemos si se han conseguido datos */
    ret = SQLFetch(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLFetch", stmt, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Fecha no válida o error en los datos proporcionados.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    if (ret == SQL_NO_DATA) {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "No hay resultados para los datos seleccionados.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    
    /* Añadimos el encabezado en el array */
    snprintf(query_result_set[0], sizeof(query_result_set[0]), "%-6s | %-8s | %-8s | %-6s | %-8s", "ID", "Salida", "Llegada", "Vacios", "Conexion");
    n_rows = 1;
    i = 1;

    /* Ahora vamos sacando los resultados y copiandolos en query_result_set */
    /* En las posiciones pares esta la informacion de msg y en las impares los datos que se printearan en Out */
    /* En las fechas hemos decidido solo guardar la hora */
    do {
        snprintf(query_result_set[i], sizeof(query_result_set[0]),
                 "%-6s | %-8s | %-8s | %-6s | %-8s",
                 primer_id, primer_departure + 11, segundo_arrival + 11, asientos_vacios, conexion);

        if (atoi((char *)conexion) == 0) {
            snprintf(query_result_set[i+1], sizeof(query_result_set[0]), "%s", primer_code);
        } else {
            snprintf(query_result_set[i+1], sizeof(query_result_set[0]),
                     "%s %s %s %s %s", 
                     primer_arrival, primer_code, segundo_departure, segundo_id, segundo_code);
        }
        n_rows++;
        i += 2;
    } while (SQL_SUCCEEDED(ret = SQLFetch(stmt)) && i + 1 < 1024);

    /* El numero de elecciones son las filas que hemos sacado */
    *n_choices = n_rows;

    /* Copiamos el encabezado en la posicion 0 */
    t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
    strncpy((*choices)[0], query_result_set[0], t);
    strncpy((*choices_msg)[0], query_result_set[0], t);

    /* Vamos guardando la informacion obtenida donde corresponde */
    for (i = 1; i < *n_choices; i++) {
        int idx_out = 2 * i - 1;
        int idx_msg = 2 * i;

        if (idx_msg < 1024) {
            t = MIN((int)strlen(query_result_set[idx_msg]) + 1, max_length);
            strncpy((*choices_msg)[i], query_result_set[idx_msg], t);
        }
        if (idx_out < 1024) {
            t = MIN((int)strlen(query_result_set[idx_out]) + 1, max_length);
            strncpy((*choices)[i], query_result_set[idx_out], t);
        }
    }
    
    /* free up statement handle */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    /* DISCONNECT */
    ret = odbc_disconnect(env, dbc);
    if (!SQL_SUCCEEDED(ret)) {
        return;
    }
    return;
}


