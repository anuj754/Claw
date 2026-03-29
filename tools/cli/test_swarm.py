#!/usr/bin/env python3
import socket
import json
import time

SWARM_PORT = 39888
BROADCAST_IP = "255.255.255.255"

def send_fake_heartbeat():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    payload = {
        "swarm_type": "heartbeat",
        "status": "active",
        "device_type": "smart_oven",
        "capabilities": ["control_oven_temperature", "set_cook_program"]
    }
    
    msg = json.dumps(payload).encode('utf-8')
    s.sendto(msg, (BROADCAST_IP, SWARM_PORT))
    print(f"Sent fake heartbeat: {payload}")

def send_fake_event():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    payload = {
        "swarm_type": "event",
        "event": {
            "type_id": 11, # kCustom
            "source": "oven_sensor",
            "name": "oven.preheated",
            "data": {"temperature": 180},
            "timestamp": int(time.time() * 1000),
            "device_type": "smart_oven"
        }
    }
    
    msg = json.dumps(payload).encode('utf-8')
    s.sendto(msg, (BROADCAST_IP, SWARM_PORT))
    print(f"Sent fake event: {payload}")

if __name__ == "__main__":
    send_fake_heartbeat()
    time.sleep(1)
    send_fake_event()
