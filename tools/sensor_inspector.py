#!/usr/bin/env python3
"""
====================================================================
BETTY IoT GATEWAY - LIVE SENSOR INSPECTOR & DISCOVERY TOOL
====================================================================
Monitors connected sensors, validates link-level authentication,
inspects live telemetry, and verifies uplinks to Betty PaaS.
====================================================================
"""

import argparse
import json
import os
import sys
import time
import urllib.request
import urllib.error

# ANSI Color codes for clean terminal output
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
BLUE = "\033[94m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"


def clear_screen():
    os.system("cls" if os.name == "nt" else "clear")


def fetch_json(url, timeout=3):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "BettySensorInspector/1.0"})
        with urllib.request.urlopen(req, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except Exception as e:
        return None


def post_json(url, data, headers=None, timeout=3):
    if headers is None:
        headers = {}
    headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=json.dumps(data).encode("utf-8"), headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            return response.getcode(), json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read().decode("utf-8"))
        except Exception:
            return e.code, {"error": str(e)}
    except Exception as e:
        return 0, {"error": str(e)}


def render_dashboard(host, port):
    sensors_url = f"http://{host}:{port}/api/sensors/connected"
    status_url = f"http://{host}:{port}/api/status"

    sensors_data = fetch_json(sensors_url)
    status_data = fetch_json(status_url)

    clear_screen()
    print(f"{BOLD}{CYAN}========================================================================{RESET}")
    print(f"{BOLD}{CYAN}   BETTY IoT GATEWAY - LIVE SENSOR INSPECTOR & DISCOVERY               {RESET}")
    print(f"{BOLD}{CYAN}   Target Gateway: http://{host}:{port}                                {RESET}")
    print(f"{BOLD}{CYAN}========================================================================{RESET}")

    if not status_data:
        print(f"\n{RED}[!] Gateway unreachable at http://{host}:{port}. Re-trying...{RESET}\n")
        return

    # Status Bar
    uptime = status_data.get("uptimeSec", 0)
    uptime_str = f"{uptime // 3600}h {(uptime % 3600) // 60}m {uptime % 60}s"
    betty_online = status_data.get("bettyConnected", False)
    betty_badge = f"{GREEN}ONLINE{RESET}" if betty_online else f"{YELLOW}STANDBY / RETRY{RESET}"
    sent = status_data.get("sentToBetty", 0)
    drops = status_data.get("authRejections", 0)
    pairing = status_data.get("pairingRemaining", 0)

    print(f"{BOLD}Status:{RESET} Uptime: {uptime_str} | Betty PaaS Uplink: {betty_badge} | Sent: {BOLD}{sent}{RESET} | Auth Drops: {RED}{drops}{RESET}")
    if pairing > 0:
        print(f"{BOLD}{YELLOW}🔐 WPS Pairing Mode Active: {pairing}s remaining to accept new sensors{RESET}")
    print(f"{CYAN}------------------------------------------------------------------------{RESET}")

    sensors = sensors_data.get("sensors", []) if sensors_data else []
    total = sensors_data.get("total", 0) if sensors_data else 0
    online = sensors_data.get("online", 0) if sensors_data else 0

    print(f"{BOLD}Connected Sensors ({online} Online / {total} Total):{RESET}")
    print(f"{'STATUS':<8} {'SENSOR ID':<16} {'INTERFACE':<12} {'MODEL':<14} {'SIGNAL':<10} {'PACKETS':<8} {'METRICS'}")
    print(f"{'-'*8} {'-'*16} {'-'*12} {'-'*14} {'-'*10} {'-'*8} {'-'*24}")

    if not sensors:
        print(f"  {YELLOW}No sensors detected yet. Waiting for incoming telemetry...{RESET}")
    else:
        for s in sensors:
            status_tag = f"{GREEN}ONLINE{RESET} " if s.get("online") else f"{RED}OFFLINE{RESET}"
            sid = s.get("id", "Unknown")[:15]
            iface = s.get("interface", "lora")[:11]
            model = s.get("model", "Generic")[:13]
            rssi = f"{s.get('rssi')} dBm" if s.get("rssi") else "N/A"
            pkts = str(s.get("packets", 0))
            metrics = json.dumps(s.get("metrics", s.get("raw", {})))
            if len(metrics) > 35:
                metrics = metrics[:32] + "..."

            print(f"{status_tag:<17} {sid:<16} {iface:<12} {model:<14} {rssi:<10} {pkts:<8} {metrics}")

    print(f"{CYAN}------------------------------------------------------------------------{RESET}")
    print(f"Press {BOLD}Ctrl+C{RESET} to exit | Auto-refreshing every 2s...")


def send_test_telemetry(host, port, sensor_id, key, temp=24.5, hum=58.2):
    url = f"http://{host}:{port}/api/sensor/data"
    headers = {"X-Sensor-Key": key}
    payload = {
        "sensorId": sensor_id,
        "model": "Inspector-Simulator",
        "metrics": {
            "temperature": temp,
            "humidity": hum,
            "simulated": True
        }
    }
    print(f"\n[Inspector] Sending test telemetry for '{sensor_id}' to {url} (Key: '{key}')...")
    code, resp = post_json(url, payload, headers=headers)
    if code == 200:
        print(f"{GREEN}[OK] Telemetry ACCEPTED: {json.dumps(resp)}{RESET}")
    elif code == 401:
        print(f"{RED}[REJECTED] HTTP 401 Unauthorized: Invalid credentials! Security guard working correctly.{RESET}")
    else:
        print(f"{YELLOW}[ERROR] HTTP {code}: {json.dumps(resp)}{RESET}")


def trigger_pairing(host, port):
    url = f"http://{host}:{port}/api/sensors/pair"
    code, resp = post_json(url, {})
    if code == 200:
        print(f"{GREEN}[OK] Gateway WPS Pairing Mode activated for 60 seconds!{RESET}")
    else:
        print(f"{RED}[!] Failed to trigger pairing mode: HTTP {code}{RESET}")


def main():
    parser = argparse.ArgumentParser(description="Betty IoT Gateway - Live Sensor Inspector & Discovery")
    parser.add_argument("--host", default="192.168.4.1", help="Gateway IP address (default: 192.168.4.1)")
    parser.add_argument("--port", type=int, default=80, help="Gateway HTTP port (default: 80)")
    parser.add_argument("--pair", action="store_true", help="Trigger 60s WPS pairing window on Gateway")
    parser.add_argument("--test-auth", action="store_true", help="Send authorized and unauthorized test telemetry")
    parser.add_argument("--key", default="betty1234", help="Link Pre-Shared Key (default: betty1234)")
    args = parser.parse_args()

    if args.pair:
        trigger_pairing(args.host, args.port)
        return

    if args.test_auth:
        print(f"{BOLD}=== Running Authentication & Credential Verification Battery ==={RESET}")
        # Test 1: Authorized packet
        send_test_telemetry(args.host, args.port, "TEST-SENSOR-AUTH", args.key, 25.1, 52.0)
        time.sleep(1)
        # Test 2: Unauthorized packet (wrong key)
        send_test_telemetry(args.host, args.port, "ROGUE-SENSOR-99", "wrong_key_123", 99.9, 99.9)
        return

    # Live Dashboard Loop
    try:
        while True:
            render_dashboard(args.host, args.port)
            time.sleep(2)
    except KeyboardInterrupt:
        print(f"\n{CYAN}Sensor Inspector stopped.{RESET}")


if __name__ == "__main__":
    main()
