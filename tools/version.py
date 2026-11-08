Import("env")
# Stamp the firmware version into the build: FW_VERSION=1.2.3 pio run   (the release workflow sets it from the git tag).
# Without the variable the firmware reports its dev default from src/ota.h.
import os

v = os.environ.get("FW_VERSION", "").strip()
if v:
    env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(v))])
    print("Firmware version:", v)
