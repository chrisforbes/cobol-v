#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <array>

namespace fs = std::filesystem;

#ifndef _MSC_VER
#define _popen popen
#define _pclose pclose
#endif

struct CommandResult {
    std::string output;
    int exitCode;
};

CommandResult runCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::string finalCmd = cmd + " 2>&1";
    FILE* pipe = _popen(finalCmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    int rc = _pclose(pipe);
    return {result, rc};
}

std::string readFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

class E2ETest : public ::testing::TestWithParam<fs::path> {};

TEST_P(E2ETest, RunTestCase) {
    fs::path cobFile = GetParam();
    fs::path errFile = cobFile;
    errFile.replace_extension(".err");
    fs::path disFile = cobFile;
    disFile.replace_extension(".dis");

    std::string cobPath = cobFile.string();
    
    // Run compiler
#ifdef _MSC_VER
    std::string compilerPath = ".\\build\\Debug\\cobolv.exe";
#else
    std::string compilerPath = "build/cobolv";
#endif

    auto compResult = runCommand(compilerPath + " " + cobPath);
    
    if (fs::exists(errFile)) {
        // Negative test case
        EXPECT_NE(compResult.exitCode, 0) << "Compiler should have failed for " << cobPath;
        std::string expectedErr = trim(readFile(errFile));
        EXPECT_TRUE(compResult.output.find(expectedErr) != std::string::npos) 
            << "Error message mismatch.\nExpected contains: " << expectedErr 
            << "\nActual output: " << compResult.output;
    } else if (fs::exists(disFile)) {
        // Positive test case
        ASSERT_EQ(compResult.exitCode, 0) << "Compiler failed for " << cobPath << ": " << compResult.output;

        // Validate with spirv-val, optionally with extra flags from a .valflags sidecar
        fs::path valflagsFile = cobFile;
        valflagsFile.replace_extension(".valflags");
        std::string extraValFlags = "";
        if (fs::exists(valflagsFile)) {
            extraValFlags = " " + trim(readFile(valflagsFile));
        }
        auto valResult = runCommand("spirv-val --target-env vulkan1.1" + extraValFlags + " output.spv");
        ASSERT_EQ(valResult.exitCode, 0) << "spirv-val failed: " << valResult.output;
        
        // Disassemble
        auto disResult = runCommand("spirv-dis output.spv");
        ASSERT_EQ(disResult.exitCode, 0) << "spirv-dis failed: " << disResult.output;
        
        std::string expectedDis = trim(readFile(disFile));
        std::vector<std::string> actualLines;
        std::stringstream ss(disResult.output);
        std::string line;
        while (std::getline(ss, line)) {
            line = trim(line);
            if (line.empty() || line[0] == ';') continue; // Skip comments/empty
            actualLines.push_back(line);
        }
        
        std::stringstream expectedSs(expectedDis);
        std::vector<std::string> expectedLines;
        while (std::getline(expectedSs, line)) {
            line = trim(line);
            if (line.empty() || line[0] == ';') continue;
            expectedLines.push_back(line);
        }

        ASSERT_EQ(actualLines.size(), expectedLines.size()) << "Disassembly line count mismatch";
        for (size_t i = 0; i < actualLines.size(); ++i) {
            EXPECT_EQ(actualLines[i], expectedLines[i]) << "Mismatch at line " << i;
        }

        // Run Amber if .amber file exists
        fs::path amberFile = cobFile;
        amberFile.replace_extension(".amber");
        if (fs::exists(amberFile)) {
#ifdef _MSC_VER
            std::string amberPath = ".\\build\\_deps\\amber-build\\Debug\\amber.exe";
#else
	    std::string amberPath = "amber -t vulkan1.1 ";
#endif
            fs::path spvDest = amberFile;
            spvDest.replace_extension(".spv");
            if (fs::exists("output.spv")) {
                fs::copy_file("output.spv", spvDest, fs::copy_options::overwrite_existing);
            }
            
            auto amberResult = runCommand(amberPath + " " + amberFile.string());
            ASSERT_EQ(amberResult.exitCode, 0) << "Amber failed for " << amberFile.string() << ": " << amberResult.output;
        }
    } else {
        ADD_FAILURE() << "No expected output file (.dis or .err) found for " << cobPath;
    }
    
    if (fs::exists("output.spv")) {
        fs::remove("output.spv");
    }
}

// Find all .cob files in tests/cases
std::vector<fs::path> getTestCases() {
    std::vector<fs::path> cases;
    fs::path casesDir = "tests/cases";
    if (fs::exists(casesDir)) {
        for (const auto& entry : fs::directory_iterator(casesDir)) {
            if (entry.path().extension() == ".cob") {
                cases.push_back(entry.path());
            }
        }
    }
    return cases;
}

INSTANTIATE_TEST_SUITE_P(
    AllCases,
    E2ETest,
    ::testing::ValuesIn(getTestCases())
);
