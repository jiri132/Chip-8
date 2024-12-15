instruction_set = {
    "CLR": 0x00E0,
    "RET": 0x00EE,
    "JMP": 0x1000,
    "CAL": 0x2000,
    "SEB": 0x3000,  # With vX, Byte
    "SNE": 0x4000,
    "SEV": 0x5000,  # With vX, vY
    "LD": 0x6000,  # With vX, Byte
    "ADD": 0x7000,  # With vX, Byte
    "SET": 0x8000,  # With vX, vY
    "OR": 0x8001,
    "AND": 0x8002,
    "XOR": 0x8003,
    "ADDV": 0x8004,  # With vX, vY
    "SUB": 0x8005,
    "SHR": 0x8006,
    "SUBN": 0x8007,
    "SHL": 0x800E,
    "SNEV": 0x9000,  # With vX, vY
    "LDI": 0xA000,  # With I and XXX
    "JMPV": 0xB000,  # With XXX + v0
    "RAN": 0xC000,
    "DIS": 0xD000,
    "SKP": 0xE09E,
    "SKNP": 0xE0A1,
    "GDT": 0xF007,
    "LDK": 0xF00A,
    "LDT": 0xF015,
    "LDS": 0xF018,
    "ADDI": 0xF01E,
    "LDF": 0xF029,
    "BCD": 0xF033,
    "RTM": 0xF055,  # Stores all registers till vX into memory (RegistryToMemory)
    "MTR": 0xF065  # Gets all registers till vX from memory to register. (MemoryToRegister)
}

def assemble_line(line):
    """
    Parse a single line of assembly and convert it to machine code.
    """
    parts = line.split()
    if not parts:
        return None

    mnemonic = parts[0]
    args = parts[1:] if len(parts) > 1 else []

    if mnemonic not in instruction_set:
        raise ValueError(f"Unknown instruction: {mnemonic}")

    opcode = instruction_set[mnemonic]

    if opcode in [0x00E0, 0x00EE]:  # No arguments
        return opcode

    if opcode & 0xF000 == 0x1000 or opcode & 0xF000 == 0x2000 or opcode & 0xF000 == 0xA000 or opcode & 0xF000 == 0xB000:
        # JMP, CAL, LDI, JMPV: Address (12-bit)
        address = int(args[0], 16)
        return opcode | (address & 0x0FFF)

    if opcode & 0xF000 == 0x3000 or opcode & 0xF000 == 0x4000 or opcode & 0xF000 == 0x6000 or opcode & 0xF000 == 0x7000:
        # SEB, SNE, LD, ADD: vX, Byte
        vx = int(args[0][1:], 16)
        byte = int(args[1], 16)
        return opcode | (vx << 8) | byte

    if opcode & 0xF000 == 0x5000 or opcode & 0xF000 == 0x9000 or (opcode & 0xF00F) in [0x8000, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0x8007]:
        # SEV, SNEV, SET, OR, AND, XOR, ADDV, SUB, SUBN: vX, vY
        vx = int(args[0][1:], 16)
        vy = int(args[1][1:], 16)
        return opcode | (vx << 8) | (vy << 4)

    if opcode & 0xF00F == 0x8006 or opcode & 0xF00F == 0x800E:
        # SHR, SHL: vX
        vx = int(args[0][1:], 16)
        return opcode | (vx << 8)

    if opcode & 0xF000 == 0xC000 or opcode & 0xF000 == 0xD000:
        # RAN, DIS: vX, Byte
        vx = int(args[0][1:], 16)
        byte = int(args[1], 16)
        return opcode | (vx << 8) | byte

    if opcode & 0xF0FF in [0xE09E, 0xE0A1]:
        # SKP, SKNP: vX
        vx = int(args[0][1:], 16)
        return opcode | (vx << 8)

    if opcode & 0xF0FF in [0xF007, 0xF00A, 0xF015, 0xF018, 0xF01E, 0xF029, 0xF033, 0xF055, 0xF065]:
        # GDT, LDK, LDT, LDS, ADDI, LDF, BCD, RTM, MTR: vX
        vx = int(args[0][1:], 16)
        return opcode | (vx << 8)

    raise ValueError(f"Unhandled instruction or invalid arguments: {line}")

def assemble(source):
    """
    Convert a list of assembly lines into machine code.
    """
    machine_code = []
    for line in source:
        line = line.strip()
        if not line or line.startswith(";"):  # Skip empty lines or comments
            continue
        machine_code.append(assemble_line(line))
    return machine_code

# Example usage
source_code = [
    "LD V1 0x3A",
    "ADD V1 0x05",
    "JMP 0x200"
]

input_file = "program.c8"  # Input text file with hex values
output_file = "assembly.rom"  # Output binary file

file_content = []
with open(input_file, "r") as txt_file:
    file_content = txt_file.read().strip().splitlines()  # Read and clean up the input

# Assuming you have the `assemble` function defined as in your code
machine_code = assemble(file_content)

# Convert the hex string to binary data
binary_data = b""
for code in machine_code:
    hex_string = hex(code)[2:]  # Remove the '0x' prefix
    hex_string = hex_string.zfill(4)  # Ensure the hex string is 4 characters long
    binary_data += bytes.fromhex(hex_string)

# Write the binary data to the output file
with open(output_file, "wb") as bin_file:
    bin_file.write(binary_data)

print(f"Binary file '{output_file}' written successfully!")
