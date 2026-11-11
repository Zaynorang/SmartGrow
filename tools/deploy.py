#!/usr/bin/env python3
"""Run on the VPS with Docker Engine + Compose v2. No credentials in argv/logs."""
import argparse
import os
import re
import secrets
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def run(args, data=None, capture=False):
    return subprocess.run(args, cwd=ROOT, input=data, text=True, check=True,
                          stdout=subprocess.PIPE if capture else None).stdout

def compose(*args, **kwargs):
    return run(["docker", "compose", *args], **kwargs)

def environment():
    return dict(line.split("=", 1) for line in (ROOT/".env").read_text().splitlines() if line and not line.startswith("#"))

def hashed_password(username, password):
    # Mosquitto converts its own password format. Plaintext exists only in a
    # private temporary file and is removed by TemporaryDirectory even on error.
    with tempfile.TemporaryDirectory(prefix="sg-auth-", dir=ROOT/".private") as directory:
        f = Path(directory)/"passwords"
        f.write_text(f"{username}:{password}\n")
        f.chmod(0o600)
        run(["docker", "run", "--rm", "--user", f"{os.getuid()}:{os.getgid()}",
             "-v", f"{directory}:/work", "eclipse-mosquitto:2.0", "mosquitto_passwd", "-U", "/work/passwords"])
        return f.read_text()

def nginx_http(domain):
    return f'''server {{
  listen 80;
  server_name {domain};
  location /.well-known/acme-challenge/ {{ root /var/www/certbot; }}
  location / {{ return 503 "SmartGrow: certificate setup in progress"; }}
}}
'''

def nginx_https(domain):
    return f'''limit_req_zone $binary_remote_addr zone=auth:10m rate=10r/m;
server {{
  listen 80;
  server_name {domain};
  location /.well-known/acme-challenge/ {{ root /var/www/certbot; }}
  location / {{ return 301 https://{domain}$request_uri; }}
}}
server {{
  listen 443 ssl;
  server_name {domain};
  ssl_certificate /etc/nginx/tls/fullchain.pem;
  ssl_certificate_key /etc/nginx/tls/privkey.pem;
  ssl_protocols TLSv1.2 TLSv1.3;
  client_max_body_size 32k;
  add_header Referrer-Policy same-origin always;
  add_header X-Content-Type-Options nosniff always;
  add_header Content-Security-Policy "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'self'; frame-ancestors 'none'; form-action 'self'" always;
  location /static/ {{ alias /static/; expires 1h; }}
  location ~ ^/(login|register|claim)/ {{
    limit_req zone=auth burst=10 nodelay;
    proxy_pass http://web:8000;
    proxy_set_header Host $host;
    proxy_set_header X-Forwarded-Proto https;
    proxy_set_header X-Real-IP $remote_addr;
  }}
  location / {{
    proxy_pass http://web:8000;
    proxy_set_header Host $host;
    proxy_set_header X-Forwarded-Proto https;
    proxy_set_header X-Real-IP $remote_addr;
  }}
}}
'''

def copy_certificates():
    script = '''from pathlib import Path
import os, shutil
for name in ("fullchain.pem", "privkey.pem"):
 p=Path("/out")/name
 shutil.copyfile(Path("/etc/letsencrypt/live/smartgrow")/name, p)
 os.chmod(p,0o640 if name=="privkey.pem" else 0o644)
 os.chown(p,1883,1883)
'''
    run(["docker", "run", "--rm", "--entrypoint", "python", "-v",
         f"{ROOT/'deploy/letsencrypt'}:/etc/letsencrypt:ro", "-v", f"{ROOT/'deploy/tls'}:/out",
         "certbot/certbot:latest", "-c", script])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["init", "tls", "renew"])
    parser.add_argument("--web-domain")
    parser.add_argument("--mqtt-domain")
    parser.add_argument("--email")
    args = parser.parse_args()
    if args.action == "init":
        if (ROOT/".env").exists():
            parser.error(".env already exists; refusing to replace credentials. Follow recovery steps in README.")
        for domain in (args.web_domain, args.mqtt_domain):
            if not domain or not re.fullmatch(r"[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}", domain): parser.error("Two valid DNS names required.")
        if not args.email or "@" not in args.email or any(c.isspace() for c in args.email): parser.error("Valid --email required.")
        for d in [".private", "deploy/tls", "deploy/letsencrypt", "deploy/mosquitto"]: (ROOT/d).mkdir(parents=True, exist_ok=True)
        (ROOT/".private").chmod(0o700)
        cfg = {"DEBUG":"0", "DJANGO_SECRET_KEY":secrets.token_urlsafe(50), "ALLOWED_HOSTS":args.web_domain,
               "CSRF_TRUSTED_ORIGINS":f"https://{args.web_domain}", "WEB_DOMAIN":args.web_domain,
               "MQTT_PUBLIC_HOST":args.mqtt_domain, "CERT_EMAIL":args.email,
               "POSTGRES_DB":"smartgrow", "POSTGRES_USER":"smartgrow", "POSTGRES_PASSWORD":secrets.token_urlsafe(32),
               "MQTT_USERNAME":"smartgrow-backend", "MQTT_PASSWORD":secrets.token_urlsafe(32)}
        hashes = hashed_password(cfg["MQTT_USERNAME"],cfg["MQTT_PASSWORD"])
        env_path = ROOT/".env"
        env_path.touch(mode=0o600); env_path.write_text("".join(f"{k}={v}\n" for k,v in cfg.items())); env_path.chmod(0o600)
        (ROOT/"deploy/mosquitto/passwords").write_text(hashes)
        (ROOT/"deploy/mosquitto/passwords").chmod(0o644)
        (ROOT/"deploy/mosquitto/acl").write_text("user smartgrow-backend\ntopic write smartgrow/v1/+/command\n" +
            "".join(f"topic read smartgrow/v1/+/{t}\n" for t in ["telemetry","availability","ack"]))
        (ROOT/"deploy/nginx.conf").write_text(nginx_http(args.web_domain))
        compose("build")
        compose("up", "-d", "db")
        compose("run", "--rm", "web", "python", "manage.py", "migrate", "--noinput")
        compose("up", "-d", "web", "nginx")
        print("Initial setup complete. Run: python3 tools/deploy.py tls")
        return
    cfg = environment()
    if args.action == "tls":
        compose("run", "--rm", "certbot", "certonly", "--webroot", "-w", "/var/www/certbot",
                "--cert-name", "smartgrow", "--key-type", "rsa", "--preferred-chain", "ISRG Root X1",
                "--email", cfg["CERT_EMAIL"], "--agree-tos", "--non-interactive",
                "-d", cfg["WEB_DOMAIN"], "-d", cfg["MQTT_PUBLIC_HOST"])
    else:
        compose("run", "--rm", "certbot", "renew", "--quiet", "--non-interactive")
    copy_certificates()
    if args.action == "tls":
        (ROOT/"deploy/nginx.conf").write_text(nginx_https(cfg["WEB_DOMAIN"]))
    compose("exec", "-T", "nginx", "nginx", "-t")
    compose("exec", "-T", "nginx", "nginx", "-s", "reload")
    if args.action == "tls": compose("up", "-d", "mosquitto", "worker")
    else: compose("kill", "-s", "HUP", "mosquitto")
    print("TLS installed/renewed. Web:", f"https://{cfg['WEB_DOMAIN']}")

if __name__ == "__main__":
    main()
