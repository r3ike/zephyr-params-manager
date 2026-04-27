// param_manager.cpp
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(params_manager, LOG_LEVEL_DBG);
#include "param_manager.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include <cstring>

namespace autopilot::params {

// ------------------------------------------------------------
// DEFINIZIONE DELLA CACHE
// ------------------------------------------------------------
// Allochiamo staticamente un buffer di byte sufficiente a contenere
// il valore di TUTTI i parametri. Usiamo uint64_t per garantire
// allineamento per qualsiasi tipo (float, int32_t, bool).
// L'attributo __aligned(4) assicura che l'accesso a 32 bit sia efficiente.
static uint8_t __aligned(4) param_cache_storage[
    static_cast<uint16_t>(ID::COUNT) * sizeof(uint64_t)
];

// Array di puntatori alla cache. Ogni elemento punta all'inizio
// della porzione di buffer riservata a quel parametro.
void* g_param_cache[static_cast<uint16_t>(ID::COUNT)];

// ------------------------------------------------------------
// FUNZIONE DI INIZIALIZZAZIONE CACHE CON VALORI DI DEFAULT
// ------------------------------------------------------------
static void init_cache_with_defaults() {
    for (uint16_t i = 0; i < static_cast<uint16_t>(ID::COUNT); ++i) {
        const Metadata& meta = g_param_metadata[i];
        // Calcola l'indirizzo nel buffer per questo parametro
        void* ptr = &param_cache_storage[i * sizeof(uint64_t)];
        g_param_cache[i] = ptr;
        
        // In base al tipo, copia il valore di default
        switch (meta.type) {
            case Metadata::INT32:
                *static_cast<int32_t*>(ptr) = meta.i32_default;
                break;
            case Metadata::FLOAT:
                *static_cast<float*>(ptr) = meta.f_default;
                break;
            case Metadata::BOOL:
                *static_cast<bool*>(ptr) = meta.b_default;
                break;
        }
    }
}

// ------------------------------------------------------------
// CALLBACK DEL SETTINGS SUBSYSTEM
// ------------------------------------------------------------
// Questa funzione viene chiamata da Zephyr quando deve caricare
// un valore dalla Flash. Viene invocata da settings_load() o
// settings_load_subtree().
static int settings_handler(const char *key, size_t len,
                            settings_read_cb read_cb, void *cb_arg)
{
    // La chiave è nel formato "params/NOME_PARAMETRO"
    // Saltiamo i primi 7 caratteri ("params/") per ottenere il nome
    LOG_INF("Caricamento chiave: %s", key);
    //const char *name = key + 7;

    //printk("Settings handler: key=%s, name=%s\n", key, name);
    
    // Cerchiamo il parametro per nome nella tabella metadati
    for (uint16_t i = 0; i < static_cast<uint16_t>(ID::COUNT); ++i) {
        if (strcmp(key, g_param_metadata[i].name) == 0) {
            // Trovato! Otteniamo il puntatore alla cache
            void* cache_ptr = g_param_cache[i];
            
            // Determiniamo la dimensione attesa in base al tipo
            size_t expected_size = 0;
            switch (g_param_metadata[i].type) {
                case Metadata::INT32: expected_size = sizeof(int32_t); break;
                case Metadata::FLOAT: expected_size = sizeof(float);   break;
                case Metadata::BOOL:  expected_size = sizeof(bool);    break;
            }
            
            // Leggiamo il valore dalla Flash usando il callback fornito da Zephyr
            ssize_t ret = read_cb(cb_arg, cache_ptr, expected_size);
            
            // Se la lettura ha successo, restituiamo 0, altrimenti errore
            return (ret == expected_size) ? 0 : -EINVAL;
        }
    }
    // Parametro non trovato
    return -ENOENT;
}

// ------------------------------------------------------------
// REGISTRAZIONE DELL'HANDLER CON ZEPHYR
// ------------------------------------------------------------
// Macro che definisce un handler statico per il sottosistema "params".
// Il primo argomento è il nome dell'handler (arbitrario).
// Il secondo è il prefisso delle chiavi che gestiamo ("params").
// Il terzo (NULL) è una callback per il commit (non ci serve).
// Il quarto è la nostra callback di lettura.
// Gli ultimi due sono per il supporto alla registrazione dinamica (non usati).
SETTINGS_STATIC_HANDLER_DEFINE(params, "params", NULL, settings_handler, NULL, NULL);

// ------------------------------------------------------------
// FUNZIONE DI INIZIALIZZAZIONE GLOBALE
// ------------------------------------------------------------
void init() {
    // 1. Inizializza la cache RAM con i valori di default presi dai metadati
    init_cache_with_defaults();
    
    // 2. Inizializza il sottosistema Settings di Zephyr
    //    (se non già fatto altrove, ma chiamarlo più volte è innocuo)
    settings_subsys_init();
    
    // 3. Carica tutti i valori salvati in Flash.
    //    Per ogni chiave "params/*" trovata, verrà chiamata la nostra
    //    settings_handler, che sovrascriverà il valore di default nella cache.
    settings_load();
}

} // namespace autopilot::params