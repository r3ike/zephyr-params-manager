#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "param_manager.hpp"
#include "mavlink_handler.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main() {
    LOG_INF("Avvio sistema parametri...");
    autopilot::params::init();
    mavlink_task_init();
    
    while (true) {
        // Esempio di lettura parametri
        //autopilot::params::ParamFloat roll_p(autopilot::params::ID::ROLL_P);
        //autopilot::params::ParamBool ahrs(autopilot::params::ID::AHRS_ENABLE);
        //LOG_INF("ROLL_P = %f, AHRS = %d", roll_p.get(), ahrs.get());
        k_sleep(K_SECONDS(5));
    }
    return 0;
}