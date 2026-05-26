#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <mutex>
#include "httplib.h"
#include "json.hpp"   
#include "viajes.h"

using namespace std;
using namespace httplib;

vector<Viaje> viajes_db;
int proximo_id = 1;
mutex db_mutex;
const string archivo_datos = "viajes.dat";

void guardar_viajes() {
    json j_array = json::array();
    for (const auto& v : viajes_db) {
        j_array.push_back(v);
    }

    ofstream archivo(archivo_datos);
    if (archivo.is_open()) {
        archivo << j_array.dump(4);
        archivo.close();
    }
    else {
        cerr << "Error al abrir " << archivo_datos << " para escritura." << endl;
    }
}

// Función para cargar los viajes desde el archivo al iniciar el servidor.
void cargar_viajes() {
    lock_guard<mutex> lock(db_mutex); // Bloquea el mutex para esta operación

    ifstream archivo(archivo_datos);
    if (archivo.is_open()) {
        try {
            json j_array_leido;
            archivo >> j_array_leido;
            if (j_array_leido.is_array()) {
                for (const auto& j_viaje : j_array_leido) {
                    // Convierte el objeto JSON a un objeto viaje usando from_json
                    Viaje v = j_viaje.get<Viaje>();
                    viajes_db.push_back(v);
                    // Actualiza proximo_id para asegurar IDs únicos
                    if (v.id >= proximo_id) {
                        proximo_id = v.id + 1;
                    }
                }
            }
            cout << "Viajes cargados desde " << archivo_datos << endl;
        }
        catch (json::parse_error& e) {
            cerr << "Error al parsear JSON desde " << archivo_datos << ": " << e.what() << endl;
        }
        catch (json::type_error& e) {
            cerr << "Error de tipo JSON desde " << archivo_datos << ": " << e.what() << endl;
        }
        archivo.close();
    }
    else {
        cout << "No se encontró " << archivo_datos << ". Se iniciará con una base de datos vacía." << endl;
    }

    // Asegura que proximo_id sea al menos 1, y mayor que cualquier ID existente.
    if (viajes_db.empty() && proximo_id < 1) {
        proximo_id = 1;
    }
    else if (!viajes_db.empty()) {
        int max_id = 0;
        for (const auto& v : viajes_db) {
            if (v.id > max_id) max_id = v.id;
        }
        if (proximo_id <= max_id) {
            proximo_id = max_id + 1;
        }
    }
}


int main(void) {
    Server svr;

    cargar_viajes();

    // Endpoint POST: Crear un nuevo viaje
    svr.Post("/viaje", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            json j_body = json::parse(req.body);
            Viaje v_nuevo;

            // Validación básica de campos obligatorios
            if (!j_body.contains("nombre") || !j_body.contains("email") || !j_body.contains("origen") || !j_body.contains("destino") || !j_body.contains("fecha") || !j_body.contains("pasajeros")) {
                res.status = 400; // Bad Request
                res.set_content("Faltan campos obligatorios: nombre, email, origen, destino, fecha, pasajeros", "text/plain; charset=utf-8");
                return;
            }
            from_json(j_body, v_nuevo); // Usa la función from_json de viaje

            lock_guard<mutex> lock(db_mutex); // Protege el acceso a la BD
            v_nuevo.id = proximo_id++; // Asigna un nuevo ID
            viajes_db.push_back(v_nuevo);
            guardar_viajes(); // Guarda la BD actualizada en el archivo

            json j_respuesta = v_nuevo; // Convierte el nuevo viaje a JSON para la respuesta
            res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            res.status = 201; // 201 Created
        }
        catch (json::parse_error& e) {
            res.status = 400;
            res.set_content("JSON mal formado: " + string(e.what()), "text/plain; charset=utf-8");
        }
        catch (json::type_error& e) {
            res.status = 400;
            res.set_content("Error en el tipo de datos del JSON o campo faltante: " + string(e.what()), "text/plain; charset=utf-8");
        }
        catch (const exception& e) {
            res.status = 500; // Internal Server Error
            res.set_content("Error interno del servidor: " + string(e.what()), "text/plain; charset=utf-8");
        }
        });

    // Endpoint GET: Obtener todos los viajes
    svr.Get("/viajes", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        lock_guard<mutex> lock(db_mutex);
        json j_array_respuesta = viajes_db;
        res.set_content(j_array_respuesta.dump(4), "application/json; charset=utf-8");
        });

    // Endpoint GET: Obtener un viaje específico por ID
    svr.Get(R"(/viaje/(\d+))", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        // El (\d+) en la ruta es una expresión regular que captura el ID numérico
        int id_viaje = stoi(req.matches[1].str()); // req.matches[1] contiene el ID capturado

        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(viajes_db.begin(), viajes_db.end(),
            [id_viaje](const Viaje& v) { return v.id == id_viaje; });

        if (it != viajes_db.end()) { // Si se encontró el viaje
            json j_viaje_respuesta = *it;
            res.set_content(j_viaje_respuesta.dump(4), "application/json; charset=utf-8");
        }
        else {
            res.status = 404; // Not Found
            res.set_content("viaje no encontrado", "text/plain; charset=utf-8");
        }
        });

    // Endpoint PUT: Actualizar un viaje existente por ID
    svr.Put(R"(/viaje/(\d+))", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        int id_viaje = stoi(req.matches[1].str());

        try {
            json j_actualizacion = json::parse(req.body);

            lock_guard<mutex> lock(db_mutex);
            auto it = find_if(viajes_db.begin(), viajes_db.end(),
                [id_viaje](const Viaje& v) { return v.id == id_viaje; });

            if (it != viajes_db.end()) {
                // Actualiza solo los campos presentes en el JSON de la petición
                if (j_actualizacion.contains("nombre")) it->nombre = j_actualizacion["nombre"].get<string>();
                if (j_actualizacion.contains("email")) it->email = j_actualizacion["email"].get<string>();
                if (j_actualizacion.contains("origen")) it->origen = j_actualizacion["origen"].get<string>();
                if (j_actualizacion.contains("destino")) it->destino = j_actualizacion["destino"].get<string>();
                if (j_actualizacion.contains("fecha")) it->fecha = j_actualizacion["fecha"].get<string>();
                if (j_actualizacion.contains("pasajeros")) it->pasajeros = j_actualizacion["pasajeros"].get<int>();

                guardar_viajes(); // Guarda la BD actualizada en el archivo
                json j_respuesta = *it; // Devuelve el viaje actualizado
                res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            }
            else {
                res.status = 404;
                res.set_content("Datos del viaje no encontrados", "text/plain; charset=utf-8");
            }
        }
        catch (json::parse_error& e) {
            res.status = 400;
            res.set_content("JSON mal formado: " + string(e.what()), "text/plain; charset=utf-8");
        }
        catch (json::type_error& e) {
            res.status = 400;
            res.set_content("Error en el tipo de datos del JSON o campo faltante: " + string(e.what()), "text/plain; charset=utf-8");
        }
        catch (const exception& e) {
            res.status = 500;
            res.set_content("Error interno del servidor: " + string(e.what()), "text/plain; charset=utf-8");
        }
        });

    // Endpoint DELETE: Eliminar un viaje por ID
    svr.Delete(R"(/viaje/(\d+))", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        int id_viaje = stoi(req.matches[1].str());

        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(viajes_db.begin(), viajes_db.end(),
            [id_viaje](const Viaje& v) { return v.id == id_viaje; });

        if (it != viajes_db.end()) {
            viajes_db.erase(it); // Elimina el viaje del vector
            guardar_viajes(); // Guarda la BD actualizada en el archivo
            res.status = 204; // No Content (exito, sin cuerpo de respuesta)
        }
        else {
            res.status = 404;
            res.set_content("Viaje no encontrado o no agendado", "text/plain; charset=utf-8");
        }
        });

    svr.Get("/", [](const Request& req, Response& res) {
        ifstream file("index.html");
        if (file.is_open()) {
            string content((istreambuf_iterator<char>(file)),
                istreambuf_iterator<char>());
            res.set_content(content, "text/html; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("index.html no encontrado", "text/plain");
        }
        });

    svr.Options(R"(.*)", [](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204; // No Content
        });

    cout << "Servidor CRUD de viajes iniciando en http://localhost:3000" << endl;
    svr.listen("0.0.0.0", 3000); // Inicia el servidor para escuchar en el puerto 3000 en todas las interfaces

    return 0;
}