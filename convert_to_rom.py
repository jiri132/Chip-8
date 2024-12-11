# Convert a text file with hex values into a binary ROM file

input_file = "program.txt"  # Input text file with hex values
output_file = "program.rom"  # Output binary file

with open(input_file, "r") as txt_file:
    hex_data = txt_file.read().strip().replace(" ", "")  # Read and clean up the input

# Convert the hex string to binary data
binary_data = bytes.fromhex(hex_data)

# Write the binary data to the output file
with open(output_file, "wb") as bin_file:
    bin_file.write(binary_data)

print(f"Binary ROM file saved as {output_file}")
