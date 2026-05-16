# load_env.py — Reads .env and injects values as build defines
# Called by PlatformIO as a pre-build script

Import("env")
import os

env_file = os.path.join(env.get("PROJECT_DIR", "."), ".env")

try:
    with open(env_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, val = line.split('=', 1)
                key = key.strip()
                val = val.strip().strip('"').strip("'")
                if key in ('MQTT_PORT',):  # only these are numeric
                    env.Append(CPPDEFINES=[(key, val)])
                else:
                    env.Append(CPPDEFINES=[(key, '\\"' + val + '\\"')])
    print("ENV: Loaded credentials from .env")
except FileNotFoundError:
    print("WARNING: .env not found! Copy .env.example to .env and fill in your credentials.")
