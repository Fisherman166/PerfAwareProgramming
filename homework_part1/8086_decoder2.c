#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    size_t FileSize;
    const uint8_t *const Bin;
} FileInfo_t;

typedef enum {
    MemNoDispalcement = 0,
    MemByteDispalcement = 1,
    MemWordDispalcement = 2,
    RegisterMode = 3,
} ModeField_t;

// Figure 4-20, page 262
typedef union {
    uint8_t val;
    struct __attribute__((packed)) {
        uint8_t Word : 1;
        uint8_t Direction : 1;
        uint8_t Opcode : 6;
    } Fields;
} RegMemToFromRegOpcode_t;

typedef union {
    uint8_t val;
    struct __attribute__((packed)) {
        uint8_t RegMem : 3;
        uint8_t Register : 3;
        ModeField_t Mode : 2;
    } Fields;
} RegMemToFromRegMod_t;

typedef union {
    uint8_t val;
    struct __attribute__((packed)) {
        uint8_t Reg : 3;
        uint8_t Word : 1;
        uint8_t Opcode : 4;
    } Fields;
} ImmediateToReg_t;

FileInfo_t LoadBin(const char *BinFile) {
    FILE *BinStream = fopen(BinFile, "rb");
    if (!BinStream) {
        printf("[%s] ERROR: Could not open %s for read\n", __func__, BinFile);
        exit(1);
    }

    fseek(BinStream, 0L, SEEK_END);
    size_t FileSize = ftell(BinStream);
    rewind(BinStream);
    printf("Binary %s is 0x%lx bytes\n", BinFile, FileSize);

    uint8_t *const Bin = malloc(FileSize);
    if (!Bin) {
        printf("ERROR: Could not malloc %lu bytes for binary.\n", FileSize);
        exit(1);
    }

    fread(Bin, FileSize, 1, BinStream);
    fclose(BinStream);

    FileInfo_t Info = {FileSize, Bin};
    return Info;
}

uint8_t GetByteFromBin(const FileInfo_t FileInfo, const uint16_t Address) {
    if (Address >= FileInfo.FileSize) {
        printf("[%s] ERROR: Attempted to access address 0x%x which is beyond the size of the bin 0x%lx\n", __func__,
               Address, FileInfo.FileSize);
        exit(1);
    }
    return FileInfo.Bin[Address];
}

uint16_t GetWordFromBin(const FileInfo_t FileInfo, const uint16_t Address) {
    if ((Address + 1) >= FileInfo.FileSize) {
        printf("[%s] ERROR: Attempted to access address 0x%x which is beyond the size of the bin 0x%lx\n", __func__,
               Address, FileInfo.FileSize);
        exit(1);
    }
    return *((uint16_t *)(FileInfo.Bin + Address));
}

const char *GetRegisterStr(uint8_t Base, uint8_t IsWord) {
    const uint8_t Index = Base | (IsWord << 3);
    // Table 4-9 Page 263
    const char *RegNames[] = {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh", "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    };
    return RegNames[Index];
}

uint16_t ImmediateToRegister(const uint16_t ip, const uint8_t OpcodeByte, const FileInfo_t FileInfo) {
    ImmediateToReg_t Opcode = {OpcodeByte};
    uint16_t BytesRead = 2;

    int16_t ImmediateValue;
    if (Opcode.Fields.Word) {
        BytesRead++;
        ImmediateValue = GetWordFromBin(FileInfo, ip + 1);
    } else {
        ImmediateValue = (int16_t)GetByteFromBin(FileInfo, ip + 1);
    }

    const char *RegisterStr = GetRegisterStr(Opcode.Fields.Reg, Opcode.Fields.Word);
    if (Opcode.Fields.Word) {
        printf("mov %s, %d\n", RegisterStr, ImmediateValue);
    } else {
        printf("mov %s, %d\n", RegisterStr, (int8_t)ImmediateValue);
    }

    return BytesRead;
}

uint16_t RegisterMemToFromRegister(const uint16_t ip, const uint8_t OpcodeByte, const FileInfo_t FileInfo) {
    const RegMemToFromRegOpcode_t Opcode = {OpcodeByte};
    const RegMemToFromRegMod_t Mod = {GetByteFromBin(FileInfo, ip + 1)};
    const char *DisplacementEffectiveAddressStr[] = {"bx + si", "bx + di", "bp + si", "bp + di",
                                                     "si",      "di",      "bp",      "bx"};

    switch (Mod.Fields.Mode) {
    case MemNoDispalcement: {
        const char *EffectiveAddressStr[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "direct", "bx"};
        const char *RegMemStr = EffectiveAddressStr[Mod.Fields.RegMem];
        const char *RegisterStr = GetRegisterStr(Mod.Fields.Register, Opcode.Fields.Word);

        // See asterisk below Table 4-8
        const uint8_t DirectAddressRM = 6;
        if (Mod.Fields.RegMem == DirectAddressRM) {
            const uint16_t Displacement = GetWordFromBin(FileInfo, ip + 2);
            if (Opcode.Fields.Direction) {
                printf("mov %s, [%x]\n", RegisterStr, Displacement);
            } else {
                printf("mov [%x], %s\n", Displacement, RegisterStr);
            }
            return 4;
        } else {
            if (Opcode.Fields.Direction) {
                printf("mov %s, [%s]\n", RegisterStr, RegMemStr);
            } else {
                printf("mov [%s], %s\n", RegMemStr, RegisterStr);
            }
            return 2;
        }
    }
    case MemByteDispalcement: {
        const int8_t Displacement = GetByteFromBin(FileInfo, ip + 2);
        const char *RegMemStr = DisplacementEffectiveAddressStr[Mod.Fields.RegMem];
        const char *RegisterStr = GetRegisterStr(Mod.Fields.Register, Opcode.Fields.Word);
        if (Opcode.Fields.Direction) {
            printf("mov %s, [%s + %d]\n", RegisterStr, RegMemStr, Displacement);
        } else {
            printf("mov [%s + %d], %s\n", RegMemStr, Displacement, RegisterStr);
        }
        return 3;
    }
    case MemWordDispalcement: {
        const int16_t Displacement = GetWordFromBin(FileInfo, ip + 2);
        const char *RegMemStr = DisplacementEffectiveAddressStr[Mod.Fields.RegMem];
        const char *RegisterStr = GetRegisterStr(Mod.Fields.Register, Opcode.Fields.Word);
        if (Opcode.Fields.Direction) {
            printf("mov %s, [%s + %d]\n", RegisterStr, RegMemStr, Displacement);
        } else {
            printf("mov [%s + %d], %s\n", RegMemStr, Displacement, RegisterStr);
        }
        return 4;
    }
    case RegisterMode: {
        const char *RegMemStr = GetRegisterStr(Mod.Fields.RegMem, Opcode.Fields.Word);
        const char *RegisterStr = GetRegisterStr(Mod.Fields.Register, Opcode.Fields.Word);
        if (Opcode.Fields.Direction) {
            printf("mov %s, %s\n", RegisterStr, RegMemStr);
        } else {
            printf("mov %s, %s\n", RegMemStr, RegisterStr);
        }
        return 2;
    }
    default:
        printf("[%s] ERROR: Can't do register mode %u at byte 0x%x.\n", __func__, Mod.Fields.Mode, ip + 1);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR: Must provide path to input bin file on commandline.\n");
    }

    FileInfo_t FileInfo = LoadBin(argv[1]);
    uint16_t ip = 0;
    while (ip < FileInfo.FileSize) {
        uint8_t OpcodeByte = GetByteFromBin(FileInfo, ip);

        // Table 4-12
        const uint8_t ImmediateToRegisterMask = 0xF0;
        const uint8_t ImmediateToRegisterOpcode = 0xB0;
        const uint8_t RegisterMemToFromRegisterMask = 0xFC;
        const uint8_t RegisterMemToFromRegisterOpcode = 0x88;

        // Must check for immediate first in case other opcodes would alias to this one because so few bits
        // are checked.
        if ((OpcodeByte & ImmediateToRegisterMask) == ImmediateToRegisterOpcode) {
            ip += ImmediateToRegister(ip, OpcodeByte, FileInfo);
        } else if ((OpcodeByte & RegisterMemToFromRegisterMask) == RegisterMemToFromRegisterOpcode) {
            ip += RegisterMemToFromRegister(ip, OpcodeByte, FileInfo);
        }
    }
    free((void *)FileInfo.Bin);

    return 0;
}
