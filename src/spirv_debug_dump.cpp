#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <cstring>
#include <sstream>

enum DebugOpcode {
    DebugInfoNone = 0,
    DebugCompilationUnit = 1,
    DebugTypeBasic = 2,
    DebugTypeVector = 6,
    DebugTypeComposite = 10,
    DebugGlobalVariable = 18,
    DebugFunction = 20,
    DebugLexicalBlock = 21,
    DebugScope = 23,
    DebugNoScope = 24,
    DebugSource = 35,
    DebugLine = 103,
    DebugEntryPoint = 107
};

std::string getSpirvString(const std::vector<uint32_t>& operands, size_t startIdx) {
    std::string str;
    for (size_t i = startIdx; i < operands.size(); ++i) {
        uint32_t word = operands[i];
        for (int byteIdx = 0; byteIdx < 4; ++byteIdx) {
            char c = static_cast<char>((word >> (byteIdx * 8)) & 0xFF);
            if (c == '\0') return str;
            str.push_back(c);
        }
    }
    return str;
}

std::string resolveId(uint32_t id,
                      const std::map<uint32_t, std::string>& strings,
                      const std::map<uint32_t, uint32_t>& constants,
                      const std::map<uint32_t, std::string>& names) {
    if (id == 0) return "None";
    if (names.count(id)) return names.at(id) + " (%" + std::to_string(id) + ")";
    if (strings.count(id)) {
        std::string s = strings.at(id);
        if (s.length() > 60) s = s.substr(0, 57) + "...";
        std::replace(s.begin(), s.end(), '\n', ' ');
        return "\"" + s + "\" (%" + std::to_string(id) + ")";
    }
    if (constants.count(id)) return std::to_string(constants.at(id));
    return "%" + std::to_string(id);
}

std::string formatDebugInstruction(uint32_t id,
                                   const std::map<uint32_t, uint32_t>& debugOpcode,
                                   const std::map<uint32_t, std::vector<uint32_t>>& debugInstructions,
                                   const std::map<uint32_t, std::string>& strings,
                                   const std::map<uint32_t, uint32_t>& constants,
                                   const std::map<uint32_t, std::string>& names) {
    if (id == 0) return "DebugInfoNone";
    if (!debugOpcode.count(id)) {
        return resolveId(id, strings, constants, names);
    }

    uint32_t op = debugOpcode.at(id);
    const auto& operands = debugInstructions.at(id);

    switch (op) {
        case 0: return "DebugInfoNone";
        case 1: // DebugCompilationUnit
            return "DebugCompilationUnit(lang: " + resolveId(operands[0], strings, constants, names) +
                   ", ver: " + resolveId(operands[1], strings, constants, names) +
                   ", source: %" + std::to_string(operands[2]) + ")";
        case 2: // DebugTypeBasic
            return "DebugTypeBasic(name: " + resolveId(operands[0], strings, constants, names) +
                   ", size: " + resolveId(operands[1], strings, constants, names) +
                   ", encoding: " + resolveId(operands[2], strings, constants, names) + ")";
        case 6: // DebugTypeVector
            return "DebugTypeVector(base: %" + std::to_string(operands[0]) +
                   ", count: " + resolveId(operands[1], strings, constants, names) + ")";
        case 10: // DebugTypeComposite
            return "DebugTypeComposite(name: " + resolveId(operands[0], strings, constants, names) +
                   ", tag: " + resolveId(operands[1], strings, constants, names) +
                   ", parent: %" + std::to_string(operands[5]) + ")";
        case 18: // DebugGlobalVariable
            return "DebugGlobalVariable(name: " + resolveId(operands[0], strings, constants, names) +
                   ", type: %" + std::to_string(operands[1]) +
                   ", var: " + resolveId(operands[7], strings, constants, names) + ")";
        case 20: // DebugFunction
            return "DebugFunction(name: " + resolveId(operands[0], strings, constants, names) +
                   ", type: %" + std::to_string(operands[1]) +
                   ", line: " + resolveId(operands[3], strings, constants, names) + ")";
        case 21: // DebugLexicalBlock
            return "DebugLexicalBlock(line: " + resolveId(operands[1], strings, constants, names) +
                   ", parent: %" + std::to_string(operands[3]) +
                   (operands.size() > 4 ? (", name: " + resolveId(operands[4], strings, constants, names)) : "") + ")";
        case 35: // DebugSource
            return "DebugSource(file: " + resolveId(operands[0], strings, constants, names) + ")";
        case 103: // DebugLine
            return "DebugLine(lineStart: " + resolveId(operands[1], strings, constants, names) +
                   ", colStart: " + resolveId(operands[3], strings, constants, names) + ")";
        case 107: // DebugEntryPoint
            return "DebugEntryPoint(func: %" + std::to_string(operands[0]) +
                   ", unit: %" + std::to_string(operands[1]) +
                   ", signature: " + resolveId(operands[2], strings, constants, names) + ")";
        default:
            return "DebugExtInst_" + std::to_string(op);
    }
}

void printTree(uint32_t id, const std::string& indent, bool isLast,
               const std::map<uint32_t, std::vector<uint32_t>>& children,
               const std::map<uint32_t, uint32_t>& debugOpcode,
               const std::map<uint32_t, std::vector<uint32_t>>& debugInstructions,
               const std::map<uint32_t, std::string>& strings,
               const std::map<uint32_t, uint32_t>& constants,
               const std::map<uint32_t, std::string>& names) {
    std::cout << indent;
    std::cout << "+- ";
    
    std::cout << formatDebugInstruction(id, debugOpcode, debugInstructions, strings, constants, names) << std::endl;
    
    if (children.count(id)) {
        const auto& childList = children.at(id);
        std::string newIndent = indent + (isLast ? "    " : "|   "); // Unicode branch bar
        for (size_t i = 0; i < childList.size(); ++i) {
            printTree(childList[i], newIndent, i == childList.size() - 1, children, debugOpcode, debugInstructions, strings, constants, names);
        }
    }
}

std::map<uint16_t, std::string> getOpcodeNames() {
    return {
        {19, "OpTypeVoid"},
        {20, "OpTypeBool"},
        {21, "OpTypeInt"},
        {22, "OpTypeFloat"},
        {23, "OpTypeVector"},
        {24, "OpTypeMatrix"},
        {32, "OpTypePointer"},
        {33, "OpTypeFunction"},
        {43, "OpConstant"},
        {54, "OpFunction"},
        {56, "OpFunctionEnd"},
        {59, "OpVariable"},
        {61, "OpLoad"},
        {62, "OpStore"},
        {65, "OpAccessChain"},
        {79, "OpVectorShuffle"},
        {80, "OpCompositeConstruct"},
        {81, "OpCompositeExtract"},
        {128, "OpIAdd"},
        {148, "OpMatrixTimesVector"},
        {248, "OpLabel"},
        {253, "OpReturn"}
    };
}

std::string formatInstruction(uint32_t opcode, uint32_t resultId, uint32_t resultType,
                              const std::vector<uint32_t>& operands,
                              const std::map<uint32_t, std::string>& strings,
                              const std::map<uint32_t, uint32_t>& constants,
                              const std::map<uint32_t, std::string>& names,
                              const std::map<uint16_t, std::string>& opcodeNames) {
    std::string opName = opcodeNames.count(opcode) ? opcodeNames.at(opcode) : "Opcode_" + std::to_string(opcode);
    std::string res;
    if (resultId != 0) {
        res += resolveId(resultId, strings, constants, names);
        if (resultType != 0) {
            res += " [" + resolveId(resultType, strings, constants, names) + "]";
        }
        res += " = ";
    }
    res += opName;
    if (!operands.empty()) {
        res += " ";
        for (size_t i = 0; i < operands.size(); ++i) {
            if (i > 0) res += ", ";
            res += resolveId(operands[i], strings, constants, names);
        }
    }
    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <spirv_file.spv>" << std::endl;
        return 1;
    }
    
    std::string filePath = argv[1];
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return 1;
    }
    
    std::vector<uint32_t> words;
    uint32_t word;
    while (file.read(reinterpret_cast<char*>(&word), 4)) {
        words.push_back(word);
    }
    
    if (words.size() < 5) {
        std::cerr << "Invalid SPIR-V file: too short" << std::endl;
        return 1;
    }
    
    if (words[0] != 0x07230203) {
        std::cerr << "Invalid SPIR-V file: magic number mismatch (got 0x" << std::hex << words[0] << ")" << std::endl;
        return 1;
    }
    
    std::map<uint32_t, std::string> strings;
    std::map<uint32_t, std::string> names;
    std::map<uint32_t, uint32_t> constants;
    std::map<uint32_t, uint32_t> debugOpcode;
    std::map<uint32_t, std::vector<uint32_t>> debugInstructions;
    uint32_t debugImportId = 0;
    
    // First pass: collect metadata definitions
    size_t i = 5;
    while (i < words.size()) {
        uint32_t word0 = words[i];
        uint16_t opcode = static_cast<uint16_t>(word0 & 0xFFFF);
        uint16_t wordCount = static_cast<uint16_t>((word0 >> 16) & 0xFFFF);
        if (wordCount == 0 || i + wordCount > words.size()) {
            std::cerr << "Malformed instruction at word " << i << std::endl;
            break;
        }
        
        std::vector<uint32_t> operands(words.begin() + i + 1, words.begin() + i + wordCount);
        
        if (opcode == 7) { // OpString
            if (!operands.empty()) {
                strings[operands[0]] = getSpirvString(operands, 1);
            }
        } else if (opcode == 11) { // OpExtInstImport
            if (!operands.empty()) {
                std::string name = getSpirvString(operands, 1);
                if (name == "NonSemantic.Shader.DebugInfo.100") {
                    debugImportId = operands[0];
                }
            }
        } else if (opcode == 5) { // OpName
            if (operands.size() >= 2) {
                names[operands[0]] = getSpirvString(operands, 1);
            }
        } else if (opcode == 43) { // OpConstant
            if (operands.size() >= 3) {
                constants[operands[1]] = operands[2];
            }
        } else if (opcode == 12) { // OpExtInst
            if (operands.size() >= 4) {
                uint32_t setId = operands[2];
                if (setId == debugImportId) {
                    uint32_t resId = operands[1];
                    uint32_t debugOp = operands[3];
                    debugOpcode[resId] = debugOp;
                    debugInstructions[resId] = std::vector<uint32_t>(operands.begin() + 4, operands.end());
                }
            }
        }
        
        i += wordCount;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  SPIR-V DEBUG INFO TREE VISUALIZATION" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "Parsed Debug Instructions:" << std::endl;
    for (const auto& pair : debugOpcode) {
        uint32_t id = pair.first;
        uint32_t op = pair.second;
        const auto& operands = debugInstructions.at(id);
        std::cout << "  %" << id << " = " << formatDebugInstruction(id, debugOpcode, debugInstructions, strings, constants, names)
                  << " (op: " << op << ", operands size: " << operands.size() << ")" << std::endl;
    }
    std::cout << "----------------------------------------\n" << std::endl;

    // Find all roots and children relationships
    std::map<uint32_t, std::vector<uint32_t>> children;
    std::vector<uint32_t> roots;
    
    for (const auto& pair : debugOpcode) {
        uint32_t id = pair.first;
        uint32_t op = pair.second;
        const auto& operands = debugInstructions.at(id);
        
        uint32_t parentId = 0;
        if (op == 20) { // DebugFunction
            if (operands.size() > 5) parentId = operands[5];
        } else if (op == 21) { // DebugLexicalBlock
            if (operands.size() > 3) parentId = operands[3];
        } else if (op == 18) { // DebugGlobalVariable
            if (operands.size() > 5) parentId = operands[5];
        } else if (op == 10) { // DebugTypeComposite
            if (operands.size() > 5) parentId = operands[5];
        }
        
        if (parentId != 0 && debugOpcode.count(parentId)) {
            children[parentId].push_back(id);
        } else if (op == 1) { // DebugCompilationUnit is root
            roots.push_back(id);
        }
    }
    
    if (roots.empty()) {
        std::cout << "(No DebugCompilationUnit roots found)" << std::endl;
    } else {
        for (size_t r = 0; r < roots.size(); ++r) {
            printTree(roots[r], "", r == roots.size() - 1, children, debugOpcode, debugInstructions, strings, constants, names);
        }
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  INSTRUCTION TO SOURCE CODE MAPPINGS" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Extract lines of the embedded source if available
    std::vector<std::string> sourceLines;
    for (const auto& pair : debugOpcode) {
        if (pair.second == 35) { // DebugSource
            const auto& operands = debugInstructions.at(pair.first);
            if (operands.size() > 1 && strings.count(operands[1])) {
                std::string rawSource = strings.at(operands[1]);
                std::stringstream ss(rawSource);
                std::string lineStr;
                while (std::getline(ss, lineStr)) {
                    sourceLines.push_back(lineStr);
                }
                break;
            }
        }
    }
    
    auto opcodeNames = getOpcodeNames();
    
    // Second pass: trace labels, scope changes, and physical execution instructions
    uint32_t activeScopeId = 0;
    uint32_t activeLineStart = 0;
    uint32_t lastPrintedLine = 0;
    
    i = 5;
    while (i < words.size()) {
        uint32_t word0 = words[i];
        uint16_t opcode = static_cast<uint16_t>(word0 & 0xFFFF);
        uint16_t wordCount = static_cast<uint16_t>((word0 >> 16) & 0xFFFF);
        
        std::vector<uint32_t> operands(words.begin() + i + 1, words.begin() + i + wordCount);
        
        if (opcode == 54) { // OpFunction
            uint32_t resId = operands[1];
            std::cout << "\n>>> Function " << resolveId(resId, strings, constants, names) << std::endl;
            activeScopeId = 0;
            activeLineStart = 0;
            lastPrintedLine = 0;
        } else if (opcode == 12) { // OpExtInst
            uint32_t setId = operands[2];
            if (setId == debugImportId) {
                uint32_t debugOp = operands[3];
                if (debugOp == 23) { // DebugScope
                    activeScopeId = operands[4];
                } else if (debugOp == 24) { // DebugNoScope
                    activeScopeId = 0;
                } else if (debugOp == 103) { // DebugLine
                    activeLineStart = constants.count(operands[5]) ? constants.at(operands[5]) : 0;
                }
            }
        } else if (opcode == 248) { // OpLabel
            std::cout << "  Label " << resolveId(operands[0], strings, constants, names) << ":" << std::endl;
        } else if (opcode != 56 && opcode != 19 && opcode != 20 && opcode != 21 && 
                   opcode != 22 && opcode != 23 && opcode != 24 && opcode != 32 && 
                   opcode != 33 && opcode != 43 && opcode != 59 && opcode != 5 && opcode != 7) {
            // This is an execution instruction!
            uint32_t resId = 0;
            uint32_t resType = 0;
            
            // Deduce ResultId and ResultType if applicable
            // For standard OpLoad, OpIAdd, OpAccessChain etc., operands[1] is result ID, operands[0] is result type ID.
            if (opcode == 61 || opcode == 65 || opcode == 79 || opcode == 80 || 
                opcode == 81 || opcode == 128 || opcode == 148) {
                resType = operands[0];
                resId = operands[1];
            }
            
            std::vector<uint32_t> printOperands;
            if (resId != 0) {
                printOperands = std::vector<uint32_t>(operands.begin() + 2, operands.end());
            } else {
                printOperands = operands;
            }
            
            // Print source line header if it changes
            if (activeLineStart != 0 && activeLineStart != lastPrintedLine) {
                std::cout << "    --------------------------------------------------" << std::endl;
                std::cout << "    [Line " << activeLineStart << "] ";
                if (activeLineStart - 1 < sourceLines.size()) {
                    std::cout << sourceLines[activeLineStart - 1];
                } else {
                    std::cout << "<source line unavailable>";
                }
                std::cout << std::endl;
                std::cout << "    --------------------------------------------------" << std::endl;
                lastPrintedLine = activeLineStart;
            }
            
            std::string formatted = formatInstruction(opcode, resId, resType, printOperands, strings, constants, names, opcodeNames);
            std::cout << "        " << formatted;
            if (activeScopeId != 0) {
                std::cout << "   ; Scope: %" << activeScopeId;
            }
            std::cout << std::endl;
        }
        
        i += wordCount;
    }
    
    std::cout << "========================================" << std::endl;
    return 0;
}
