//
// Created by jon on 12/2/23.
//

#include <math.h>
#include "aqi.h"
#include "stdio.h"
#define DBG printf


static float api_pm25(float x) {
    if(x <= 30.0f) {
        return x * 50.0f / 30.0f;
    }else if(x <= 60.0f) {
        return 50.0f + (x - 30.0f) * 50.0f / 30.0f;
    }else if(x <= 90.0f){
        return 100.0f + (x - 60.0f) * 100.0f / 30.0f;
    }else if(x <= 120.0f) {
        return 200.0f + (x - 90.0f) * 100.0f / 30.0f;
    }else if(x <= 250.0f) {
        return 300 + (x - 120.0f) * 100.0f / 13.0f;
    }else if(x > 250) {
        return 400.0f + (x - 250.0f) * 100.0f / 130.0f;
    }else{
        return 0; // unreachable if x is not NaN
    }
}

static float aqi_pm100(float x) {
    if(x <= 100.0f) {
        return x;
    }else if(x <= 250.0f){
        return 100.0f + (x - 100.0f) * 100.0f / 150.0f;
    }else if(x <= 350.0f) {
        return x - 250.0f;
    }else if(x <= 430.0f) {
        return 300.0f + (x - 350.0f) * 100.0f / 80.0f;
    }else if(x > 430) {
        return 400 + (x - 430.0f) * 100.0f / 80.0f;
    }else{
        return 0;
    }
}

float aqi_from_pm25_100(float pm25, float pm100) {
    float aqi_25 = api_pm25(pm25);
    float aqi_100 = aqi_pm100(pm100);
    float aqi = fmaxf(aqi_25, aqi_100);
    DBG("pm25=%f,pm100=%f,aqi25=%f,aqi100=%f,aqi=%f", pm25, pm100, aqi_25, aqi_100, aqi);
    return aqi;
}