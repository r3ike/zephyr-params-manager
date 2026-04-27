// module_params.hpp
#pragma once
#include "param_manager.hpp"
#include <vector>

namespace autopilot {

// Classe base astratta per tutti i moduli che hanno parametri
class ModuleParams {
public:
    // Distruttore virtuale (buona pratica per classi base)
    virtual ~ModuleParams() = default;

    // Metodo virtuale che può essere chiamato periodicamente (es. nel loop principale)
    // per aggiornare tutti i parametri registrati dal modulo.
    // Di default, chiama load() su ogni parametro nella lista.
    virtual void updateParams() {
        for (auto param : _param_list) {
            if (param) {
                param->load();
            }
        }
    }

protected:
    // Metodo per registrare un parametro.
    // Deve essere chiamato dai costruttori delle classi derivate.
    void registerParam(ParamBase* param) {
        if (param) {
            _param_list.push_back(param);
        }
    }

private:
    // Lista di puntatori ai parametri appartenenti a questo modulo.
    // Usiamo std::vector per semplicità; in produzione potremmo usare
    // un array statico per evitare allocazioni dinamiche.
    std::vector<ParamBase*> _param_list;
};

} // namespace autopilot