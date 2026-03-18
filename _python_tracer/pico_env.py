import os
import sys
import time
import shutil
import subprocess
import re
import serial
import serial.tools.list_ports
import tty
import termios

# --- TERMINAL COLORS ---
C_GREEN = "\033[92m"
C_RED = "\033[91m"
C_YELLOW = "\033[93m"
C_RESET = "\033[0m"

# --- HARDWARE & BUILD CONFIG ---
BAUD_RATE = 115200
PROJECT_CONFIG_PATH = "Standard/ProjectConfig.h"
BUILD_CMD = ["cmake", "--build", "build"]
UF2_OUTPUT = "build/Standard/main_blinky.uf2"
PICO_DRIVE = "/Volumes/RPI-RP2"


# --- STATUS HELPERS ---
def print_status(msg):
    """Prints a temporary message that overwrites the current terminal line."""
    print(f"\r    {C_YELLOW}⚡ {msg}{C_RESET}\033[K", end="", flush=True)


def clear_status():
    """Clears the temporary status line entirely."""
    print("\r\033[K", end="", flush=True)


def wait_for_keypress(prompt):
    print_status(prompt)
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        char = sys.stdin.read(1)
        if char == "\x03":
            raise KeyboardInterrupt
        if char == "\x04":
            raise EOFError
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)


# --- SERIAL HELPERS ---
def auto_detect_port():
    """Automatically finds the Pico's serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "usbmodem" in port.device.lower() or "pico" in port.description.lower():
            return port.device
    return None


def check_port_availability():
    print_status("Checking if Pico serial port is available...")
    pico_port = auto_detect_port()
    if pico_port:
        if os.name == "posix":  # macOS / Linux
            try:
                lsof_result = subprocess.run(
                    ["lsof", pico_port], capture_output=True, text=True
                )
                if lsof_result.returncode == 0:
                    clear_status()
                    lines = lsof_result.stdout.strip().split("\n")
                    blocker = (
                        lines[1].split()[0] if len(lines) > 1 else "another program"
                    )
                    print(
                        f"{C_RED}❌ Error: The serial port ({pico_port}) is locked by '{blocker}'.{C_RESET}"
                    )
                    print(
                        f"{C_YELLOW}Please close the connection and run the tests again.{C_RESET}"
                    )
                    sys.exit(1)
            except FileNotFoundError:
                pass

        # Windows / Fallback check
        try:
            with serial.Serial(pico_port, BAUD_RATE, timeout=0.1):
                pass
        except serial.SerialException:
            clear_status()
            print(
                f"{C_RED}❌ Error: The serial port ({pico_port}) is locked by another program.{C_RESET}"
            )
            print(
                f"{C_YELLOW}Please close the connection and run the tests again.{C_RESET}"
            )
            sys.exit(1)
    clear_status()


# --- BUILD HELPERS ---
def patch_config_file(flags):
    """Updates ProjectConfig.h with the test's required flags."""
    with open(PROJECT_CONFIG_PATH, "r") as file:
        content = file.read()
    for flag, value in flags.items():
        pattern = rf"(#define\s+{flag}\s+)\d+"
        replacement = rf"\g<1>{value}"
        content = re.sub(pattern, replacement, content)
    with open(PROJECT_CONFIG_PATH, "w") as file:
        file.write(content)


def compile_and_flash():
    """Compiles the project and copies the UF2 to the Pico."""
    print_status("[Build] Running CMake...")
    result = subprocess.run(BUILD_CMD, capture_output=True, text=True)
    if result.returncode != 0:
        clear_status()
        print(f"{C_RED}❌ Build Failed:\n{result.stderr}{C_RESET}")
        return False

    wait_for_keypress(
        "[Action Required] Hold BOOTSEL, press RUN, release BOOTSEL, then press ANY KEY..."
    )

    timeout = 10.0
    start_wait = time.time()
    while not os.path.exists(PICO_DRIVE):
        elapsed = time.time() - start_wait
        print_status(f"[Flash] Waiting for Pico drive to mount... ({elapsed:.1f}s)")
        if elapsed > timeout:
            clear_status()
            print(
                f"{C_RED}❌ Pico drive did not appear after {timeout} seconds!{C_RESET}"
            )
            return False
        time.sleep(0.1)

    max_retries = 5
    for attempt in range(max_retries):
        try:
            print_status(
                f"[Flash] Copying UF2 to Pico... (Attempt {attempt + 1}/{max_retries})"
            )
            shutil.copy(UF2_OUTPUT, PICO_DRIVE)
            break
        except PermissionError as e:
            if attempt < max_retries - 1:
                time.sleep(0.2)
            else:
                clear_status()
                print(
                    f"{C_RED}❌ Failed to copy after {max_retries} attempts: {e}{C_RESET}"
                )
                return False
        except IOError as e:
            clear_status()
            print(f"{C_RED}❌ Error during copy (Drive disconnected): {e}{C_RESET}")
            return False

    clear_status()
    return True


def get_binary_memory_usage():
    """Runs arm-none-eabi-size and parses the output."""
    elf_path = "build/Standard/main_blinky.elf"
    if not os.path.exists(elf_path):
        return None

    try:
        result = subprocess.run(
            ["arm-none-eabi-size", elf_path], capture_output=True, text=True
        )
        if result.returncode == 0:
            lines = result.stdout.strip().split("\n")
            if len(lines) >= 2:
                parts = lines[1].split()
                return {
                    "text": int(parts[0]),
                    "data": int(parts[1]),
                    "bss": int(parts[2]),
                }
    except FileNotFoundError:
        print(
            f"\n{C_YELLOW}⚠ Warning: 'arm-none-eabi-size' not found in PATH. Memory tracking disabled.{C_RESET}"
        )
    return None
