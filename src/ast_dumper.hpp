#include "ast.hpp"
#include <iostream>
#include <map>

namespace cobolv {

class AstDumper {
    int indentLevel = 0;
    void indent() {
        for (int i = 0; i < indentLevel; ++i) std::cout << "  ";
    }

    std::string dataTypeToString(const DataType& dt) {
        std::string s;
        switch (dt.baseType) {
            case BaseType::FLOAT: s = "FLOAT"; break;
            case BaseType::INT: s = "INT"; break;
            case BaseType::UINT: s = "UINT"; break;
            case BaseType::BOOL: s = "BOOL"; break;
            default: s = "UNKNOWN"; break;
        }
        if (dt.vectorSize > 0) s += "V(" + std::to_string(dt.vectorSize) + ")";
        if (dt.matrixRows > 0) s += "M(" + std::to_string(dt.matrixRows) + "," + std::to_string(dt.matrixCols) + ")";
        return s;
    }

public:
    void dump(const AstNode& node) {
        if (auto p = dynamic_cast<const ProgramNode*>(&node)) dumpProgram(*p);
        else if (auto d = dynamic_cast<const DivisionNode*>(&node)) dumpDivision(*d);
        else if (auto s = dynamic_cast<const SectionNode*>(&node)) dumpSection(*s);
        else if (auto st = dynamic_cast<const StatementNode*>(&node)) dumpStatement(*st);
        else if (auto e = dynamic_cast<const ExpressionNode*>(&node)) dumpExpression(*e);
    }

    void dumpProgram(const ProgramNode& prog) {
        indent(); std::cout << "Program: " << prog.programId << "\n";
        indentLevel++;
        for (auto const& div : prog.divisions) dump(*div);
        indentLevel--;
    }

    void dumpDivision(const DivisionNode& div) {
        indent(); std::cout << "Division: " << (int)div.divisionType << "\n";
        indentLevel++;
        for (auto const& sec : div.sections) dump(*sec);
        indentLevel--;
    }

    void dumpSection(const SectionNode& sec) {
        indent(); std::cout << "Section: " << sec.name << "\n";
        indentLevel++;
        if (auto sn = dynamic_cast<const SpecialNamesNode*>(&sec)) {
            indent(); std::cout << "LocalSize: " << sn->localSize.x << "," << sn->localSize.y << "," << sn->localSize.z << "\n";
        }
        for (auto const& child : sec.children) {
            if (auto fd = dynamic_cast<const FileDescriptionNode*>(child.get())) {
                indent(); std::cout << "FD: " << fd->name << " (Format: " << fd->format << ")\n";
                indentLevel++;
                for (auto const& rec : fd->records) dumpDataItem(*rec);
                indentLevel--;
            } else if (auto sel = dynamic_cast<const SelectNode*>(child.get())) {
                indent(); std::cout << "SELECT: " << sel->name << " ASSIGN TO " << sel->assignTo << "\n";
            } else if (auto di = dynamic_cast<const DataItemNode*>(child.get())) {
                dumpDataItem(*di);
            } else {
                dump(*child);
            }
        }
        indentLevel--;
    }

    void dumpDataItem(const DataItemNode& di) {
        indent(); std::cout << "DataItem: " << di.level << " " << di.name << " " << dataTypeToString(di.dataType);
        if (di.occursCount > 0) std::cout << " OCCURS " << di.occursCount;
        std::cout << "\n";
        indentLevel++;
        for (auto const& child : di.children) dumpDataItem(*child);
        indentLevel--;
    }

    void dumpStatement(const StatementNode& stmt) {
        if (auto m = dynamic_cast<const MoveNode*>(&stmt)) {
            indent(); std::cout << "MOVE\n";
            indentLevel++;
            indent(); std::cout << "Source:\n";
            indentLevel++; dumpExpression(*m->source); indentLevel--;
            indent(); std::cout << "Destination:\n";
            indentLevel++; dumpExpression(*m->destination); indentLevel--;
            indentLevel--;
        } else if (auto c = dynamic_cast<const ComputeNode*>(&stmt)) {
            indent(); std::cout << "COMPUTE\n";
            indentLevel++;
            dumpExpression(*c->expression);
            indent(); std::cout << "INTO\n";
            dumpExpression(*c->destination);
            indentLevel--;
        } else if (auto gb = dynamic_cast<const GoBackNode*>(&stmt)) {
            indent(); std::cout << "GOBACK\n";
        } else {
            indent(); std::cout << "Statement (Other)\n";
        }
    }

    void dumpExpression(const ExpressionNode& expr) {
        if (auto lit = dynamic_cast<const LiteralNode*>(&expr)) {
            indent(); std::cout << "Literal: " << lit->token.lexeme << "\n";
        } else if (auto id = dynamic_cast<const IdentifierNode*>(&expr)) {
            indent(); std::cout << "Identifier: " << id->name << "\n";
            if (id->subscript) {
                indentLevel++;
                indent(); std::cout << "Subscript:\n";
                indentLevel++; dumpExpression(*id->subscript); indentLevel--;
                indentLevel--;
            }
        } else if (auto qid = dynamic_cast<const QualifiedIdentifierNode*>(&expr)) {
            indent(); std::cout << "QualifiedIdentifier: " << qid->name << "\n";
            if (qid->subscript) {
                indentLevel++;
                indent(); std::cout << "Subscript:\n";
                indentLevel++; dumpExpression(*qid->subscript); indentLevel--;
                indentLevel--;
            }
            indentLevel++;
            indent(); std::cout << "OF:\n";
            indentLevel++; dump(*qid->parent); indentLevel--;
            indentLevel--;
        } else {
            indent(); std::cout << "Expression (Other)\n";
        }
    }
};

} // namespace cobolv
