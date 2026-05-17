#include <gtest/gtest.h>
#include "lexer.hpp"
#include "parser.hpp"
#include "emitter.hpp"
#include <fstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

using namespace cobolv;

#ifndef _MSC_VER
#define _popen popen
#define _pclose pclose
#endif

// Helper to run a command and return output + exit code
struct CommandResult {
    std::string output;
    int exitCode;
};

CommandResult runCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // Redirect stderr to stdout to capture everything
    std::string finalCmd = cmd + " 2>&1";
    FILE* pipe = _popen(finalCmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    int rc = _pclose(pipe);
    return {result, rc};
}

TEST(EmitterTest, EmitValidMinimalShader) {
    const std::string source = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. EMIT-TEST.
PROCEDURE DIVISION.
    MAIN-LOGIC.
        GOBACK.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    Emitter emitter;
    auto binary = emitter.emit(*program);

    // Write binary to temporary file
    std::string filename = "test_output.spv";
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(binary.data()), binary.size() * sizeof(uint32_t));
    out.close();

    // 1. Validate with spirv-val
    auto valResult = runCommand("spirv-val --target-env vulkan1.1 " + filename);
    EXPECT_EQ(valResult.exitCode, 0) << "spirv-val failed: " << valResult.output;

    // 2. Disassemble and check content
    auto disResult = runCommand("spirv-dis " + filename);
    EXPECT_EQ(disResult.exitCode, 0) << "spirv-dis failed: " << disResult.output;
    
    EXPECT_NE(disResult.output.find("OpCapability Shader"), std::string::npos);
    EXPECT_NE(disResult.output.find("OpMemoryModel Logical GLSL450"), std::string::npos);
    EXPECT_NE(disResult.output.find("OpEntryPoint GLCompute"), std::string::npos);
    EXPECT_NE(disResult.output.find("OpExecutionMode"), std::string::npos);
    EXPECT_NE(disResult.output.find("OpReturn"), std::string::npos);
    
    // Cleanup
    std::remove(filename.c_str());
}

TEST(EmitterTest, EmitDebugMinimalShader) {
    const std::string source = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. DEBUG-TEST.
DATA DIVISION.
WORKING-STORAGE SECTION.
01 MY-VAR PIC I.
PROCEDURE DIVISION.
    MAIN-LOGIC.
        MOVE 42 TO MY-VAR.
        GOBACK.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    Emitter emitter;
    auto binary = emitter.emit(*program, {}, true, "debug_test.cob");

    // Write binary to temporary file
    std::string filename = "test_debug_output.spv";
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(binary.data()), binary.size() * sizeof(uint32_t));
    out.close();

    // 1. Validate with spirv-val
    auto valResult = runCommand("spirv-val --target-env vulkan1.1 " + filename);
    EXPECT_EQ(valResult.exitCode, 0) << "spirv-val failed: " << valResult.output;

    // 2. Disassemble and check content
    auto disResult = runCommand("spirv-dis " + filename);
    EXPECT_EQ(disResult.exitCode, 0) << "spirv-dis failed: " << disResult.output;
    
    EXPECT_NE(disResult.output.find("OpString \"debug_test.cob\""), std::string::npos);
    EXPECT_NE(disResult.output.find("NonSemantic.Shader.DebugInfo.100"), std::string::npos);
    EXPECT_NE(disResult.output.find("DebugSource"), std::string::npos);
    EXPECT_NE(disResult.output.find("DebugLine"), std::string::npos);
    EXPECT_NE(disResult.output.find("OpName"), std::string::npos);
    
    // Cleanup
    std::remove(filename.c_str());
}
