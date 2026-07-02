Import("env")  # type: ignore[name-defined]

# Use environment-specific artifact names, e.g. USBPaddleController_pico.uf2.
env.Replace(PROGNAME=f"USBPaddleController_{env['PIOENV']}")  # type: ignore[name-defined]
