#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
using namespace std;

#define PRINT_DISASM_INSTR_STREAM

#ifdef PRINT_DISASM_INSTR_STREAM
#include <map>
#include <set>
map<int, string> g_labelMap;
#endif

bool g_data_hazard_detected = false;
bool g_control_hazard_detected = false;

// Function codes or opcode values
constexpr uint32_t ADD = 0x20;
constexpr uint32_t SUB = 0x22;
constexpr uint32_t LB = 0x20;
constexpr uint32_t SB = 0x28;
constexpr uint32_t BEQ = 0x04;

// bitmask with shift
struct Mask
{
    uint32_t bits;
    uint8_t shift;
};

const Mask opcodeMask = { 0xFC000000, 26 };
const Mask src1Mask = { 0x03E00000, 21 };
const Mask src2Mask = { 0x001F0000, 16 };
const Mask rDestMask = { 0x0000F800, 11 };
const Mask rFuncMask = { 0x0000003F,  0 };
const Mask iConstMask = { 0x0000FFFF,  0 };

// File name for input
string inputFilename = "input.txt";

uint32_t maskAndShift(const Mask& mask, uint32_t value)
{
    return (value & mask.bits) >> mask.shift;
}

int16_t maskAndShiftShort(const Mask& mask, int16_t inputBits)
{
    return (inputBits & static_cast<int16_t>(mask.bits)) >> mask.shift;
}

// Normal Registers (32 32-bit integers)
using Registers = array<int32_t, 32>;
Registers Regs = {};

// Pipeline Registers (IF/ID)
struct IFIDReg
{
    uint32_t Inst = 0;
};

IFIDReg W_IFID;
IFIDReg R_IFID;

// Pipeline Registers (ID/EX)
struct IDEXReg
{
    bool RegDst = false;
    bool ALUSrc = false;
    uint32_t ALUOp = 0;
    bool MemRead = false;
    bool MemWrite = false;
    bool Branch = false;
    bool MemToReg = false;
    bool RegWrite = false;

    uint32_t RFunctionBits = 0;

    int32_t ReadReg1Value = 0;
    int32_t ReadReg2Value = 0;
    int32_t SEOffset = 0;

    uint32_t WriteRegNumR = 0;
    uint32_t WriteRegNumAlt = 0;
    uint32_t BranchTarget = 0;

    uint32_t rs = 0;
    uint32_t rt = 0;
};

IDEXReg W_IDEX;
IDEXReg R_IDEX;

// Pipeline Registers (EX/MEM)
struct EXMEMReg
{
    bool MemRead = false;
    bool MemWrite = false;
    bool Branch = false;
    bool MemToReg = false;
    bool RegWrite = false;

    bool isZero = false;
    int32_t ALUResult = 0;
    int32_t SWValue = 0;
    uint32_t WriteRegNum = 0;
    uint32_t BranchTarget = 0;
};

EXMEMReg W_EXMEM;
EXMEMReg R_EXMEM;

// Pipeline Registers (MEM/WB)
struct MEMWBReg
{
    bool MemToReg = false;
    bool RegWrite = false;

    uint8_t  LBDataValue = 0;
    int32_t  ALUResult = 0;
    uint32_t WriteRegNum = 0;
};

MEMWBReg W_MEMWB;
MEMWBReg R_MEMWB;

// Program Counter (index into instruction cache)
int PC = 0;

// Raw hex instruction input
vector<uint32_t> InstructionCache;


// Disassembleable Interface for instructions (R&I)
class Disassembleable
{
public:
    virtual void disassemble() = 0;
    virtual bool isRFormat() = 0;
    virtual ~Disassembleable() = default;
};

struct RInstruction : public Disassembleable
{
    uint32_t bits;
    std::string instruction;
    uint32_t opcode;
    uint32_t src1, src2, dest, function;

    RInstruction(uint32_t bits_, uint32_t opcode_) : bits(bits_), opcode(opcode_) {}

    void disassemble() override;
    bool isRFormat() override;
};

struct IInstruction : public Disassembleable
{
    uint32_t bits;
    std::string instruction;
    uint32_t opcode;
    uint32_t src1, src2;
    int16_t constant;

    IInstruction(uint32_t bits_, uint32_t opcode_) : bits(bits_), opcode(opcode_) {}

    void disassemble() override;
    bool isRFormat() override;
};
void RInstruction::disassemble()
{
    src1 = maskAndShift(src1Mask, bits);
    src2 = maskAndShift(src2Mask, bits);
    dest = maskAndShift(rDestMask, bits);
    function = maskAndShift(rFuncMask, bits);
}

bool RInstruction::isRFormat()
{
    return true;
}
void IInstruction::disassemble()
{
    src1 = maskAndShift(src1Mask, bits);
    src2 = maskAndShift(src2Mask, bits);
    constant = maskAndShiftShort(iConstMask, static_cast<int16_t>(bits));
}

bool IInstruction::isRFormat()
{
    return false;
}
std::unique_ptr<Disassembleable> decodeInstruction(uint32_t input)
{
    uint32_t opcode = maskAndShift(opcodeMask, input);

    if (opcode == 0)
    {
        return std::make_unique<RInstruction>(input, opcode);
    }
    else
    {
        return std::make_unique<IInstruction>(input, opcode);
    }
}

// Instruction counter
int InstIdx = 0;

using Byte = uint8_t;
constexpr int MEMORY_SIZE = 1024;
std::array<Byte, MEMORY_SIZE> Main_Memory = { 0 };

void initMainMemory()
{
    Byte inc = 0x00;
    for (int i = 0; i < MEMORY_SIZE; ++i)
    {
        Main_Memory[i] = inc;
        if (inc == 0xFF)
        {
            inc = 0x00;
            continue;
        }
        ++inc;
    }
}

void initPipelineRegs()
{
    W_IFID = IFIDReg{};
    W_IDEX = IDEXReg{};
    W_EXMEM = EXMEMReg{};
    W_MEMWB = MEMWBReg{};

    R_IFID = IFIDReg{};
    R_IDEX = IDEXReg{};
    R_EXMEM = EXMEMReg{};
    R_MEMWB = MEMWBReg{};
}

void initRegs()
{
    Regs[0] = 0x0;
    for (int32_t i = 1; i < 32; ++i)
    {
        Regs[i] = 0x100 + i;
    }
}

void initInstructionCache(const std::vector<uint32_t>& instructions)
{
    InstructionCache = instructions;
    PC = 0;
    InstIdx = 0;
}

void initialize(const std::vector<uint32_t>& inst)
{
    // reset the hazard flags for each new instruction set
    g_data_hazard_detected = false;
    g_control_hazard_detected = false;
    initMainMemory();
    initPipelineRegs();
    initRegs();
    initInstructionCache(inst);
}

void printRegs()
{
    for (int i = 0; i < 32; ++i)
    {
        cout << "[$" << setw(2) << setfill('0') << i << "]: "
            << "0x" << hex << uppercase << setw(3) << Regs[i] << dec << '\n';
    }
}

#ifdef PRINT_DISASM_INSTR_STREAM
std::string formatInstruction(uint32_t hex_inst, int current_index)
{
    std::stringstream ss;

    auto decoded = decodeInstruction(hex_inst);
    decoded->disassemble();

    if (decoded->isRFormat())
    {
        RInstruction* r_inst = static_cast<RInstruction*>(decoded.get());
        if (r_inst->bits == 0)
        {
            ss << "nop";
        }
        else
        {
            switch (r_inst->function)
            {
            case ADD:
                ss << "add  $" << r_inst->dest << ", $" << r_inst->src1 << ", $" << r_inst->src2; break;
            case SUB:
                ss << "sub  $" << r_inst->dest << ", $" << r_inst->src1 << ", $" << r_inst->src2; break;
            default:
                ss << "unknown R-type (func: 0x" << std::hex << r_inst->function << ")"; break;
            }
        }
    }
    else
    {
        IInstruction* i_inst = static_cast<IInstruction*>(decoded.get());
        switch (i_inst->opcode)
        {
        case LB:
            ss << "lb   $" << i_inst->src2 << ", " << i_inst->constant << "($" << i_inst->src1 << ")";
            break;
        case SB:
            ss << "sb   $" << i_inst->src2 << ", " << i_inst->constant << "($" << i_inst->src1 << ")";
            break;
        case BEQ: {
            int target_index = current_index + 1 + i_inst->constant;
            ss << "beq  $" << i_inst->src1 << ", $" << i_inst->src2 << ", ";
            // use the label if it exists, otherwise fall back to the raw offset
            if (g_labelMap.count(target_index))
            {
                ss << g_labelMap[target_index];
            }
            else
            {
                ss << i_inst->constant; // raw offset
            }
            break;
        }
        default:
            ss << "unknown I-type (op: 0x" << std::hex << i_inst->opcode << ")";
            break;
        }
    }
    return ss.str();
}

void discoverLabels(const vector<uint32_t>& instructions)
{
    g_labelMap.clear(); // clear old labels for the new instruction set
    int label_counter = 0;

    for (int i = 0; i < instructions.size(); ++i)
    {
        uint32_t hex_inst = instructions[i];
        uint32_t opcode = maskAndShift(opcodeMask, hex_inst);

        if (opcode == BEQ)
        {
            // it's a branch, so calculate its target address
            int16_t offset = static_cast<int16_t>(hex_inst & 0xFFFF);
            int target_index = i + 1 + offset;

            // if the target is valid and doesn't already have a label, assign one
            if (target_index >= 0 && target_index < instructions.size())
            {
                if (g_labelMap.find(target_index) == g_labelMap.end())
                {
                    g_labelMap[target_index] = "L" + to_string(label_counter++);
                }
            }
        }
    }
}
#endif

void print_init()
{
    #ifdef PRINT_DISASM_INSTR_STREAM
    cout << "-----------------------------------------------------------\n";
    cout << "INSTRUCTION STREAM\n";

    // find all branch targets and create labels for them
    discoverLabels(InstructionCache);

    // print the instructions, checking for labels at each lin
    for (int i = 0; i < InstructionCache.size(); ++i)
    {
        // before printing the disasm instr, check if the current instruction index is a target of a branch
        // if so, print the label on its own line before the instruction
        if (g_labelMap.count(i))
        {
            cout << g_labelMap[i] << ":" << endl;
        }

        // print the instruction in hex format
        uint32_t hex_inst = InstructionCache[i];

        // indent the instruction line for clarity
        cout << "\t"
            << "0x" << hex << uppercase << setw(8) << setfill('0') << hex_inst << "\t->\t"
            << formatInstruction(hex_inst, i) << dec << endl;
    }
    #endif
    cout << "-----------------------------------------------------------\n";
    cout << "INITIAL REGISTER VALUES\n";
    printRegs();
    cout << "-----------------------------------------------------------\n";
}

void print_final()
{
    if (g_data_hazard_detected)
    {
        cout << "[DATA HAZARD] detected.\n";
    }
    if (g_control_hazard_detected)
    {
        cout << "[CONTROL HAZARD] detected.\n";
    }
    cout << "FINAL REGISTER VALUES\n";
    printRegs();
    cout << "===========================================================\n";
}

// Instruction Fetch Stage
void IF_Stage()
{
    // Implement here
}

// Instruction Decode Stage
void ID_Stage()
{
    // Implement here
}

// Execute Stage
void EX_Stage()
{
    // Implement here
}

// Memory Stage
void MEM_Stage()
{
    // Implement here
}

void WB_Stage()
{
    // Implement here
}

// Function to advance the pipeline registers to the next stage
void advanceRegisters()
{
    R_IFID = W_IFID;
    R_IDEX = W_IDEX;
    R_EXMEM = W_EXMEM;
    R_MEMWB = W_MEMWB;
}

// Function to load instruction sets from a file
vector<vector<uint32_t>> loadInstructionSetsWithCount(const std::string& filename)
{
    std::ifstream infile(filename);
    if (!infile.is_open())
    {
        cerr << "Could not open file: " << filename << endl;
        exit(1);
    }

    size_t expectedSetCount = 0;
    vector<size_t> instructionsPerSet;
    vector<vector<uint32_t>> allSets;
    std::string line;

    if (std::getline(infile, line))
    {
        try
        {
            expectedSetCount = std::stoul(line);
        }
        catch (...)
        {
            cerr << "Invalid set count at the top of the file.\n";
            exit(1);
        }
    }
    else
    {
        cerr << "Empty file.\n";
        exit(1);
    }

    for (size_t i = 0; i < expectedSetCount; ++i)
    {
        if (std::getline(infile, line))
        {
            try
            {
                size_t count = std::stoul(line);
                instructionsPerSet.push_back(count);
            }
            catch (...)
            {
                cerr << "Invalid instruction count for set #" << (i + 1) << ".\n";
                exit(1);
            }
        }
        else
        {
            cerr << "Missing instruction count for set #" << (i + 1) << ".\n";
            exit(1);
        }
    }

    for (size_t i = 0; i < expectedSetCount; ++i)
    {
        vector<uint32_t> currentSet;
        size_t remaining = instructionsPerSet[i];

        while (remaining > 0 && std::getline(infile, line))
        {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;

            uint32_t instr = 0;
            try
            {
                instr = std::stoul(line, nullptr, 16);
            }
            catch (...)
            {
                cerr << "Invalid instruction format in set #" << (i + 1) << ".\n";
                exit(1);
            }

            currentSet.push_back(instr);
            --remaining;
        }

        if (currentSet.size() != instructionsPerSet[i])
        {
            cerr << "Warning: Expected " << instructionsPerSet[i]
                << " instructions in set #" << (i + 1)
                << ", but got " << currentSet.size() << ".\n";
        }

        allSets.push_back(currentSet);
    }

    return allSets;
}


int main()
{
    auto instructionSets = loadInstructionSetsWithCount(inputFilename);


    int setNum = 1;
    for (const auto& set : instructionSets)
    {
        cout << "Instruction Set #" << setNum++ << "\n";
        initialize(set);
        print_init();
        for (size_t i = 0; i < InstructionCache.size(); ++i)
        {
            InstIdx++;
            IF_Stage();
            ID_Stage();
            EX_Stage();
            MEM_Stage();
            WB_Stage();
            advanceRegisters();
        }
        print_final();
    }
    return 0;
}
