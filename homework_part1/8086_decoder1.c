#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Figure 4-20, page 262
typedef union {
    uint16_t val;
    struct __attribute__((packed)) {
        uint8_t Word : 1;
        uint8_t Direction : 1;
        uint8_t Opcode : 6;
        uint8_t RegMem : 3;
        uint8_t Register : 3;
        uint8_t Mode : 2;
    } Fields;
} OpcodeWord_t;

const char *GetRegisterStr(uint8_t Base, uint8_t IsWord) {
    const uint8_t Index = Base | (IsWord << 3);
    // Table 4-9 Page 263
    const char *RegNames[] = {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh", "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    };
    return RegNames[Index];
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR: Must provide path to input bin file on commandline.\n");
    }

    const char *BinFile = argv[1];
    FILE *BinStream = fopen(BinFile, "rb");
    if (!BinStream) {
        printf("ERROR: Could not open %s for read\n", BinFile);
        exit(1);
    }

    size_t byte_counter = 0;
    OpcodeWord_t OpcodeWord;
    while (fread(&OpcodeWord, sizeof(OpcodeWord_t), 1, BinStream)) {
        byte_counter += sizeof(OpcodeWord_t);

        if (OpcodeWord.Fields.Mode != 3) {
            printf("ERROR: Found non-mode 3 access at byte 0x%lx\n", byte_counter);
            continue;
        }

        const char *RegMemStr = GetRegisterStr(OpcodeWord.Fields.RegMem, OpcodeWord.Fields.Word);
        const char *RegisterStr = GetRegisterStr(OpcodeWord.Fields.Register, OpcodeWord.Fields.Word);
        if (OpcodeWord.Fields.Direction) {
            printf("mov %s, %s\n", RegisterStr, RegMemStr);
        } else {
            printf("mov %s, %s\n", RegMemStr, RegisterStr);
        }
    }

    fclose(BinStream);
    return 0;
}
