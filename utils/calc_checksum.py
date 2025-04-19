import os

if os.name == 'nt':
    os.system('')


def calc_checksum(cmd):
    cs = 0
    for c in cmd:
        cs ^= ord(c)
    return cs


while True:
    cmd = input("Comando (vuoto per uscire): ").strip()
    if cmd == "":
        break
    cs = calc_checksum(cmd)
    print(f"Checksum XOR: {cs}  →  Comando completo:")
    print(f"{cmd} *{cs}")
