#include <gtest/gtest.h>
#include "lexer.hpp"
#include <algorithm>

using namespace cobolv;

// A minimal but realistic COBOL-V shader snippet:
//
//   IDENTIFICATION DIVISION.
//   PROGRAM-ID. BRIGHTNESS-FILTER.
//   DATA DIVISION.
//   PROCEDURE DIVISION.
//       GOBACK.
//
// This exercises division/section keywords, identifiers, periods and GOBACK.

static const std::string k_SimpleShader = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. BRIGHTNESS-FILTER.
DATA DIVISION.
PROCEDURE DIVISION.
    MAIN-LOGIC.
        GOBACK.
)";

TEST(LexerTest, TokenizeMinimalShader) {
    Lexer lexer(k_SimpleShader);
    auto tokens = lexer.tokenize();

    // Remove the trailing EOF for cleaner indexing
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
    tokens.pop_back();

    // Build the expected sequence
    struct Expected { TokenType type; std::string lexeme; };
    const std::vector<Expected> expected = {
        // IDENTIFICATION DIVISION.
        { TokenType::IDENTIFICATION,  "IDENTIFICATION"     },
        { TokenType::DIVISION,        "DIVISION"           },
        { TokenType::PERIOD,          "."                  },
        // PROGRAM-ID. BRIGHTNESS-FILTER.
        { TokenType::PROGRAM_ID,      "PROGRAM-ID"         },
        { TokenType::PERIOD,          "."                  },
        { TokenType::IDENTIFIER,      "BRIGHTNESS-FILTER"  },
        { TokenType::PERIOD,          "."                  },
        // DATA DIVISION.
        { TokenType::DATA,            "DATA"               },
        { TokenType::DIVISION,        "DIVISION"           },
        { TokenType::PERIOD,          "."                  },
        // PROCEDURE DIVISION.
        { TokenType::PROCEDURE,       "PROCEDURE"          },
        { TokenType::DIVISION,        "DIVISION"           },
        { TokenType::PERIOD,          "."                  },
        // MAIN-LOGIC.
        { TokenType::IDENTIFIER,      "MAIN-LOGIC"         },
        { TokenType::PERIOD,          "."                  },
        // GOBACK.
        { TokenType::GOBACK,          "GOBACK"             },
        { TokenType::PERIOD,          "."                  },
    };

    ASSERT_EQ(tokens.size(), expected.size());

    for (size_t i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE("Token index " + std::to_string(i));
        EXPECT_EQ(tokens[i].type, expected[i].type);
        EXPECT_EQ(tokens[i].lexeme, expected[i].lexeme);
    }
}

TEST(LexerTest, LineNumbers) {
    Lexer lexer(k_SimpleShader);
    auto tokens = lexer.tokenize();
    tokens.pop_back(); // remove EOF

    // IDENTIFICATION is the first real token — source starts with a blank line,
    // so it should be on line 2.
    EXPECT_EQ(tokens[0].lexeme, "IDENTIFICATION");
    EXPECT_EQ(tokens[0].line, 2);

    // GOBACK is on line 7.
    auto it = std::find_if(tokens.begin(), tokens.end(),
        [](const Token& t){ return t.type == TokenType::GOBACK; });
    ASSERT_NE(it, tokens.end());
    EXPECT_EQ(it->line, 7);
}

TEST(LexerTest, DataDeclarations) {
    const std::string source = R"(
DATA DIVISION.
WORKING-STORAGE SECTION.
    01  UV-COORDS           PIC V(2).
    01  LUMINANCE           PIC FLOAT.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tokens.pop_back(); // remove EOF

    struct Expected { TokenType type; std::string lexeme; };
    const std::vector<Expected> expected = {
        { TokenType::DATA,            "DATA"            },
        { TokenType::DIVISION,        "DIVISION"        },
        { TokenType::PERIOD,          "."               },
        { TokenType::WORKING_STORAGE, "WORKING-STORAGE" },
        { TokenType::SECTION,         "SECTION"         },
        { TokenType::PERIOD,          "."               },
        
        { TokenType::NUMBER,          "01"              },
        { TokenType::IDENTIFIER,      "UV-COORDS"       },
        { TokenType::PIC,             "PIC"             },
        { TokenType::V,               "V"               },
        { TokenType::LPAREN,          "("               },
        { TokenType::NUMBER,          "2"               },
        { TokenType::RPAREN,          ")"               },
        { TokenType::PERIOD,          "."               },

        { TokenType::NUMBER,          "01"              },
        { TokenType::IDENTIFIER,      "LUMINANCE"       },
        { TokenType::PIC,             "PIC"             },
        { TokenType::FLOAT,           "FLOAT"           },
        { TokenType::PERIOD,          "."               },
    };

    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE("Token index " + std::to_string(i));
        EXPECT_EQ(tokens[i].type, expected[i].type);
        EXPECT_EQ(tokens[i].lexeme, expected[i].lexeme);
    }
}

TEST(LexerTest, ComplexDataTypes) {
    const std::string source = R"(
    01  INT-VEC   PIC IV(3).
    01  UINT-VEC  PIC UV(4).
    01  BOOL-VEC  PIC BV(2).
    01  MAT-SQ    PIC M(3).
    01  MAT-RECT  PIC M(4,3).
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tokens.pop_back(); // remove EOF

    struct Expected { TokenType type; std::string lexeme; };
    const std::vector<Expected> expected = {
        { TokenType::NUMBER,          "01"     },
        { TokenType::IDENTIFIER,      "INT-VEC"},
        { TokenType::PIC,             "PIC"    },
        { TokenType::IV,              "IV"     },
        { TokenType::LPAREN,          "("      },
        { TokenType::NUMBER,          "3"      },
        { TokenType::RPAREN,          ")"      },
        { TokenType::PERIOD,          "."      },

        { TokenType::NUMBER,          "01"      },
        { TokenType::IDENTIFIER,      "UINT-VEC"},
        { TokenType::PIC,             "PIC"     },
        { TokenType::UV,              "UV"      },
        { TokenType::LPAREN,          "("       },
        { TokenType::NUMBER,          "4"       },
        { TokenType::RPAREN,          ")"       },
        { TokenType::PERIOD,          "."       },

        { TokenType::NUMBER,          "01"      },
        { TokenType::IDENTIFIER,      "BOOL-VEC"},
        { TokenType::PIC,             "PIC"     },
        { TokenType::BV,              "BV"      },
        { TokenType::LPAREN,          "("       },
        { TokenType::NUMBER,          "2"       },
        { TokenType::RPAREN,          ")"       },
        { TokenType::PERIOD,          "."       },

        { TokenType::NUMBER,          "01"      },
        { TokenType::IDENTIFIER,      "MAT-SQ"  },
        { TokenType::PIC,             "PIC"     },
        { TokenType::M,               "M"       },
        { TokenType::LPAREN,          "("       },
        { TokenType::NUMBER,          "3"       },
        { TokenType::RPAREN,          ")"       },
        { TokenType::PERIOD,          "."       },

        { TokenType::NUMBER,          "01"      },
        { TokenType::IDENTIFIER,      "MAT-RECT"},
        { TokenType::PIC,             "PIC"     },
        { TokenType::M,               "M"       },
        { TokenType::LPAREN,          "("       },
        { TokenType::NUMBER,          "4"       },
        { TokenType::COMMA,           ","       },
        { TokenType::NUMBER,          "3"       },
        { TokenType::RPAREN,          ")"       },
        { TokenType::PERIOD,          "."       },
    };

    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE("Token index " + std::to_string(i));
        EXPECT_EQ(tokens[i].type, expected[i].type);
        EXPECT_EQ(tokens[i].lexeme, expected[i].lexeme);
    }
}
