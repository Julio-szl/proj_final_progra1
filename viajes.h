#ifndef VIAJES_H
#define VIAJES_H
#include <string>
#include "json.hpp" 

using namespace std;
using json = nlohmann::ordered_json;

struct Viaje {
    int id;
    string origen;
    string destino;
    string fecha;
    int capacidad;

    // Amigas para la serialización/deserialización con nlohmann/json
    friend void to_json(json& j, const Viaje& v) {
        j = json{
            {"id", v.id},
            {"origen", v.origen},
            {"destino", v.destino},
            {"fecha", v.fecha},
            {"capacidad", v.capacidad}
        };
    }

    friend void from_json(const json& j, Viaje& v) {
        // El ID se asigna al crear o se lee del archivo,
        // por lo que solo lo leemos si está presente (útil para cargar desde archivo)
        if (j.contains("id")) {
            j.at("id").get_to(v.id);
        }

        // Para los campos que vienen del cliente, es bueno verificar si existen
        // antes de intentar accederlos para evitar excepciones si faltan.
        if (j.contains("origen")) j.at("origen").get_to(v.origen);
        if (j.contains("destino")) j.at("destino").get_to(v.destino);
        if (j.contains("fecha")) j.at("fecha").get_to(v.fecha);
        if (j.contains("capacidad")) j.at("capacidad").get_to(v.capacidad);
    }
};

#endif 
#pragma once