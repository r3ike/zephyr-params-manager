// param.hpp
#pragma once
#include <cstdio>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include "param_defs.hpp"

namespace autopilot::params {



// ------------------------------------------------------------
// DICHIARAZIONI ESTERNE (definite in param_manager.cpp)
// ------------------------------------------------------------
// Funzione helper: dato un ID restituisce il nome stringa.
// Implementata semplicemente come:
//   return g_param_metadata[static_cast<uint16_t>(id)].name;
inline const char* id_to_key(ID id) {
    return g_param_metadata[static_cast<uint16_t>(id)].name;
}

// Cache globale: un array di puntatori void*.
// Ogni puntatore punta a una locazione di RAM dove è memorizzato
// il valore corrente del parametro.
// La cache è indicizzata tramite cast dell'ID a uint16_t.
extern void* g_param_cache[];

// Aggiungere in param.hpp prima di template<typename T> class Param
class ParamBase {
public:
    virtual int load() = 0;
    virtual ~ParamBase() = default;
};

void init();


// ------------------------------------------------------------
// CLASSE TEMPLATE Param<T>
// ------------------------------------------------------------
template<typename T>
class Param : public ParamBase{
public:
    // Costruttore: riceve l'ID del parametro.
    // Chiama subito load() per assicurarsi che il valore sia caricato
    // dalla Flash (o impostato al default) prima di qualsiasi utilizzo.
    explicit Param(ID id) : _id(id) {
        load();
    }

    // METODO get(): restituisce una copia del valore corrente.
    // Legge direttamente dalla cache RAM (accesso veloce).
    T get() const {
        // Recupera il puntatore dalla cache globale usando l'ID come indice.
        // Il cast a T* è sicuro perché il tipo T è garantito dal template.
        return *static_cast<T*>(g_param_cache[static_cast<uint16_t>(_id)]);
    }

    // METODO set(): scrive un nuovo valore.
    // 1. Aggiorna immediatamente la cache RAM.
    // 2. Salva il valore nella memoria non volatile (Flash) tramite Zephyr Settings.
    int set(const T& value) {
        // Ottiene il puntatore alla locazione di cache
        T* cache_ptr = static_cast<T*>(g_param_cache[static_cast<uint16_t>(_id)]);
        // Scrive il nuovo valore in RAM
        *cache_ptr = value;
        char full_key[64];
        snprintf(full_key, sizeof(full_key), "params/%s", id_to_key(_id));
        // Salva in Flash usando come chiave il nome del parametro
        // Il valore viene scritto così com'è in binario (sizeof(T) byte)
        int ret = settings_save_one(full_key, cache_ptr, sizeof(T));
        
        return ret;
    }

    // METODO load(): forza la ricarica del valore dalla Flash.
    // Chiama settings_load_subtree() che attiva la callback del
    // Settings Subsystem per il nostro parametro specifico.
    int load() override{
        // Carica solo il sottoalbero relativo a questo parametro.
        // La chiave passata è il nome del parametro (es. "ROLL_P").
        return settings_load_subtree(id_to_key(_id));
    }

    // OPERATORE DI CAST IMPLICITO a T
    // Permette di usare un oggetto Param<T> come se fosse una variabile di tipo T.
    // Esempio: float gain = _roll_p;  // chiama automaticamente get()
    operator T() const { return get(); }

    // OPERATORE DI ASSEGNAMENTO
    // Permette di assegnare un valore direttamente all'oggetto Param.
    // Esempio: _roll_p = 2.5f;  // chiama automaticamente set()
    Param& operator=(const T& value) {
        set(value);
        return *this;
    }

private:
    ID _id;   // Identificativo del parametro
};

// ALIAS per una sintassi più familiare (simile a PX4)
using ParamInt   = Param<int32_t>;
using ParamFloat = Param<float>;
using ParamBool  = Param<bool>;

} // namespace autopilot::params