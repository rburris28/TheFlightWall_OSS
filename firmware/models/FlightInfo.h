#pragma once

#include <Arduino.h>
#include <vector>
#include "AirportInfo.h"

struct FlightInfo
{
    // Flight identifiers
    String ident;
    String ident_icao;
    String ident_iata;

    // Operator
    String operator_code;
    String operator_icao;
    String operator_iata;

    // Route
    AirportInfo origin;
    AirportInfo destination;

    // Aircraft
    String aircraft_code;

    // Live position from the state vector used to enrich this flight
    bool has_live_position = false;
    double latitude = NAN;
    double longitude = NAN;
    double distance_km = NAN;
    double bearing_deg = NAN;
    double heading_deg = NAN;

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;
};
