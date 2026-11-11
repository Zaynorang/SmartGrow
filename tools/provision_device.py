#!/usr/bin/env python3
"""Create broker identity, activation code and ESP32 header for ONE physical device."""
import argparse
import fcntl
import json
import os
import re
import secrets
import ssl
import subprocess
import urllib.request
from pathlib import Path
from deploy import ROOT, environment, hashed_password, compose

def cpp(value):
    return json.dumps(value, ensure_ascii=True)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("device_id"); p.add_argument("--name",default="SmartGrow"); p.add_argument("--ca-file",type=Path)
    args=p.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{3,16}",args.device_id): p.error("Device ID: 3–16 letters, digits, _ or - (fits the setup hotspot SSID)")
    cfg=environment(); out=ROOT/".private"/args.device_id
    if out.exists(): p.error("Device folder already exists; use its existing files. Refusing credential rotation.")
    ca=args.ca_file.read_text() if args.ca_file else urllib.request.urlopen(
        "https://letsencrypt.org/certs/isrgrootx1.pem",timeout=20).read().decode()
    ssl.create_default_context(cadata=ca) # Reject invalid trust material before any mutation.
    user="sg_"+args.device_id
    mqtt_password=secrets.token_urlsafe(32); ap_password=secrets.token_urlsafe(12); claim=secrets.token_urlsafe(24)
    hashed=hashed_password(user,mqtt_password)
    out.mkdir(mode=0o700)
    header=(f'#pragma once\n#define DEVICE_ID {cpp(args.device_id)}\n#define MQTT_HOSTNAME {cpp(cfg["MQTT_PUBLIC_HOST"])}\n'
            f'#define MQTT_PORT_NUMBER 8883\n#define MQTT_USER {cpp(user)}\n#define MQTT_PASSWORD {cpp(mqtt_password)}\n'
            f'#define SETUP_AP_PASSWORD {cpp(ap_password)}\nstatic const char MQTT_ROOT_CA[] = {cpp(ca)};\n')
    # Retain exact credentials before broker/DB mutation to allow recovery without guessing.
    (out/"device_config.h").write_text(header); (out/"device_config.h").chmod(0o600)
    registration={"device_id":args.device_id,"name":args.name,"claim_code":claim}
    (out/"registration.json").write_text(json.dumps(registration)); (out/"registration.json").chmod(0o600)
    (out/"KARTU-PERANGKAT.txt").write_text(
        f"SmartGrow — {args.name}\nID perangkat: {args.device_id}\nKode aktivasi sekali pakai: {claim}\n"
        f"Hotspot: SmartGrow-Setup-{args.device_id}\nKata sandi hotspot: {ap_password}\n"
        f"Pengaturan lokal: http://192.168.4.1\nDashboard: https://{cfg['WEB_DOMAIN']}\n"
        "Jaga kartu ini. Kode aktivasi dan kata sandi hotspot bersifat rahasia.\n")
    (out/"KARTU-PERANGKAT.txt").chmod(0o600)
    # One provisioning process at a time; append ACLs without rewriting other devices.
    with (ROOT/".private/provision.lock").open("w") as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        passwd=ROOT/"deploy/mosquitto/passwords"
        if any(line.split(":",1)[0]==user for line in passwd.read_text().splitlines()):
            p.error("Broker identity already exists. See recovery instructions.")
        with passwd.open("a") as f: f.write(hashed)
        with (ROOT/"deploy/mosquitto/acl").open("a") as f:
            f.write(f"\nuser {user}\ntopic read smartgrow/v1/{args.device_id}/command\n")
            for topic in ["telemetry","availability","ack"]: f.write(f"topic write smartgrow/v1/{args.device_id}/{topic}\n")
        compose("kill","-s","HUP","mosquitto")
        compose("exec","-T","web","python","manage.py","create_device",data=json.dumps(registration))
    print(f"Created {args.device_id}. Private header and buyer card: {out}")
    print("Copy device_config.h into firmware/include/ on the build computer. Do not share that header with buyers.")

if __name__=="__main__": main()
