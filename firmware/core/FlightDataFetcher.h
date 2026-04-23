#pragma once

#include <Arduino.h>
#include <vector>
#include "interfaces/BaseStateVectorFetcher.h"
#include "interfaces/BaseFlightFetcher.h"
#include "models/StateVector.h"
#include "models/FlightInfo.h"

class FlightDataFetcher
{
public:
    FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                      BaseFlightFetcher *flightFetcher);

    bool fetchFlights(std::vector<StateVector> &outStates,
                      std::vector<FlightInfo> &outFlights,
                      size_t &outEnrichedCount);

private:
    BaseStateVectorFetcher *_stateFetcher;
    BaseFlightFetcher *_flightFetcher;
};
