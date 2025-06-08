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

// #define PRINT_DISASM_INSTR_STREAM
// #define PRINT_BRANCH_STATUS
// #define EXTENDED_INSTRUCTION_SET
// #define RUNTIME_DEBUG

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
#ifdef EXTENDED_INSTRUCTION_SET
constexpr uint32_t ADDI = 0x08;
#endif
// Bitmask with shift
struct Mask
{
    uint32_t bits;
    uint8_t shift;
};

const Mask opcodeMask = { 0xFC000000, 26 };
const Mask src1Mask = { 0x03E00000, 21 }; // rs
const Mask src2Mask = { 0x001F0000, 16 }; // rt
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
    bool RegWrite = false;
    bool ALUSrc = false;
    uint32_t ALUOp = 0;
    bool MemRead = false;
    bool MemWrite = false;
    bool MemToReg = false;
    bool Branch = false;

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
std::string formatInstruction(uint32_t inst_raw, int current_index)
{
    std::stringstream ss;

    auto decoded = decodeInstruction(inst_raw);
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
                #ifdef EXTENDED_INSTRUCTION_SET
        case ADDI:
            ss << "addi $" << i_inst->src2 << ", $" << i_inst->src1 << ", " << i_inst->constant;
            break;
            #endif
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
        uint32_t inst_raw = instructions[i];
        uint32_t opcode = maskAndShift(opcodeMask, inst_raw);

        if (opcode == BEQ)
        {
            // it's a branch, so calculate its target address
            int16_t offset = static_cast<int16_t>(inst_raw & 0xFFFF);
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
        uint32_t inst_raw = InstructionCache[i];

        cout << "[" << setw(3) << setfill(' ') << i << "] "
            << "\t"
            << "0x" << hex << uppercase << setw(8) << setfill('0') << inst_raw << "\t->\t"
            << formatInstruction(inst_raw, i) << dec << endl;
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
    // just in case we run out of instructions but the pipeline is still running
    if (PC < InstructionCache.size())
    {
        W_IFID.Inst = InstructionCache[PC];
    }
    else
    {
        W_IFID.Inst = 0;
    }

    PC++;
}

// Instruction Decode Stage
void ID_Stage()
{
    uint32_t inst_raw = R_IFID.Inst;

    W_IDEX = IDEXReg{}; // clear the output register

    #ifdef RUNTIME_DEBUG
    cout << "ID\n";
    #endif

    // if the instruction is 0, it is a nop
    if (inst_raw == 0)
    {
        #ifdef RUNTIME_DEBUG
        cout << "nop" << endl;
        #endif
        return;
    }

    auto decoded = decodeInstruction(inst_raw);

    decoded->disassemble();

    #ifdef RUNTIME_DEBUG
    // print the disassembled instruction
    if (decoded->isRFormat())
    {
        RInstruction* r_inst = static_cast<RInstruction*>(decoded.get());
        if (r_inst->bits == 0)
        {
            cout << "nop";
        }
        else
        {
            switch (r_inst->function)
            {
            case ADD:
                cout << "add  $" << r_inst->dest << ", $" << r_inst->src1 << ", $" << r_inst->src2; break;
            case SUB:
                cout << "sub  $" << r_inst->dest << ", $" << r_inst->src1 << ", $" << r_inst->src2; break;
            default:
                cout << "unknown R-type (func: 0x" << std::hex << r_inst->function << ")"; break;
            }
        }
    }
    else
    {
        IInstruction* i_inst = static_cast<IInstruction*>(decoded.get());
        switch (i_inst->opcode)
        {
        case LB:
            cout << "lb   $" << i_inst->src2 << ", " << i_inst->constant << "($" << i_inst->src1 << ")";
            break;
        case SB:
            cout << "sb   $" << i_inst->src2 << ", " << i_inst->constant << "($" << i_inst->src1 << ")";
            break;
        case BEQ: {
            int target_index = InstIdx + 1 + i_inst->constant;
            cout << "beq  $" << i_inst->src1 << ", $" << i_inst->src2 << ", ";
            // use the label if it exists, otherwise fall back to the raw offset
            if (g_labelMap.count(target_index))
            {
                cout << g_labelMap[target_index];
            }
            else
            {
                cout << i_inst->constant; // raw offset
            }
            break;
        }
                #ifdef EXTENDED_INSTRUCTION_SET
        case ADDI:
            cout << "addi $" << i_inst->src2 << ", $" << i_inst->src1 << ", " << i_inst->constant;
            break;
            #endif
        default:
            cout << "unknown I-type (op: 0x" << std::hex << i_inst->opcode << ")";
            break;
        }
    }
    cout << endl;
    #endif

    // common control signals for both R and I type instructions
    // extract the various fields from the instruction
    uint32_t opcode = maskAndShift(opcodeMask, inst_raw);
    uint32_t rs = maskAndShift(src1Mask, inst_raw);
    uint32_t rt = maskAndShift(src2Mask, inst_raw);
    uint32_t rd = maskAndShift(rDestMask, inst_raw);
    uint32_t function = maskAndShift(rFuncMask, inst_raw);
    uint32_t immediate16 = maskAndShift(iConstMask, inst_raw);

    if (R_IDEX.MemRead && (R_IDEX.WriteRegNumAlt == rs || R_IDEX.WriteRegNumAlt == rt))
    {
        g_data_hazard_detected = true; // a hazard existed

        // stall the pipeline
        // inject a nop into the pipeline
        W_IDEX = {};

        // prevent the pipeline from advancing by undoing the work of this cycle's IF_Stage
        PC--; // rewind PC to re-fetch the instruction that followed this one
        W_IFID = R_IFID; // keep the current stalled instruction in IF/ID to be re-decoded next cycle

        return; // stop processing this stage
    }

    // set the extracted values into the IDEX register
    W_IDEX.RFunctionBits = function; // for R type instructions, this is the function code

    W_IDEX.ReadReg1Value = Regs[rs]; // rs field is the first source register
    W_IDEX.ReadReg2Value = Regs[rt]; // rt field is the second source register
    W_IDEX.SEOffset = static_cast<int32_t>(static_cast<int16_t>(maskAndShift(iConstMask, inst_raw))); // sign extend immediate (shifts by 0 bits)

    W_IDEX.WriteRegNumR = rd; // rd field is the destination for R type instuctions
    W_IDEX.WriteRegNumAlt = rt; // rt is the destination for I type instructions (the alternative write register)
    W_IDEX.BranchTarget = immediate16; // pass the immediate value as the branch target offset

    // forward from MEM/WB to ID (MEM -> ID forward)
    // "Does the instruction in the WB stage want to write to a register that we need?"
    if (R_MEMWB.RegWrite && R_MEMWB.WriteRegNum != 0)
    {
        // if WB is writing to our rs
        if (R_MEMWB.WriteRegNum == rs)
        {
            W_IDEX.ReadReg1Value = R_MEMWB.MemToReg ? R_MEMWB.LBDataValue : R_MEMWB.ALUResult;
        }
        // if WB is writing to our rt
        if (R_MEMWB.WriteRegNum == rt)
        {
            W_IDEX.ReadReg2Value = R_MEMWB.MemToReg ? R_MEMWB.LBDataValue : R_MEMWB.ALUResult;
        }
    }

    W_IDEX.rs = rs; // store the rs register number
    W_IDEX.rt = rt; // store the rt register number

    // instruction is of R type
    if (decoded->isRFormat())
    {
        // set control signals for R type instructions
        W_IDEX.RegDst = true; // 0 for rt, 1 for rd
        W_IDEX.RegWrite = true; // R type instructions write to the register file
        W_IDEX.ALUSrc = false; // 0 for read data 2, 1 for immediate value
        W_IDEX.ALUOp = 0b10; // ALUOp for R type instructions is 0b10 (function code)
        W_IDEX.MemRead = false; // R type instructions do not read from memory
        W_IDEX.MemWrite = false; // R type instructions do not write to memory
        W_IDEX.MemToReg = false; // 0 for ALU result, 1 for memory data
        W_IDEX.Branch = false; // R type instructions do not branch
    }
    // instruction is of I type
    else
    {
        switch (opcode)
        {
        case LB:
            W_IDEX.RegDst = false; // 0 for rt, 1 for rd
            W_IDEX.RegWrite = true; // load byte instructions write to the register file
            W_IDEX.ALUSrc = true; // 0 for read data 2, 1 for immediate value
            W_IDEX.ALUOp = 0b00; // ALUOp for load/store instructions is 0b00 (add)
            W_IDEX.MemRead = true; // load byte instructions read from memory
            W_IDEX.MemWrite = false; // load byte instructions do not write to memory
            W_IDEX.MemToReg = true; // 0 for ALU result, 1 for memory data (load byte instructions write the loaded byte to the register)
            W_IDEX.Branch = false; // load byte instructions do not branch
            break;

        case SB:
            W_IDEX.RegDst = false; // so we don't care about this: we don't write to the register file for store byte instructions
            W_IDEX.RegWrite = false; // store byte instructions do not write to the register file
            W_IDEX.ALUSrc = true; // 0 for read data 2, 1 for immediate value
            W_IDEX.ALUOp = 0b00; // ALUOp for load/store instructions is 0b00 (add)
            W_IDEX.MemRead = false; // store byte instructions do not read from memory
            W_IDEX.MemWrite = true; // store byte instructions write to memory
            W_IDEX.MemToReg = false; // we don't care about this: we don't write to the register file for store byte instructions
            W_IDEX.Branch = false; // store byte instructions do not branch
            break;

        case BEQ:
            W_IDEX.RegDst = false; // so we don't care about this: we don't write to the register file for branch instructions
            W_IDEX.RegWrite = false; // branch instructions do not write to the register file
            W_IDEX.ALUSrc = false; // 0 for read data 2, 1 for immediate value (we use the second source register)
            W_IDEX.ALUOp = 0b01; // ALUOp for branch instructions is 0b01 (sub)
            W_IDEX.MemRead = false; // branch instructions do not read from memory
            W_IDEX.MemWrite = false; // branch instructions do not write to memory
            W_IDEX.MemToReg = false; // we don't care about this: we don't write to the register file for branch instructions
            W_IDEX.Branch = true; // branch instructions do branch
            break;

            #ifdef EXTENDED_INSTRUCTION_SET
        case ADDI:
            W_IDEX.RegDst = false; // 0 for rt, 1 for rd
            W_IDEX.RegWrite = true; // add immediate instructions write to the register file
            W_IDEX.ALUSrc = true; // 0 for read data 2, 1 for immediate value
            W_IDEX.ALUOp = 0b00; // ALUOp for addi instructions is 0b00 (add)
            W_IDEX.MemRead = false; // add immediate instructions do not read from memory
            W_IDEX.MemWrite = false; // add immediate instructions do not write to memory
            W_IDEX.MemToReg = false; // 0 for ALU result, 1 for memory data (add immediate instructions write the ALU result to the register)
            W_IDEX.Branch = false; // add immediate instructions do not branch
            break;
            #endif

        default:
            cerr << "unknown opcode: 0x" << hex << opcode << endl;
            exit(1);
        }
    }
}

// Execute Stage
void EX_Stage()
{
    // clear the output register
    W_EXMEM = {};

    // determine ALU inputs, start with the values from the read ID/EX register
    int32_t alu_input1 = R_IDEX.ReadReg1Value;
    int32_t alu_input2 = R_IDEX.ReadReg2Value;


    #ifdef RUNTIME_DEBUG
    cout << "EX\n";
    #endif

    // forwarding logic
    // This is the core of forwarding. We check for hazards and select the most recent data.
    // The order is important: check the WB stage first, then the MEM stage.

    // forward from MEM/WB to EX (MEM -> EX forward, memory to ALU forwarding)
    // "Does the instruction in the WB stage want to write to a register that we need?"
    if (R_MEMWB.RegWrite && R_MEMWB.WriteRegNum != 0)
    {
        if (R_MEMWB.WriteRegNum == R_IDEX.rs)
        {
            g_data_hazard_detected = true;
            alu_input1 = R_MEMWB.MemToReg ? R_MEMWB.LBDataValue : R_MEMWB.ALUResult;
            #ifdef RUNTIME_DEBUG
            cout << "mem->ex forward: " << alu_input1 << " from reg " << R_MEMWB.WriteRegNum << endl;
            cout << "alu_input1 got: " << alu_input1 << ", is value from " << (R_MEMWB.MemToReg ? "memory" : "ALU") << endl;
            #endif
        }
        if (R_MEMWB.WriteRegNum == R_IDEX.rt)
        {
            g_data_hazard_detected = true;
            alu_input2 = R_MEMWB.MemToReg ? R_MEMWB.LBDataValue : R_MEMWB.ALUResult;
            #ifdef RUNTIME_DEBUG
            cout << "mem->ex forward: " << alu_input2 << " from reg " << R_MEMWB.WriteRegNum << endl;
            cout << "alu_input2 got: " << alu_input2 << ", is value from " << (R_MEMWB.MemToReg ? "memory" : "ALU") << endl;
            #endif
        }
    }

    // forward from EX/MEM to EX (EX -> EX forward, ALU to ALU forwarding)
    // "If we didn't forward from WB, check if the instruction in the MEM stage can provide the data"
    // run this code second, so that we get fresher data if available
    if (R_EXMEM.RegWrite && R_EXMEM.WriteRegNum != 0)
    {
        if (R_EXMEM.WriteRegNum == R_IDEX.rs)
        {
            g_data_hazard_detected = true;
            alu_input1 = R_EXMEM.ALUResult;
            #ifdef RUNTIME_DEBUG
            cout << "\t\tex->ex forward: " << alu_input1 << " from reg " << R_EXMEM.WriteRegNum << endl;
            cout << "\t\talu_input1 got: " << alu_input1 << ", is value from ALU" << endl;
            #endif
        }
        if (R_EXMEM.WriteRegNum == R_IDEX.rt)
        {
            g_data_hazard_detected = true;
            alu_input2 = R_EXMEM.ALUResult;
            #ifdef RUNTIME_DEBUG
            cout << "\t\tex->ex forward: " << alu_input2 << " from reg " << R_EXMEM.WriteRegNum << endl;
            cout << "\t\talu_input2 got: " << alu_input2 << ", is value from ALU" << endl;
            #endif
        }
    }

    // select the second ALU operand (either a register value or the immediate)
    int32_t alu_operand2 = R_IDEX.ALUSrc ? R_IDEX.SEOffset : alu_input2;

    #ifdef RUNTIME_DEBUG
    cout << "alu_input1: " << alu_input1 << ", alu_operand2: " << alu_operand2 << " (from " << (R_IDEX.ALUSrc ? "immediate" : "register") << ")" << endl;
    cout << "ALUOp: " << R_IDEX.ALUOp << ", RFunctionBits: " << R_IDEX.RFunctionBits << endl;
    #endif

    // perform the ALU operation
    int32_t alu_result = 0;

    // add for lw/sw
    if (R_IDEX.ALUOp == 0b00)
    {
        alu_result = alu_input1 + alu_operand2;
    }
    // sub for beq
    else if (R_IDEX.ALUOp == 0b01)
    {
        alu_result = alu_input1 - alu_operand2;
    }
    // R type, use function code
    else if (R_IDEX.ALUOp == 0b10)
    {
        switch (R_IDEX.RFunctionBits)
        {
        case ADD: alu_result = alu_input1 + alu_operand2; break;
        case SUB: alu_result = alu_input1 - alu_operand2; break;
        default: cerr << "unknown R-type function code: 0x" << hex << R_IDEX.RFunctionBits << endl; exit(1);
        }
    }

    #ifdef RUNTIME_DEBUG
    cout << "ALU result: " << alu_result << endl;
    #endif

    // pass values to the EX/MEM pipeline register
    W_EXMEM.isZero = (alu_result == 0); // for branches and other things??
    W_EXMEM.ALUResult = alu_result; // ALU result to be used in the next stage
    W_EXMEM.SWValue = alu_input2; // value to be stored in memory for store instructions (rt is stored, so second source register is passed)
    W_EXMEM.WriteRegNum = R_IDEX.RegDst ? R_IDEX.WriteRegNumR : R_IDEX.WriteRegNumAlt; // write register number: rd for R type (R), rt for I type (Alternative)
    W_EXMEM.BranchTarget = R_IDEX.BranchTarget; // pass the branch target address

    // pass control signals through
    W_EXMEM.MemRead = R_IDEX.MemRead;
    W_EXMEM.MemWrite = R_IDEX.MemWrite;
    W_EXMEM.Branch = R_IDEX.Branch;
    W_EXMEM.MemToReg = R_IDEX.MemToReg;
    W_EXMEM.RegWrite = R_IDEX.RegWrite;
}

// Memory Stage
void MEM_Stage()
{
    W_MEMWB = MEMWBReg{}; // clear the output register

    // handle memory read
    if (R_EXMEM.MemRead)
    {
        // load byte instruction
        // read from memory at the address given by the ALU result
        if (R_EXMEM.ALUResult < 0 || R_EXMEM.ALUResult >= MEMORY_SIZE)
        {
            cerr << "memory read out of bounds: " << R_EXMEM.ALUResult << endl;
            exit(1);
        }
        W_MEMWB.LBDataValue = Main_Memory[R_EXMEM.ALUResult]; // load byte value from memory
    }

    // handle memory write
    if (R_EXMEM.MemWrite)
    {
        // store byte instruction
        // write to memory at the address given by the ALU result
        if (R_EXMEM.ALUResult < 0 || R_EXMEM.ALUResult >= MEMORY_SIZE)
        {
            cerr << "memory write out of bounds: " << R_EXMEM.ALUResult << endl;
            exit(1);
        }
        Main_Memory[R_EXMEM.ALUResult] = static_cast<uint8_t>(R_EXMEM.SWValue); // store byte value to memory
    }

    #ifdef PRINT_BRANCH_STATUS
    if (R_EXMEM.Branch)
    {
        cout << "branching instruction target address: " << PC - 3 + static_cast<int16_t>(R_EXMEM.BranchTarget)
            << (R_EXMEM.isZero ? " (taken)" : " (not taken)") << endl;
    }
    #endif

    // handle branch
    if (R_EXMEM.Branch && R_EXMEM.isZero)
    {
        // branch taken, set the PC to the branch target
        g_control_hazard_detected = true; // a control hazard occurred

        PC = PC - 3 + static_cast<int16_t>(R_EXMEM.BranchTarget); // branch target is an offset from the current PC (not sll by 2, because we are not using byte addressing)

        // flush the previous pipeline registers
        // R_IFID = IFIDReg{}; // clear the IF/ID register
        W_IFID = IFIDReg{}; // clear the IF/ID register
        // R_IDEX = IDEXReg{}; // clear the ID/EX register
        W_IDEX = IDEXReg{}; // clear the ID/EX register
        // R_EXMEM = EXMEMReg{}; // clear the EX/MEM register
        W_EXMEM = EXMEMReg{}; // clear the EX/MEM register
    }

    // pass values to the MEM/WB pipeline register
    W_MEMWB.MemToReg = R_EXMEM.MemToReg; // control signal for memory to register
    W_MEMWB.RegWrite = R_EXMEM.RegWrite; // control signal for register write

    W_MEMWB.ALUResult = R_EXMEM.ALUResult; // ALU result to be used in the next stage
    W_MEMWB.WriteRegNum = R_EXMEM.WriteRegNum; // write register number
}

void WB_Stage()
{
    // write back to the register file
    if (R_MEMWB.RegWrite && R_MEMWB.WriteRegNum != 0)
    {
        if (R_MEMWB.MemToReg)
        {
            // load byte instruction, write the loaded byte to the register
            Regs[R_MEMWB.WriteRegNum] = static_cast<int8_t>(R_MEMWB.LBDataValue);
        }
        else
        {
            // ALU result, write the ALU result to the register
            Regs[R_MEMWB.WriteRegNum] = R_MEMWB.ALUResult;
        }
    }
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
    #ifdef FILE_REDIRECT
    inputFilename = "debug_input.txt";
    #endif

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
