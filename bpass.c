/*
* Created by roberto on 3/5/21.
*/
#include "search.h"
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>

/* Query para sacar el nombre, flight_id, salida y ticket_no */
static const char *query = 
    "SELECT tickets.passenger_name, ticket_flights.flight_id, flights.scheduled_departure, ticket_flights.ticket_no FROM bookings JOIN tickets ON tickets.book_ref = bookings.book_ref JOIN ticket_flights ON ticket_flights.ticket_no = tickets.ticket_no JOIN flights ON ticket_flights.flight_id = flights.flight_id LEFT JOIN boarding_passes ON ticket_flights.flight_id = boarding_passes.flight_id AND ticket_flights.ticket_no = boarding_passes.ticket_no WHERE boarding_passes.boarding_no IS NULL AND bookings.book_ref = ? ORDER BY ticket_flights.ticket_no;";

/* Query para sacar el mayor boarind_no de un flight_id */
static const char *query_boarding_no =
    "SELECT COALESCE(MAX(boarding_no), 0) AS max_boarding_no FROM boarding_passes WHERE flight_id = ?;";

/* Query para sacar el primer asiento libre de un flight_id */
static const char *query_seat_no =
    "SELECT DISTINCT seats.seat_no, seats.aircraft_code FROM seats JOIN flights ON seats.aircraft_code = flights.aircraft_code LEFT JOIN boarding_passes ON flights.flight_id = boarding_passes.flight_id AND seats.seat_no = boarding_passes.seat_no WHERE boarding_passes.seat_no IS NULL AND flights.flight_id = ? ORDER BY seats.aircraft_code, seats.seat_no LIMIT 1;";

/* Query para insertar la nueva tarjeta de embarque segun los datos anteriores conseguidos */
static const char *query_insert = 
    "INSERT INTO boarding_passes (ticket_no, flight_id, boarding_no, seat_no) VALUES (?, ?, ?, ?);";


void    results_bpass(/*@unused@*/ char * bookID,
                       int * n_choices, char *** choices,
                       char *** choices_msg,
                       int max_length)
/**
* @param bookID  form field bookId
* @param n_choices fill this with the number of results
* @param choices fill this with the actual results
* @param choices_msg aqui guardamos la informacion que se imprimira en Msg
* @param max_length output win maximum width
* @param max_rows output win maximum number of rows
*
* @author Alina Datsko Yaskerska
*/

{
    int i=0;
    int t=0;
    char query_result_set[1024][1024];
    int n_rows = 0;

    SQLHENV env;
    SQLHDBC dbc;
    /* Como vamos a hacer varias queries simultaneamente, necesitamos varios statements */
    SQLHSTMT stmt_select, stmt_boarding, stmt_seat, stmt_insert;
    SQLRETURN ret; /* ODBC API return status */
    SQLCHAR nombre[21], flight_id[256], seat_no[256], ticket_no[256],
            departure[256], boarding_no[256];
    int boarding_no_int;

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
    /* Allocamos la primera query de todas */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt_select);
    ret = SQLPrepare(stmt_select, (SQLCHAR *)query, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLPrepare", stmt_select, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al preparar la consulta principal.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* Unimos el prepared statement a la book_ref escrita por pantalla */
    SQLBindParameter(stmt_select, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, bookID, 0, NULL);

    ret = SQLExecute(stmt_select);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLExecute", stmt_select, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al ejecutar la consulta principal.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* Unimos cada columna a su correspondiente char */
    SQLBindCol(stmt_select, 1, SQL_C_CHAR, nombre, sizeof(nombre), NULL);
    SQLBindCol(stmt_select, 2, SQL_C_CHAR, flight_id, sizeof(flight_id), NULL);
    SQLBindCol(stmt_select, 3, SQL_C_CHAR, departure, sizeof(departure), NULL);
    SQLBindCol(stmt_select, 4, SQL_C_CHAR, ticket_no, sizeof(ticket_no), NULL);

    /* Si no hay ningun dato es porque o ya se han añadido o porque nunca faltó la tarjeta de embarque*/
    ret = SQLFetch(stmt_select);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLExecute", stmt_select, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Book ref no válido");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    if (ret == SQL_NO_DATA) {
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "No hay resultados para los datos seleccionados.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* Preparamos la siguiente query, la del boardin_no */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt_boarding);
    ret = SQLPrepare(stmt_boarding, (SQLCHAR *)query_boarding_no, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLPrepare", stmt_boarding, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al preparar la consulta de boarding_no.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* Preparamos la tercera query, la del seat_no */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt_seat);
    ret = SQLPrepare(stmt_seat, (SQLCHAR *)query_seat_no, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLPrepare", stmt_seat, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al preparar la consulta de seat_no.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }

    /* Preparamos la query que inserta los datos */
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt_insert);
    ret = SQLPrepare(stmt_insert, (SQLCHAR *)query_insert, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        odbc_extract_error("SQLPrepare", stmt_insert, SQL_HANDLE_STMT);
        snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al preparar la consulta de insert.");
        t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
        strncpy((*choices_msg)[0], query_result_set[0], t);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);
        odbc_disconnect(env, dbc);
        *n_choices = 0;
        return;
    }
    
    /* Añadimos la cabera con el nombre de las columnas */
    snprintf(query_result_set[0], sizeof(query_result_set[0]), "%-20s | %-6s | %-10s | %-13s", "Nombre pasajero", "ID", "Salida", "Asiento");
    n_rows = 1;
    i = 1;

    /* Comenzamos el bucle para ir sacando datos y copiandolos */
    do {
        /* Une el prepared statement a un char */
        SQLBindParameter(stmt_boarding, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, flight_id, 0, NULL);
        ret = SQLExecute(stmt_boarding);
        if (!SQL_SUCCEEDED(ret)) {
            snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al obtener boarding_no para vuelo %s", flight_id);
            t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
            strncpy((*choices_msg)[0], query_result_set[0], t);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);
            odbc_disconnect(env, dbc);
            *n_choices = 0;
            return;
        }

        /* Unimos la columna a un char */
        SQLBindCol(stmt_boarding, 1, SQL_C_CHAR, boarding_no, sizeof(boarding_no), NULL);

        /* Cogemos el boarding_no */
        ret = SQLFetch(stmt_boarding);

        /* Como la consulta devuelve el mayor ya usado, sumamos 1 
           para conseguir un nuevo boardin_no para cada vuelo */
        boarding_no_int = atoi((char*)boarding_no) + 1;

        /* Une el prepared statement a un char */
        SQLBindParameter(stmt_seat, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, flight_id, 0, NULL);
        ret = SQLExecute(stmt_seat);
        if (!SQL_SUCCEEDED(ret)) {
            snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al obtener asiento para vuelo %s", flight_id);
            t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
            strncpy((*choices_msg)[0], query_result_set[0], t);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);
            odbc_disconnect(env, dbc);
            *n_choices = 0;
            return;
        }

        /* Une la columna a un char */
        SQLBindCol(stmt_seat, 1, SQL_C_CHAR, seat_no, sizeof(seat_no), NULL);

        /* Cogemos el asiento libre */
        ret = SQLFetch(stmt_seat);
        if (ret == SQL_NO_DATA) {
            snprintf(query_result_set[0], sizeof(query_result_set[0]), "No hay asientos disponibles para el vuelo %s", flight_id);
            t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
            strncpy((*choices_msg)[0], query_result_set[0], t);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);
            odbc_disconnect(env, dbc);
            *n_choices = 0;
            return; return;
        }

        /* Pasamos el boarding_no a un char */
        snprintf((char*)boarding_no, sizeof(boarding_no), "%d", boarding_no_int);
        
        /* Unimos cada prepared statement a un char */
        SQLBindParameter(stmt_insert, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, ticket_no, 0, NULL);
        SQLBindParameter(stmt_insert, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, flight_id, 0, NULL);
        SQLBindParameter(stmt_insert, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, boarding_no, 0, NULL);
        SQLBindParameter(stmt_insert, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, seat_no, 0, NULL);

        /* Ejecutamos la consulta de insertar */
        ret = SQLExecute(stmt_insert);
        if (!SQL_SUCCEEDED(ret)) {
            snprintf(query_result_set[0], sizeof(query_result_set[0]), "Error al insertar tarjeta de embarque para vuelo %s", flight_id);
            t = MIN((int)strlen(query_result_set[0]) + 1, max_length);
            strncpy((*choices_msg)[0], query_result_set[0], t);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);
            odbc_disconnect(env, dbc);
            *n_choices = 0;
            return;
        }
        /* Cerramos el cursor asociado a cada statement
           asi podremos volver a utilizar cada statement sin problema */
        SQLCloseCursor(stmt_boarding);
        SQLCloseCursor(stmt_seat);
        SQLCloseCursor(stmt_insert);

        /* Pegamos los datos obtenidos en el array */
        snprintf(query_result_set[i], sizeof(query_result_set[0]),
                 "%-20.20s | %-6s | %-10.10s | %-13s",
                 nombre, flight_id, departure, seat_no);

        i++;
        n_rows++;
    } while (SQL_SUCCEEDED(ret = SQLFetch(stmt_select)) && i < 1024);

    /* El numero de resultados son las filas obtenidas*/
    *n_choices = n_rows;

    /* Pegamos la informacion obtenida en choices y choices_msg */
    for (i = 0; i < *n_choices; i++) {
        t = MIN((int)strlen(query_result_set[i]) + 1, max_length);
        strncpy((*choices_msg)[i], query_result_set[i], t);
        strncpy((*choices)[i], query_result_set[i], t);
    }
    
    /* free up statement handle */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt_select);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt_boarding);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt_seat);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt_insert);

    /* DISCONNECT */
    ret = odbc_disconnect(env, dbc);
    if (!SQL_SUCCEEDED(ret)) {
        return;
    }
    return;
}

