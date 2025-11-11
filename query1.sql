WITH trasbordos AS (
	SELECT DISTINCT f1.flight_id AS primer_id, f2.flight_id AS segundo_id, f1.scheduled_departure AS primer_departure, f2.scheduled_departure AS segundo_departure,
	       f1.scheduled_arrival AS primer_arrival, f2.scheduled_arrival AS segundo_arrival, 
           1 AS conexion, 
		   LEAST(
            (SELECT COUNT(*) 
             FROM seats s1
             LEFT JOIN boarding_passes b1 
                ON b1.flight_id = f1.flight_id AND b1.seat_no = s1.seat_no
             WHERE s1.aircraft_code = f1.aircraft_code AND b1.seat_no IS NULL),
            (SELECT COUNT(*) 
             FROM seats s2
             LEFT JOIN boarding_passes b2 
                ON b2.flight_id = f2.flight_id AND b2.seat_no = s2.seat_no
             WHERE s2.aircraft_code = f2.aircraft_code AND b2.seat_no IS NULL)
           ) AS asientos_vacios,
		   f1.aircraft_code AS primer_code, f2.aircraft_code AS segundo_code
	FROM flights f1
	JOIN flights f2 ON f1.arrival_airport = f2.departure_airport
	WHERE (f2.scheduled_arrival - f1.scheduled_departure) > INTERVAL '0 seconds'
    AND (f2.scheduled_arrival - f1.scheduled_departure) <= INTERVAL '1 day' 
	AND f1.scheduled_arrival < f2.scheduled_departure
	AND f1.departure_airport = 'SVO' AND f2.arrival_airport = 'LED'
	AND DATE(f1.scheduled_arrival) = '2017-08-26'
	AND f1.status IN ('On Time', 'Delayed', 'Scheduled') 
	AND f2.status IN ('On Time', 'Delayed', 'Scheduled') 
	GROUP BY f1.flight_id, f2.flight_id
), directos AS (
	SELECT flights.flight_id AS primer_id, flights.flight_id AS segundo_id, flights.scheduled_departure AS primer_departure, 
	       flights.scheduled_departure AS segundo_departure, flights.scheduled_arrival AS primer_arrival, flights.scheduled_arrival AS segundo_arrival, 
           0 AS conexion, 
		   COUNT(seats.seat_no) FILTER (WHERE boarding_passes.seat_no IS NULL) AS asientos_vacios,
		   flights.aircraft_code AS primer_code, flights.aircraft_code AS segundo_code
	FROM flights
	JOIN seats ON flights.aircraft_code = seats.aircraft_code
	LEFT JOIN boarding_passes ON (flights.flight_id = boarding_passes.flight_id
                                  AND seats.seat_no = boarding_passes.seat_no 
								  AND flights.status IN ('On Time', 'Delayed', 'Scheduled') )
	WHERE flights.departure_airport = 'SVO' AND flights.arrival_airport = 'LED'
	AND DATE(flights.scheduled_arrival) = '2017-08-26'
	AND (flights.scheduled_arrival - flights.scheduled_departure) > INTERVAL '0 seconds'
    AND (flights.scheduled_arrival - flights.scheduled_departure) <= INTERVAL '1 day' 
	AND flights.status IN ('On Time', 'Delayed', 'Scheduled') 
	GROUP BY flights.flight_id
), total_vuelos AS (
	SELECT trasbordos.primer_id, trasbordos.segundo_id, trasbordos.primer_departure, trasbordos.segundo_departure,
	       trasbordos.primer_arrival, trasbordos.segundo_arrival, trasbordos.asientos_vacios, trasbordos.conexion, trasbordos.primer_code, trasbordos.segundo_code
	FROM trasbordos
	UNION ALL
	SELECT directos.primer_id, directos.segundo_id, directos.primer_departure, directos.segundo_departure,
	       directos.primer_arrival, directos.segundo_arrival, directos.asientos_vacios, directos.conexion, 
		   directos.primer_code, directos.segundo_code
	FROM directos
)
SELECT total_vuelos.primer_id, total_vuelos.primer_departure, total_vuelos.segundo_arrival, total_vuelos.asientos_vacios, total_vuelos.conexion
FROM total_vuelos
WHERE total_vuelos.asientos_vacios > 0
ORDER BY (total_vuelos.segundo_arrival - total_vuelos.primer_departure) 