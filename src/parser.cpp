#include "parser.hpp"
#include <stdexcept>

namespace cobolv {

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    
    program->divisions.push_back(parseIdentificationDivision(*program));
    
    if (check(TokenType::ENVIRONMENT)) {
        program->divisions.push_back(parseEnvironmentDivision(*program));
    }

    if (check(TokenType::INPUT_OUTPUT)) {
        program->divisions.push_back(parseInputOutputSection());
    }

    if (check(TokenType::DATA)) {
        program->divisions.push_back(parseDataDivision());
    }
    
    program->divisions.push_back(parseProcedureDivision());
    
    if (!isAtEnd()) {
        throw std::runtime_error("Unexpected tokens at end of file: " + peek().lexeme + " at line " + std::to_string(peek().line));
    }

    return program;
}

std::unique_ptr<DivisionNode> Parser::parseIdentificationDivision(ProgramNode& program) {
    consume(TokenType::IDENTIFICATION, "Expected IDENTIFICATION");
    consume(TokenType::DIVISION, "Expected DIVISION");
    consume(TokenType::PERIOD, "Expected '.' after IDENTIFICATION DIVISION");
    
    auto div = std::make_unique<DivisionNode>();
    div->divisionType = TokenType::IDENTIFICATION;
    
    if (match(TokenType::PROGRAM_ID)) {
        consume(TokenType::PERIOD, "Expected '.' after PROGRAM-ID");
        program.programId = consumeIdentifier("Expected program name").lexeme;
        consume(TokenType::PERIOD, "Expected '.' after program name");
    }
    
    return div;
}

std::unique_ptr<DivisionNode> Parser::parseEnvironmentDivision(ProgramNode& program) {
    consume(TokenType::ENVIRONMENT, "Expected ENVIRONMENT");
    consume(TokenType::DIVISION, "Expected DIVISION");
    consume(TokenType::PERIOD, "Expected '.' after ENVIRONMENT DIVISION");

    auto div = std::make_unique<EnvironmentDivisionNode>();
    while (!isAtEnd() && !check(TokenType::INPUT_OUTPUT) && !check(TokenType::DATA) && !check(TokenType::PROCEDURE)) {
        if (match(TokenType::CONFIGURATION)) {
            div->sections.push_back(parseConfigurationSection(program));
        } else {
            advance();
        }
    }
    return div;
}

std::unique_ptr<SectionNode> Parser::parseConfigurationSection(ProgramNode& program) {
    consume(TokenType::SECTION, "Expected SECTION after CONFIGURATION");
    consume(TokenType::PERIOD, "Expected '.' after CONFIGURATION SECTION");

    auto section = std::make_unique<GenericSectionNode>(SectionType::CONFIGURATION);
    section->name = "CONFIGURATION";

    while (!isAtEnd() && !check(TokenType::INPUT_OUTPUT) && !check(TokenType::DATA) && !check(TokenType::PROCEDURE) && !check(TokenType::CONFIGURATION)) {
        if (match(TokenType::SPECIAL_NAMES)) {
            section->children.push_back(parseSpecialNames());
        } else if (match(TokenType::OBJECT_COMPUTER)) {
            consume(TokenType::PERIOD, "Expected '.' after OBJECT-COMPUTER");
            if (match(TokenType::VULKAN_VERTEX_SHADER)) program.shaderStage = ShaderStage::VERTEX;
            else if (match(TokenType::VULKAN_FRAGMENT_SHADER)) program.shaderStage = ShaderStage::FRAGMENT;
            else if (match(TokenType::VULKAN_COMPUTE_SHADER)) program.shaderStage = ShaderStage::COMPUTE;
            else throw std::runtime_error("Expected shader stage after OBJECT-COMPUTER");
            consume(TokenType::PERIOD, "Expected '.' after shader stage");
        } else {
            break;
        }
    }
    return section;
}

std::unique_ptr<SectionNode> Parser::parseSpecialNames() {
    consume(TokenType::PERIOD, "Expected '.' after SPECIAL-NAMES");
    auto node = std::make_unique<SpecialNamesNode>();

    while (!isAtEnd() && !check(TokenType::INPUT_OUTPUT) && !check(TokenType::DATA) && !check(TokenType::PROCEDURE) && !check(TokenType::SECTION) && !check(TokenType::NUMBER)) {
        if (match(TokenType::LOCAL_SIZE)) {
            consume(TokenType::IS, "Expected IS after LOCAL-SIZE");
            consume(TokenType::LPAREN, "Expected '('");
            node->localSize.x = std::stoi(consume(TokenType::NUMBER, "Expected X size").lexeme);
            consume(TokenType::COMMA, "Expected ','");
            node->localSize.y = std::stoi(consume(TokenType::NUMBER, "Expected Y size").lexeme);
            consume(TokenType::COMMA, "Expected ','");
            node->localSize.z = std::stoi(consume(TokenType::NUMBER, "Expected Z size").lexeme);
            consume(TokenType::RPAREN, "Expected ')'");
        } else if (checkIdentifier()) {
            std::string hardwareName = advance().lexeme;
            consume(TokenType::IS, "Expected IS");
            std::string cobolIdent = consumeIdentifier("Expected COBOL identifier").lexeme;
            node->builtInMappings[hardwareName] = cobolIdent;
            if (check(TokenType::PERIOD)) consume(TokenType::PERIOD, "Expected '.' after mapping");
        } else {
            break;
        }
    }
    return node;
}

std::unique_ptr<DivisionNode> Parser::parseInputOutputSection() {
    consume(TokenType::INPUT_OUTPUT, "Expected INPUT-OUTPUT");
    consume(TokenType::SECTION, "Expected SECTION after INPUT-OUTPUT");
    consume(TokenType::PERIOD, "Expected '.' after INPUT-OUTPUT SECTION");

    auto div = std::make_unique<DivisionNode>();
    div->divisionType = TokenType::INPUT_OUTPUT;

    while (!isAtEnd() && !check(TokenType::DATA) && !check(TokenType::PROCEDURE)) {
        if (match(TokenType::FILE_CONTROL)) {
            div->sections.push_back(parseFileControl());
        } else {
            advance();
        }
    }
    return div;
}

std::unique_ptr<SectionNode> Parser::parseFileControl() {
    consume(TokenType::PERIOD, "Expected '.' after FILE-CONTROL");
    auto section = std::make_unique<GenericSectionNode>(SectionType::INPUT_OUTPUT);
    section->name = "FILE-CONTROL";

    while (!isAtEnd() && !check(TokenType::DATA) && !check(TokenType::PROCEDURE) && !check(TokenType::SECTION)) {
        if (check(TokenType::SELECT)) {
            section->children.push_back(parseSelect());
        } else {
            break;
        }
    }
    return section;
}

std::unique_ptr<AstNode> Parser::parseSelect() {
    consume(TokenType::SELECT, "Expected SELECT");
    auto node = std::make_unique<SelectNode>();
    node->name = consumeIdentifier("Expected file name").lexeme;
    consume(TokenType::ASSIGN, "Expected ASSIGN");
    consume(TokenType::TO, "Expected TO");
    node->assignTo = consume(TokenType::STRING, "Expected assignment target (string)").lexeme;
    if (node->assignTo.front() == '"') node->assignTo = node->assignTo.substr(1, node->assignTo.length() - 2);

    if (match(TokenType::ORGANIZATION)) {
        consume(TokenType::IS, "Expected IS");
        if (match(TokenType::IMAGE_1D)) node->organization = OrganizationType::IMAGE_1D;
        else if (match(TokenType::IMAGE_2D)) node->organization = OrganizationType::IMAGE_2D;
        else if (match(TokenType::IMAGE_3D)) node->organization = OrganizationType::IMAGE_3D;
        else if (match(TokenType::IMAGE_1D_ARRAY)) node->organization = OrganizationType::IMAGE_1D_ARRAY;
        else if (match(TokenType::IMAGE_2D_ARRAY)) node->organization = OrganizationType::IMAGE_2D_ARRAY;
        else if (match(TokenType::IMAGE_CUBE)) node->organization = OrganizationType::IMAGE_CUBE;
        else if (match(TokenType::IMAGE_CUBE_ARRAY)) node->organization = OrganizationType::IMAGE_CUBE_ARRAY;
        else if (match(TokenType::TEXTURE_1D)) node->organization = OrganizationType::TEXTURE_1D;
        else if (match(TokenType::TEXTURE_2D)) node->organization = OrganizationType::TEXTURE_2D;
        else if (match(TokenType::TEXTURE_3D)) node->organization = OrganizationType::TEXTURE_3D;
        else if (match(TokenType::TEXTURE_1D_ARRAY)) node->organization = OrganizationType::TEXTURE_1D_ARRAY;
        else if (match(TokenType::TEXTURE_2D_ARRAY)) node->organization = OrganizationType::TEXTURE_2D_ARRAY;
        else if (match(TokenType::TEXTURE_CUBE)) node->organization = OrganizationType::TEXTURE_CUBE;
        else if (match(TokenType::TEXTURE_CUBE_ARRAY)) node->organization = OrganizationType::TEXTURE_CUBE_ARRAY;
        else if (match(TokenType::STORAGE_1D)) node->organization = OrganizationType::STORAGE_1D;
        else if (match(TokenType::STORAGE_2D)) node->organization = OrganizationType::STORAGE_2D;
        else if (match(TokenType::STORAGE_3D)) node->organization = OrganizationType::STORAGE_3D;
        else if (match(TokenType::STORAGE_1D_ARRAY)) node->organization = OrganizationType::STORAGE_1D_ARRAY;
        else if (match(TokenType::STORAGE_2D_ARRAY)) node->organization = OrganizationType::STORAGE_2D_ARRAY;
        else if (match(TokenType::STORAGE_CUBE)) node->organization = OrganizationType::STORAGE_CUBE;
        else if (match(TokenType::STORAGE_CUBE_ARRAY)) node->organization = OrganizationType::STORAGE_CUBE_ARRAY;
        else if (match(TokenType::SAMPLER)) node->organization = OrganizationType::SAMPLER;
        else if (match(TokenType::ACCESS_PUSH)) node->organization = OrganizationType::ACCESS_PUSH;
        else if (match(TokenType::ACCESS_UNIFORM)) node->organization = OrganizationType::ACCESS_UNIFORM;
        else if (match(TokenType::ACCESS_STORAGE)) node->organization = OrganizationType::ACCESS_STORAGE;
    }

    if (match(TokenType::ACCESS)) {
        consume(TokenType::MODE, "Expected MODE after ACCESS");
        consume(TokenType::IS, "Expected IS");
        if (match(TokenType::READ_ONLY)) node->accessMode = AccessMode::READ_ONLY;
        else if (match(TokenType::WRITE_ONLY)) node->accessMode = AccessMode::WRITE_ONLY;
        else if (match(TokenType::READ_WRITE)) node->accessMode = AccessMode::READ_WRITE;
    }

    consume(TokenType::PERIOD, "Expected '.' after SELECT");
    return node;
}

std::unique_ptr<DivisionNode> Parser::parseDataDivision() {
    consume(TokenType::DATA, "Expected DATA");
    consume(TokenType::DIVISION, "Expected DIVISION");
    consume(TokenType::PERIOD, "Expected '.' after DATA DIVISION");
    
    auto div = std::make_unique<DivisionNode>();
    div->divisionType = TokenType::DATA;
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE)) {
        if (match(TokenType::FILE_SECTION)) {
            div->sections.push_back(parseFileSection());
        } else if (match(TokenType::INPUT)) {
            div->sections.push_back(parseInputSection());
        } else if (match(TokenType::OUTPUT)) {
            div->sections.push_back(parseOutputSection());
        } else if (match(TokenType::WORKING_STORAGE)) {
            div->sections.push_back(parseWorkingStorageSection());
        } else if (match(TokenType::LOCAL_STORAGE)) {
            div->sections.push_back(parseLocalStorageSection());
        } else if (match(TokenType::LINKAGE)) {
            div->sections.push_back(parseLinkageSection());
        } else {
            advance();
        }
    }
    
    return div;
}

std::unique_ptr<SectionNode> Parser::parseFileSection() {
    consume(TokenType::SECTION, "Expected SECTION after FILE");
    consume(TokenType::PERIOD, "Expected '.' after FILE SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::FILE_SECTION);
    section->name = "FILE";
    
    while (!isAtEnd() && !check(TokenType::WORKING_STORAGE) && !check(TokenType::LINKAGE) && !check(TokenType::PROCEDURE)) {
        if (check(TokenType::FD)) {
            section->children.push_back(parseFileDescription());
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<FileDescriptionNode> Parser::parseFileDescription() {
    consume(TokenType::FD, "Expected FD");
    auto node = std::make_unique<FileDescriptionNode>();
    node->name = consumeIdentifier("Expected file name after FD").lexeme;
    bool sawPeriod = match(TokenType::PERIOD);

    while (!isAtEnd() && !check(TokenType::PERIOD) && !check(TokenType::NUMBER)) {
        sawPeriod = false;
        if (match(TokenType::FORMAT)) {
            consume(TokenType::IS, "Expected IS after FORMAT");
            node->format = consumeIdentifier("Expected format name").lexeme;
        } else if (match(TokenType::USAGE)) {
            consume(TokenType::IS, "Expected IS after USAGE");
            if (match(TokenType::COHERENT)) node->isCoherent = true;
            else if (match(TokenType::VOLATILE)) node->isVolatile = true;
        } else if (match(TokenType::LAYOUT)) {
            consume(TokenType::IS, "Expected IS after LAYOUT");
            if (match(TokenType::STD140)) node->layout = LayoutType::STD140;
            else if (match(TokenType::STD430)) node->layout = LayoutType::STD430;
            else if (match(TokenType::SCALAR)) node->layout = LayoutType::SCALAR;
            else throw std::runtime_error("Expected STD140, STD430, or SCALAR after LAYOUT IS");
        } else {
            break;
        }
    }
    
    if (!sawPeriod || check(TokenType::PERIOD)) {
        consume(TokenType::PERIOD, "Expected '.' after FD header");
    }

    while (!isAtEnd() && check(TokenType::NUMBER)) {
        node->records.push_back(parseDataItem());
    }
    return node;
}

std::unique_ptr<SectionNode> Parser::parseWorkingStorageSection() {
    consume(TokenType::SECTION, "Expected SECTION after WORKING-STORAGE");
    consume(TokenType::PERIOD, "Expected '.' after WORKING-STORAGE SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::WORKING_STORAGE);
    section->name = "WORKING-STORAGE";
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::WORKING_STORAGE) && !check(TokenType::LINKAGE) && !check(TokenType::DIVISION)) {
        if (check(TokenType::NUMBER)) {
            section->children.push_back(parseDataItem());
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<SectionNode> Parser::parseLocalStorageSection() {
    consume(TokenType::SECTION, "Expected SECTION after LOCAL-STORAGE");
    consume(TokenType::PERIOD, "Expected '.' after LOCAL-STORAGE SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::LOCAL_STORAGE);
    section->name = "LOCAL-STORAGE";
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::WORKING_STORAGE) && 
           !check(TokenType::LOCAL_STORAGE) && !check(TokenType::LINKAGE) && !check(TokenType::DIVISION)) {
        if (check(TokenType::NUMBER)) {
            section->children.push_back(parseDataItem());
        } else {
            break;
        }
    }
    
    return section;
}


std::unique_ptr<SectionNode> Parser::parseLinkageSection() {
    consume(TokenType::SECTION, "Expected SECTION after LINKAGE");
    consume(TokenType::PERIOD, "Expected '.' after LINKAGE SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::LINKAGE);
    section->name = "LINKAGE";
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::WORKING_STORAGE) && !check(TokenType::LINKAGE) && !check(TokenType::DIVISION)) {
        if (check(TokenType::NUMBER)) {
            section->children.push_back(parseDataItem());
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<SectionNode> Parser::parseInputSection() {
    consume(TokenType::SECTION, "Expected SECTION after INPUT");
    consume(TokenType::PERIOD, "Expected '.' after INPUT SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::INPUT_SECTION);
    section->name = "INPUT";
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::WORKING_STORAGE) && !check(TokenType::DIVISION)) {
        if (check(TokenType::NUMBER)) {
            auto item = parseDataItem();
            item->isInput = true;
            section->children.push_back(std::move(item));
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<SectionNode> Parser::parseOutputSection() {
    consume(TokenType::SECTION, "Expected SECTION after OUTPUT");
    consume(TokenType::PERIOD, "Expected '.' after OUTPUT SECTION");
    
    auto section = std::make_unique<GenericSectionNode>(SectionType::OUTPUT_SECTION);
    section->name = "OUTPUT";
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::WORKING_STORAGE) && !check(TokenType::DIVISION)) {
        if (check(TokenType::NUMBER)) {
            auto item = parseDataItem();
            item->isOutput = true;
            section->children.push_back(std::move(item));
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<DataItemNode> Parser::parseDataItem() {
    auto item = std::make_unique<DataItemNode>();
    item->level = std::stoi(consume(TokenType::NUMBER, "Expected level").lexeme);
    item->name = consumeIdentifier("Expected data name").lexeme;
    
    if (match(TokenType::PIC)) {
        if (match(TokenType::FLOAT)) {
            item->dataType.baseType = BaseType::FLOAT;
        } else if (match(TokenType::IV)) {
            item->dataType.baseType = BaseType::INT;
            if (match(TokenType::LPAREN)) {
                item->dataType.vectorSize = std::stoi(consume(TokenType::NUMBER, "Expected vector size").lexeme);
                consume(TokenType::RPAREN, "Expected ')'");
            }
        } else if (match(TokenType::UV)) {
            item->dataType.baseType = BaseType::UINT;
            if (match(TokenType::LPAREN)) {
                item->dataType.vectorSize = std::stoi(consume(TokenType::NUMBER, "Expected vector size").lexeme);
                consume(TokenType::RPAREN, "Expected ')'");
            }
        } else if (match(TokenType::BV)) {
            item->dataType.baseType = BaseType::BOOL;
            if (match(TokenType::LPAREN)) {
                item->dataType.vectorSize = std::stoi(consume(TokenType::NUMBER, "Expected vector size").lexeme);
                consume(TokenType::RPAREN, "Expected ')'");
            }
        } else if (match(TokenType::I)) { item->dataType.baseType = BaseType::INT; }
        else if (match(TokenType::U)) { item->dataType.baseType = BaseType::UINT; }
        else if (match(TokenType::B)) { item->dataType.baseType = BaseType::BOOL; }
        else if (match(TokenType::V)) {
            item->dataType.baseType = BaseType::FLOAT;
            if (match(TokenType::LPAREN)) {
                item->dataType.vectorSize = std::stoi(consume(TokenType::NUMBER, "Expected vector size").lexeme);
                consume(TokenType::RPAREN, "Expected ')'");
            }
        } else if (match(TokenType::M)) {
            item->dataType.baseType = BaseType::FLOAT; 
            if (match(TokenType::LPAREN)) {
                item->dataType.matrixRows = std::stoi(consume(TokenType::NUMBER, "Expected matrix rows").lexeme);
                if (match(TokenType::COMMA)) {
                    item->dataType.matrixCols = std::stoi(consume(TokenType::NUMBER, "Expected matrix columns").lexeme);
                } else {
                    item->dataType.matrixCols = item->dataType.matrixRows; 
                }
                consume(TokenType::RPAREN, "Expected ')'");
            }
        }
    }
    
    if (match(TokenType::IS)) {
        if (match(TokenType::BUILT_IN)) {
            item->isBuiltIn = true;
        }
    }
    
    if (match(TokenType::LOCATION)) {
        item->location = std::stoi(consume(TokenType::NUMBER, "Expected location number").lexeme);
    }

    if (match(TokenType::OCCURS)) {
        item->occursCount = std::stoi(consume(TokenType::NUMBER, "Expected occurs count").lexeme);
        consume(TokenType::TIMES, "Expected TIMES after occurs count");
    }
    
    if (match(TokenType::INTERPOLATION)) {
        consume(TokenType::IS, "Expected IS after INTERPOLATION");
        if (match(TokenType::FLAT)) item->interpolation = InterpolationMode::FLAT;
        else if (match(TokenType::NOPERSPECTIVE)) item->interpolation = InterpolationMode::NOPERSPECTIVE;
        else if (match(TokenType::SMOOTH)) item->interpolation = InterpolationMode::SMOOTH;
        else throw std::runtime_error("Expected FLAT, NOPERSPECTIVE, or SMOOTH after INTERPOLATION IS");
    }
    
    if (match(TokenType::VALUE)) {
        item->initialValue = parseExpression();
    }
    
    consume(TokenType::PERIOD, "Expected '.' after data item");
    return item;
}

std::unique_ptr<DivisionNode> Parser::parseProcedureDivision() {
    consume(TokenType::PROCEDURE, "Expected PROCEDURE");
    consume(TokenType::DIVISION, "Expected DIVISION");
    consume(TokenType::PERIOD, "Expected '.' after PROCEDURE DIVISION");
    
    auto div = std::make_unique<DivisionNode>();
    div->divisionType = TokenType::PROCEDURE;
    
    while (!isAtEnd() && !check(TokenType::DIVISION)) {
        div->sections.push_back(parseSection());
    }
    
    return div;
}

std::unique_ptr<SectionNode> Parser::parseSection() {
    auto section = std::make_unique<GenericSectionNode>(SectionType::PROCEDURE); 
    
    if (checkIdentifier() && (peek().lexeme.back() == '.' || (current + 1 < tokens.size() && tokens[current+1].type == TokenType::PERIOD))) {
        section->name = advance().lexeme;
        if (section->name.back() == '.') {
            section->name.pop_back();
        } else {
            consume(TokenType::PERIOD, "Expected '.' after paragraph name");
        }
    } else {
        throw std::runtime_error("Expected paragraph name or section in PROCEDURE DIVISION at line " + std::to_string(peek().line));
    }
    
    while (!isAtEnd() && !check(TokenType::PROCEDURE) && !check(TokenType::DIVISION)) {
        if (checkIdentifier() && (peek().lexeme.back() == '.' || (current + 1 < tokens.size() && tokens[current+1].type == TokenType::PERIOD))) {
            break;
        }
        
        if (match(TokenType::PERIOD)) continue;

        auto stmt = parseStatement();
        if (stmt) {
            section->children.push_back(std::move(stmt));
        } else if (!isAtEnd()) {
            throw std::runtime_error("Unexpected token in PROCEDURE DIVISION: " + peek().lexeme + " at line " + std::to_string(peek().line));
        } else {
            break;
        }
    }
    
    return section;
}

std::unique_ptr<StatementNode> Parser::parseStatement() {
    if (check(TokenType::GOBACK)) {
        consume(TokenType::GOBACK, "Expected GOBACK");
        return std::make_unique<GoBackNode>();
    }

    if (check(TokenType::DISCARD)) {
        consume(TokenType::DISCARD, "Expected DISCARD");
        return std::make_unique<DiscardNode>();
    }
    
    if (check(TokenType::MOVE)) {
        return parseMoveStatement();
    }
    
    if (check(TokenType::COMPUTE)) {
        return parseComputeStatement();
    }

    if (check(TokenType::IF)) {
        return parseIfStatement();
    }

    if (check(TokenType::PERFORM)) {
        return parsePerformStatement();
    }

    if (check(TokenType::READ)) {
        return parseReadStatement();
    }

    if (check(TokenType::WRITE)) {
        return parseWriteStatement();
    }

    if (check(TokenType::CALL)) {
        return parseCallStatement();
    }

    if (check(TokenType::ATOMIC_ADD) || check(TokenType::ATOMIC_SUB) || 
        check(TokenType::ATOMIC_AND) || check(TokenType::ATOMIC_OR) || 
        check(TokenType::ATOMIC_XOR) || check(TokenType::ATOMIC_MIN) || 
        check(TokenType::ATOMIC_MAX) || check(TokenType::ATOMIC_EXCHANGE) || 
        check(TokenType::ATOMIC_COMPARE_EXCHANGE)) {
        return parseAtomicOpStatement();
    }

    if (check(TokenType::CONSULT)) {
        return parseCohortOpStatement();
    }

    if (check(TokenType::INQUIRE)) {
        return parseInquireStatement();
    }

    if (check(TokenType::SYNC)) {
        return parseSyncStatement();
    }
    
    return nullptr;
}

std::unique_ptr<StatementNode> Parser::parseMoveStatement() {
    consume(TokenType::MOVE, "Expected MOVE");
    auto move = std::make_unique<MoveNode>();
    move->source = parseExpression();
    consume(TokenType::TO, "Expected TO");
    move->destination = parseExpression();
    return move;
}

std::unique_ptr<StatementNode> Parser::parseCallStatement() {
    consume(TokenType::CALL, "Expected CALL");
    auto node = std::make_unique<CallNode>();
    
    // CALL "GLSL.SIN" or CALL SIN
    if (match(TokenType::STRING)) {
        node->functionName = previous().lexeme;
        // Strip quotes
        if (node->functionName.size() >= 2) {
            node->functionName = node->functionName.substr(1, node->functionName.size() - 2);
        }
    } else {
        node->functionName = consumeIdentifier("Expected function name or string after CALL").lexeme;
    }

    if (match(TokenType::USING)) {
        do {
            node->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    if (match(TokenType::GIVING)) {
        do {
            node->destinations.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    return node;
}

std::unique_ptr<StatementNode> Parser::parseComputeStatement() {
    consume(TokenType::COMPUTE, "Expected COMPUTE");
    auto compute = std::make_unique<ComputeNode>();
    compute->destination = parsePrimary();
    consume(TokenType::EQUALS, "Expected '=' in COMPUTE statement");
    compute->expression = parseExpression();
    return compute;
}

std::unique_ptr<StatementNode> Parser::parseIfStatement() {
    consume(TokenType::IF, "Expected IF");
    auto node = std::make_unique<IfStatementNode>();
    node->condition = parseExpression();

    while (!isAtEnd() && !check(TokenType::ELSE) && !check(TokenType::END_IF) && !check(TokenType::PERIOD) && !check(TokenType::DIVISION) && !check(TokenType::SECTION)) {
        auto stmt = parseStatement();
        if (stmt) node->thenBranch.push_back(std::move(stmt));
        else break;
    }

    if (match(TokenType::ELSE)) {
        while (!isAtEnd() && !check(TokenType::END_IF) && !check(TokenType::PERIOD) && !check(TokenType::DIVISION) && !check(TokenType::SECTION)) {
            auto stmt = parseStatement();
            if (stmt) node->elseBranch.push_back(std::move(stmt));
            else break;
        }
    }

    if (match(TokenType::END_IF)) {
        match(TokenType::PERIOD);
    }

    return node;
}

std::unique_ptr<StatementNode> Parser::parsePerformStatement() {
    consume(TokenType::PERFORM, "Expected PERFORM");
    auto node = std::make_unique<PerformStatementNode>();

    if (match(TokenType::VARYING)) {
        node->varyingVar = parsePrimary();
        consume(TokenType::FROM, "Expected FROM");
        node->fromExpr = parseExpression();
        consume(TokenType::BY, "Expected BY");
        node->byExpr = parseExpression();
        consume(TokenType::UNTIL, "Expected UNTIL");
        node->untilExpr = parseExpression();
    } else if (match(TokenType::UNTIL)) {
        node->untilExpr = parseExpression();
    } else {
        node->timesExpr = parseExpression();
        consume(TokenType::TIMES, "Expected TIMES after expression");
    }

    while (!isAtEnd() && !check(TokenType::END_PERFORM) && !check(TokenType::PERIOD) && !check(TokenType::DIVISION) && !check(TokenType::SECTION)) {
        auto stmt = parseStatement();
        if (stmt) node->body.push_back(std::move(stmt));
        else break;
    }

    if (match(TokenType::END_PERFORM)) {
        match(TokenType::PERIOD);
    }

    return node;
}

std::unique_ptr<StatementNode> Parser::parseReadStatement() {
    consume(TokenType::READ, "Expected READ");
    auto node = std::make_unique<ReadNode>();
    node->imageName = consumeIdentifier("Expected image name").lexeme;
    
    if (match(TokenType::WITH)) {
        node->samplerName = consumeIdentifier("Expected sampler name").lexeme;
    }
    
    if (match(TokenType::FETCH)) {
        node->isFetch = true;
    } else if (match(TokenType::GATHER)) {
        node->isGather = true;
        if (match(TokenType::COMPONENT)) {
            node->component = parseExpression();
        }
    }

    consume(TokenType::AT, "Expected AT");
    node->coordinates = parseExpression();
    
    while (true) {
        if (match(TokenType::AT)) {
            consume(TokenType::LOD, "Expected LOD after AT");
            node->lod = parseExpression();
        } else if (match(TokenType::WITH)) {
            if (match(TokenType::PROJECTION)) {
                node->isProjective = true;
            } else if (match(TokenType::GRADIENTS)) {
                consume(TokenType::LPAREN, "Expected '(' after GRADIENTS");
                node->gradX = parseExpression();
                consume(TokenType::COMMA, "Expected ',' between gradients");
                node->gradY = parseExpression();
                consume(TokenType::RPAREN, "Expected ')' after gradients");
            } else if (match(TokenType::BIAS)) {
                node->lodBias = parseExpression();
            } else {
                throw std::runtime_error("Expected PROJECTION, GRADIENTS, or BIAS after WITH at line " + std::to_string(peek().line));
            }
        } else if (match(TokenType::COMPARING)) {
            match(TokenType::WITH); // Optional WITH
            node->compareValue = parseExpression();
        } else {
            break;
        }
    }

    consume(TokenType::INTO, "Expected INTO");
    node->target = parseExpression();
    
    return node;
}

std::unique_ptr<StatementNode> Parser::parseWriteStatement() {
    consume(TokenType::WRITE, "Expected WRITE");
    auto node = std::make_unique<WriteNode>();
    node->imageName = consumeIdentifier("Expected image name").lexeme;
    
    consume(TokenType::FROM, "Expected FROM");
    node->source = parseExpression();
    
    consume(TokenType::AT, "Expected AT");
    node->coordinates = parseExpression();
    return node;
}

std::unique_ptr<StatementNode> Parser::parseAtomicOpStatement() {
    auto node = std::make_unique<AtomicOpNode>();
    
    if (match(TokenType::ATOMIC_ADD)) node->opType = AtomicOpType::ADD;
    else if (match(TokenType::ATOMIC_SUB)) node->opType = AtomicOpType::SUB;
    else if (match(TokenType::ATOMIC_AND)) node->opType = AtomicOpType::AND;
    else if (match(TokenType::ATOMIC_OR)) node->opType = AtomicOpType::OR;
    else if (match(TokenType::ATOMIC_XOR)) node->opType = AtomicOpType::XOR;
    else if (match(TokenType::ATOMIC_MIN)) node->opType = AtomicOpType::MIN;
    else if (match(TokenType::ATOMIC_MAX)) node->opType = AtomicOpType::MAX;
    else if (match(TokenType::ATOMIC_EXCHANGE)) node->opType = AtomicOpType::EXCHANGE;
    else if (match(TokenType::ATOMIC_COMPARE_EXCHANGE)) node->opType = AtomicOpType::COMPARE_EXCHANGE;
    else throw std::runtime_error("Expected atomic operation");

    if (node->opType == AtomicOpType::COMPARE_EXCHANGE) {
        node->value = parseAddition();
        consume(TokenType::AND, "Expected AND in ATOMIC-COMPARE-EXCHANGE");
        node->comparator = parseAddition();
    } else {
        node->value = parseAddition();
    }
    
    if (node->opType == AtomicOpType::SUB) {
        consume(TokenType::FROM, "Expected FROM in ATOMIC-SUB");
    } else if (node->opType == AtomicOpType::ADD) {
        consume(TokenType::TO, "Expected TO in ATOMIC-ADD");
    } else {
        consume(TokenType::WITH, "Expected WITH in atomic operation");
    }
    
    node->target = parseExpression();
    
    if (match(TokenType::AT)) {
        node->coordinates = parseExpression();
    }
    
    if (match(TokenType::GIVING)) {
        node->destination = parseExpression();
    }
    
    return node;
}

static bool isComponent(TokenType type) {
    switch (type) {
        case TokenType::X: case TokenType::Y: case TokenType::Z: case TokenType::W:
        case TokenType::RED: case TokenType::GREEN: case TokenType::BLUE: case TokenType::ALPHA:
            return true;
        default: return false;
    }
}

static int getComponentIndex(TokenType type) {
    switch (type) {
        case TokenType::X: case TokenType::RED: return 0;
        case TokenType::Y: case TokenType::GREEN: return 1;
        case TokenType::Z: case TokenType::BLUE: return 2;
        case TokenType::W: case TokenType::ALPHA: return 3;
        default: return 0;
    }
}

std::unique_ptr<ExpressionNode> Parser::parseExpression() {
    return parseLogicalOr();
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenType::OR)) {
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), TokenType::OR, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalAnd() {
    auto expr = parseLogicalNot();
    while (match(TokenType::AND)) {
        auto right = parseLogicalNot();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), TokenType::AND, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalNot() {
    if (match(TokenType::NOT)) {
        auto expr = parseLogicalNot();
        return std::make_unique<UnaryExpressionNode>(TokenType::NOT, std::move(expr));
    }
    return parseComparison();
}

std::unique_ptr<ExpressionNode> Parser::parseComparison() {
    auto expr = parseBitwise();

    while (true) {
        TokenType op = TokenType::UNKNOWN;
        if (match(TokenType::EQUALS)) op = TokenType::EQUALS;
        else if (match(TokenType::GREATER_SYM)) op = TokenType::GREATER_SYM;
        else if (match(TokenType::LESS_SYM)) op = TokenType::LESS_SYM;
        else if (match(TokenType::GREATER_EQUAL_SYM)) op = TokenType::GREATER_EQUAL_SYM;
        else if (match(TokenType::LESS_EQUAL_SYM)) op = TokenType::LESS_EQUAL_SYM;
        else if (match(TokenType::NOT_EQUAL_SYM)) op = TokenType::NOT_EQUAL_SYM;
        else if (match(TokenType::GREATER)) {
            match(TokenType::THAN);
            op = TokenType::GREATER;
        } else if (match(TokenType::LESS)) {
            match(TokenType::THAN);
            op = TokenType::LESS;
        } else if (match(TokenType::EQUAL)) {
            match(TokenType::TO);
            op = TokenType::EQUAL;
        } else if (match(TokenType::NOT)) {
            if (match(TokenType::EQUAL)) {
                match(TokenType::TO);
                op = TokenType::NOT_EQUAL;
            } else {
                throw std::runtime_error("Unexpected NOT in comparison");
            }
        }

        if (op == TokenType::UNKNOWN) break;

        auto right = parseBitwise();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseBitwise() {
    auto expr = parseShift();
    while (match(TokenType::BIT_AND) || match(TokenType::BIT_OR) || match(TokenType::BIT_XOR)) {
        TokenType op = previous().type;
        auto right = parseShift();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseShift() {
    auto expr = parseAddition();
    while (match(TokenType::BIT_LSHIFT) || match(TokenType::BIT_RSHIFT)) {
        TokenType op = previous().type;
        auto right = parseAddition();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseAddition() {
    auto expr = parseMultiplication();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = previous().type;
        auto right = parseMultiplication();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseMultiplication() {
    auto expr = parseUnary();
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        TokenType op = previous().type;
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpressionNode>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseUnary() {
    if (match(TokenType::BIT_NOT) || match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = previous().type;
        auto right = parseUnary();
        return std::make_unique<UnaryExpressionNode>(op, std::move(right));
    }
    return parsePrimary();
}

std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
    if (match(TokenType::NUMBER) || match(TokenType::STRING)) {
        return std::make_unique<LiteralNode>(previous());
    }
    
    if (match(TokenType::LPAREN)) {
        std::vector<std::unique_ptr<ExpressionNode>> elements;
        elements.push_back(parseExpression());
        
        if (check(TokenType::COMMA)) {
            while (match(TokenType::COMMA)) {
                elements.push_back(parseExpression());
            }
            consume(TokenType::RPAREN, "Expected ')' after vector literal");
            return std::make_unique<VectorLiteralNode>(std::move(elements));
        } else {
            consume(TokenType::RPAREN, "Expected ')' after sub-expression");
            return std::move(elements[0]);
        }
    }
    
    if (isComponent(peek().type)) {
        std::vector<int> components;
        while (isComponent(peek().type)) {
            components.push_back(getComponentIndex(advance().type));
        }
        consume(TokenType::OF, "Expected 'OF' after swizzle components");
        auto inner = parsePrimary();
        return std::make_unique<SwizzleNode>(components, std::move(inner));
    }

    if (match(TokenType::FLOAT_TO_INT) || match(TokenType::FLOAT_TO_UINT) || 
        match(TokenType::INT_TO_FLOAT) || match(TokenType::UINT_TO_FLOAT) ||
        match(TokenType::INT_TO_UINT) || match(TokenType::UINT_TO_INT)) {
        TokenType convType = previous().type;
        consume(TokenType::LPAREN, "Expected '(' after conversion operator");
        auto inner = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after conversion expression");
        return std::make_unique<ConversionNode>(convType, std::move(inner));
    }

    if (checkIdentifier()) {
        std::string name = advance().lexeme;
        std::unique_ptr<ExpressionNode> subscript;
        if (match(TokenType::LPAREN)) {
            subscript = parseExpression();
            consume(TokenType::RPAREN, "Expected ')' after subscript");
        }

        if (match(TokenType::OF)) {
            auto parent = parsePrimary();
            auto qident = std::make_unique<QualifiedIdentifierNode>(name, std::move(parent));
            qident->subscript = std::move(subscript);
            return qident;
        }
        auto ident = std::make_unique<IdentifierNode>(name);
        ident->subscript = std::move(subscript);
        return ident;
    }
    throw std::runtime_error("Expected expression");
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw std::runtime_error(message + " (line " + std::to_string(peek().line) + ", column " + std::to_string(peek().column) + ")");
}

bool Parser::checkIdentifier() const {
    if (isAtEnd()) return false;
    TokenType type = peek().type;
    return type == TokenType::IDENTIFIER || 
           type == TokenType::I || type == TokenType::V || type == TokenType::U || 
           type == TokenType::B || type == TokenType::M ||
           type == TokenType::IV || type == TokenType::UV || type == TokenType::BV ||
           type == TokenType::FLOAT;
}

const Token& Parser::consumeIdentifier(const std::string& message) {
    if (checkIdentifier()) return advance();
    throw std::runtime_error(message + " (line " + std::to_string(peek().line) + ", column " + std::to_string(peek().column) + ")");
}

std::unique_ptr<StatementNode> Parser::parseCohortOpStatement() {
    consume(TokenType::CONSULT, "Expected CONSULT");
    consume(TokenType::COHORT, "Expected COHORT");
    consume(TokenType::FOR, "Expected FOR");

    auto node = std::make_unique<CohortOpNode>();
    node->mode = CohortMode::REDUCE; // Default

    if (match(TokenType::SUM)) node->opType = CohortOpType::SUM;
    else if (match(TokenType::PRODUCT)) node->opType = CohortOpType::PRODUCT;
    else if (match(TokenType::MIN)) node->opType = CohortOpType::MIN;
    else if (match(TokenType::MAX)) node->opType = CohortOpType::MAX;
    else if (match(TokenType::AND)) node->opType = CohortOpType::AND;
    else if (match(TokenType::OR)) node->opType = CohortOpType::OR;
    else if (match(TokenType::XOR)) node->opType = CohortOpType::XOR;
    else if (match(TokenType::ANY)) node->opType = CohortOpType::ANY;
    else if (match(TokenType::ALL)) node->opType = CohortOpType::ALL;
    else if (match(TokenType::ELECT)) node->opType = CohortOpType::ELECT;
    else if (match(TokenType::BALLOT)) node->opType = CohortOpType::BALLOT;
    else if (match(TokenType::BROADCAST)) node->opType = CohortOpType::BROADCAST;
    else if (match(TokenType::BROADCAST_FIRST)) node->opType = CohortOpType::BROADCAST_FIRST;
    else throw std::runtime_error("Expected cohort operation type (SUM, PRODUCT, ELECT, etc.) at line " + std::to_string(peek().line));

    // Optional Mode
    if (match(TokenType::REDUCE)) node->mode = CohortMode::REDUCE;
    else if (match(TokenType::INCLUSIVE_SCAN)) node->mode = CohortMode::INCLUSIVE_SCAN;
    else if (match(TokenType::EXCLUSIVE_SCAN)) node->mode = CohortMode::EXCLUSIVE_SCAN;

    // Optional "OF" and expression
    if (node->opType != CohortOpType::ELECT) {
        match(TokenType::OF); // Optional
        node->value = parseExpression();
    }

    // BROADCAST specific
    if (node->opType == CohortOpType::BROADCAST) {
        consume(TokenType::FROM, "Expected FROM after broadcast value");
        node->sourceId = parseExpression();
    }

    consume(TokenType::GIVING, "Expected GIVING after cohort operation");
    node->destination = parseExpression();

    return node;
}


std::unique_ptr<StatementNode> Parser::parseInquireStatement() {
    consume(TokenType::INQUIRE, "Expected INQUIRE");
    auto node = std::make_unique<InquireStatementNode>();
    node->resourceName = consumeIdentifier("Expected resource name after INQUIRE").lexeme;
    
    consume(TokenType::FOR, "Expected FOR after resource name");
    if (match(TokenType::SIZE)) {
        node->queryType = InquireQueryType::SIZE;
    } else if (match(TokenType::LEVELS)) {
        node->queryType = InquireQueryType::LEVELS;
    } else {
        throw std::runtime_error("Expected SIZE or LEVELS after FOR in INQUIRE");
    }
    
    if (match(TokenType::AT)) {
        consume(TokenType::LOD, "Expected LOD after AT");
        node->lod = parseExpression();
    }
    
    consume(TokenType::GIVING, "Expected GIVING after query");
    node->destination = parseExpression();
    
    return node;
}

std::unique_ptr<StatementNode> Parser::parseSyncStatement() {

    consume(TokenType::SYNC, "Expected SYNC");
    consume(TokenType::WORKGROUP, "Expected WORKGROUP after SYNC");
    return std::make_unique<SyncStatementNode>(SyncStatementNode::Scope::WORKGROUP);
}

} // namespace cobolv

