#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mavlink, LOG_LEVEL_DBG);

#include "mavlink_handler.hpp"
#include "param_manager.hpp"
#include <cstring>
#include <unistd.h>       // close()
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mavlink/common/mavlink.h>


static int sock;

// Invia un messaggio MAVLink a un destinatario specifico
static void send_mavlink_message_to(const mavlink_message_t& msg, struct sockaddr_in* dest) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    int len = mavlink_msg_to_send_buffer(buffer, &msg);
    if (len > 0) {
        sendto(sock, buffer, len, 0, (struct sockaddr*)dest, sizeof(*dest));
    }
}

// Risponde alla richiesta della lista parametri
static void handle_param_request_list(struct sockaddr_in* client) {
    using namespace autopilot::params;
    for (uint16_t i = 0; i < static_cast<uint16_t>(ID::COUNT); ++i) {
        const Metadata& meta = g_param_metadata[i];
        mavlink_message_t msg;
        float value = 0;
        switch (meta.type) {
            case Metadata::INT32: value = static_cast<float>(*static_cast<int32_t*>(g_param_cache[i])); break;
            case Metadata::FLOAT: value = *static_cast<float*>(g_param_cache[i]); break;
            case Metadata::BOOL:  value = *static_cast<bool*>(g_param_cache[i]) ? 1.0f : 0.0f; break;
        }
        mavlink_msg_param_value_pack(
            1, MAV_COMP_ID_AUTOPILOT1, &msg,
            meta.name, value,
            (meta.type == Metadata::INT32) ? MAV_PARAM_TYPE_INT32 :
            (meta.type == Metadata::FLOAT) ? MAV_PARAM_TYPE_REAL32 :
            MAV_PARAM_TYPE_UINT8,
            static_cast<uint16_t>(ID::COUNT), i);
        send_mavlink_message_to(msg, client);
    }
}

// Gestisce la modifica di un parametro
static void handle_param_set(const mavlink_param_set_t& set, struct sockaddr_in* client) {
    using namespace autopilot::params;
    for (uint16_t i = 0; i < static_cast<uint16_t>(ID::COUNT); ++i) {
        if (strcmp(set.param_id, g_param_metadata[i].name) == 0) {
            bool ok = false;
            switch (g_param_metadata[i].type) {
                case Metadata::FLOAT: {
                    ParamFloat p(static_cast<ID>(i));
                    int ret = p.set(set.param_value);
                    if (ret) {
                        LOG_ERR("Salvataggio fallito (err %d)", ret);
                    }
                    ok = true;
                    break;
                }
                case Metadata::INT32: {
                    ParamInt p(static_cast<ID>(i));
                    p.set(static_cast<int32_t>(set.param_value));
                    ok = true;
                    break;
                }
                case Metadata::BOOL: {
                    ParamBool p(static_cast<ID>(i));
                    p.set(set.param_value != 0.0f);
                    ok = true;
                    break;
                }
            }
            if (ok) {
                mavlink_message_t msg;
                mavlink_msg_param_value_pack(
                    1, MAV_COMP_ID_AUTOPILOT1, &msg,
                    set.param_id, set.param_value,
                    set.param_type, 0, i);
                send_mavlink_message_to(msg, client);
                LOG_INF("Parametro %s impostato a %f", set.param_id, (double)set.param_value);
            }
            return;
        }
    }
}

// Invia un heartbeat a un destinatario specifico
static void send_heartbeat_to(struct sockaddr_in* dest) {
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &msg,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_GENERIC,
        MAV_MODE_FLAG_MANUAL_INPUT_ENABLED,
        0,
        MAV_STATE_ACTIVE);
    send_mavlink_message_to(msg, dest);
}

static void send_autopilot_version(struct sockaddr_in* dest) {
    mavlink_message_t msg;
    mavlink_msg_autopilot_version_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &msg,
        MAV_AUTOPILOT_GENERIC,        // autopilot type
        0, 0, 0, 0,                     // versioni varie
        0, 0, 0, 0, 0, 0, 0);          // altre info
    send_mavlink_message_to(msg, dest);
}

static void send_sys_status_to(struct sockaddr_in* dest) {
    mavlink_message_t msg;

    // --- Sensori di base (come prima) ---
    uint32_t onboard_control_sensors_present =
        MAV_SYS_STATUS_SENSOR_3D_GYRO |
        MAV_SYS_STATUS_SENSOR_3D_ACCEL |
        MAV_SYS_STATUS_SENSOR_3D_MAG |
        MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE |
        MAV_SYS_STATUS_SENSOR_GPS;

    uint32_t onboard_control_sensors_enabled = onboard_control_sensors_present;
    uint32_t onboard_control_sensors_health = onboard_control_sensors_present; // tutti OK

    // --- Sensori estesi (MAVLink 2) ---
    // Se non hai sensori aggiuntivi (es. lidar, flusso ottico), metti 0.
    uint32_t onboard_control_sensors_present_extended = 0;
    uint32_t onboard_control_sensors_enabled_extended = 0;
    uint32_t onboard_control_sensors_health_extended = 0;

    // --- Altri parametri ---
    uint16_t load = 0;              // carico CPU in centesimi (0 = non disponibile)
    uint16_t voltage_battery = 0;   // mV (0 = non disponibile)
    int16_t  current_battery = -1;  // mA (-1 = non disponibile)
    int8_t   battery_remaining = -1;// percentuale (-1 = non disponibile)

    uint16_t drop_rate_comm = 0;
    uint16_t errors_comm = 0;
    uint16_t errors_count1 = 0;
    uint16_t errors_count2 = 0;
    uint16_t errors_count3 = 0;
    uint16_t errors_count4 = 0;

    // --- Pacchettizzazione con TUTTI i campi ---
    mavlink_msg_sys_status_pack(
        1,                          // system_id
        MAV_COMP_ID_AUTOPILOT1,    // component_id
        &msg,
        onboard_control_sensors_present,
        onboard_control_sensors_enabled,
        onboard_control_sensors_health,
        load,
        voltage_battery,
        current_battery,
        battery_remaining,
        drop_rate_comm,
        errors_comm,
        errors_count1,
        errors_count2,
        errors_count3,
        errors_count4,
        onboard_control_sensors_present_extended,   // nuovo!
        onboard_control_sensors_enabled_extended,   // nuovo!
        onboard_control_sensors_health_extended     // nuovo!
    );

    send_mavlink_message_to(msg, dest);
}

static void mavlink_thread(void *, void *, void *) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERR("socket() fallita: %d", sock);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);  // porta effimera
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERR("bind() fallita");
        close(sock);
        return;
    }

    // Destinazione fissa per gli heartbeat (il client Python ascolterà su 14551)
    struct sockaddr_in heartbeat_dest;
    memset(&heartbeat_dest, 0, sizeof(heartbeat_dest));
    heartbeat_dest.sin_family = AF_INET;
    heartbeat_dest.sin_port = htons(14550);  // QGC default
    inet_pton(AF_INET, "127.0.0.1", &heartbeat_dest.sin_addr);

    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint64_t last_heartbeat = 0;

    while (true) {
        uint64_t now = k_uptime_get();

        // Invia heartbeat periodico alla destinazione fissa
        if (now - last_heartbeat >= 1000) {
            send_heartbeat_to(&heartbeat_dest);
            send_autopilot_version(&heartbeat_dest);
            send_sys_status_to(&heartbeat_dest); 
            last_heartbeat = now;
        }

        // Ricezione dati (non bloccante)
        struct sockaddr_in recv_addr;
        socklen_t recv_len = sizeof(recv_addr);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&recv_addr, &recv_len);
        if (n > 0) {
            // Aggiorniamo la destinazione dell'heartbeat con l'ultimo mittente (opzionale)
            //memcpy(&heartbeat_dest, &recv_addr, sizeof(recv_addr));

            mavlink_message_t msg;
            mavlink_status_t status;
            for (int i = 0; i < n; ++i) {
                if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
                    switch (msg.msgid) {
                        case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
                            printk("Received PARAM_REQUEST_LIST\n");
                            handle_param_request_list(&recv_addr);
                            break;
                        case MAVLINK_MSG_ID_PARAM_SET: {
                            mavlink_param_set_t set;
                            printk("Received PARAM_SET\n");
                            mavlink_msg_param_set_decode(&msg, &set);
                            handle_param_set(set, &recv_addr);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
        k_sleep(K_MSEC(10));
    }
}

K_THREAD_DEFINE(mavlink_tid, 2048, mavlink_thread, NULL, NULL, NULL, 7, 0, 0);

void mavlink_task_init() {}