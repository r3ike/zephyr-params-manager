#!/usr/bin/env python3
import time
from pymavlink import mavutil

def main():
    # Ascolta su 14551, invia a 14550
    conn = mavutil.mavlink_connection('udpin:127.0.0.1:14551')
    print("In attesa di heartbeat...")
    conn.wait_heartbeat()
    print("Heartbeat ricevuto.")

    print("Richiesta lista parametri...")
    conn.mav.param_request_list_send(conn.target_system, conn.target_component)
    while True:
        msg = conn.recv_match(type='PARAM_VALUE', blocking=True, timeout=2)
        if msg is None:
            break
        print(f"Ricevuto: {msg.param_id} = {msg.param_value}")
        if msg.param_index == msg.param_count - 1:
            break

    # Test modifica parametro
    new_gain = 2.5
    print(f"Imposto ROLL_P a {new_gain}")
    conn.mav.param_set_send(conn.target_system, conn.target_component, b'ROLL_P', new_gain, mavutil.mavlink.MAV_PARAM_TYPE_REAL32)

    while True:
        msg = conn.recv_match(type='PARAM_VALUE', blocking=True, timeout=2)
        if msg and msg.param_id == 'ROLL_P':
            print(f"Conferma ricevuta: {msg.param_id} = {msg.param_value}")
            break
        elif msg is None:
            print("Timeout attesa conferma.")
            break

if __name__ == "__main__":
    main()