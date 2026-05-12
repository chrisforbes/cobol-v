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

    std::string tokenTypeToString(TokenType t) {
        switch (t) {
            case TokenType::PLUS: return "+";
            case TokenType::MINUS: return "-";
            case TokenType::STAR: return "*";
            case TokenType::SLASH: return "/";
            case TokenType::EQUALS: return "=";
            case TokenType::GREATER_SYM: return ">";
            case TokenType::LESS_SYM: return "<";
            case TokenType::GREATER_EQUAL_SYM: return ">=";
            case TokenType::LESS_EQUAL_SYM: return "<=";
            case TokenType::NOT_EQUAL_SYM: return "<>";
            case TokenType::AND: return "AND";
            case TokenType::OR: return "OR";
            case TokenType::NOT: return "NOT";
            case TokenType::FLOAT_TO_INT: return "FLOAT-TO-INT";
            case TokenType::FLOAT_TO_UINT: return "FLOAT-TO-UINT";
            case TokenType::INT_TO_FLOAT: return "INT-TO-FLOAT";
            case TokenType::UINT_TO_FLOAT: return "UINT-TO-FLOAT";
            case TokenType::INT_TO_UINT: return "INT-TO-UINT";
            case TokenType::UINT_TO_INT: return "UINT-TO-INT";
            default: return "TOKEN(" + std::to_string((int)t) + ")";
        }
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
        } else if (auto ifs = dynamic_cast<const IfStatementNode*>(&stmt)) {
            indent(); std::cout << "IF\n";
            indentLevel++;
            dumpExpression(*ifs->condition);
            indent(); std::cout << "THEN\n";
            indentLevel++; for (auto const& s : ifs->thenBranch) dumpStatement(*s); indentLevel--;
            if (!ifs->elseBranch.empty()) {
                indent(); std::cout << "ELSE\n";
                indentLevel++; for (auto const& s : ifs->elseBranch) dumpStatement(*s); indentLevel--;
            }
            indentLevel--;
        } else if (auto perf = dynamic_cast<const PerformStatementNode*>(&stmt)) {
            indent(); std::cout << "PERFORM";
            if (perf->timesExpr) {
                std::cout << " TIMES\n";
                indentLevel++; dumpExpression(*perf->timesExpr); indentLevel--;
            } else if (perf->varyingVar) {
                std::cout << " VARYING\n";
                indentLevel++;
                indent(); std::cout << "Var:\n";
                indentLevel++; dumpExpression(*perf->varyingVar); indentLevel--;
                indent(); std::cout << "From:\n";
                indentLevel++; dumpExpression(*perf->fromExpr); indentLevel--;
                indent(); std::cout << "By:\n";
                indentLevel++; dumpExpression(*perf->byExpr); indentLevel--;
                indent(); std::cout << "Until:\n";
                indentLevel++; dumpExpression(*perf->untilExpr); indentLevel--;
                indentLevel--;
            } else if (perf->untilExpr) {
                std::cout << " UNTIL\n";
                indentLevel++; dumpExpression(*perf->untilExpr); indentLevel--;
            }
            indent(); std::cout << "BODY\n";
            indentLevel++; for (auto const& s : perf->body) dumpStatement(*s); indentLevel--;
        } else if (auto rd = dynamic_cast<const ReadNode*>(&stmt)) {
            indent(); std::cout << "READ " << rd->imageName;
            if (!rd->samplerName.empty()) std::cout << " WITH " << rd->samplerName;
            if (rd->isFetch) std::cout << " FETCH";
            if (rd->isGather) std::cout << " GATHER";
            if (rd->isProjective) std::cout << " WITH PROJECTION";
            std::cout << "\n";
            indentLevel++;
            indent(); std::cout << "Coords:\n";
            indentLevel++; dumpExpression(*rd->coordinates); indentLevel--;
            if (rd->lod) {
                indent(); std::cout << "LOD:\n";
                indentLevel++; dumpExpression(*rd->lod); indentLevel--;
            }
            if (rd->lodBias) {
                indent(); std::cout << "BIAS:\n";
                indentLevel++; dumpExpression(*rd->lodBias); indentLevel--;
            }
            if (rd->gradX) {
                indent(); std::cout << "GradX:\n";
                indentLevel++; dumpExpression(*rd->gradX); indentLevel--;
                indent(); std::cout << "GradY:\n";
                indentLevel++; dumpExpression(*rd->gradY); indentLevel--;
            }
            if (rd->compareValue) {
                indent(); std::cout << "CompareValue:\n";
                indentLevel++; dumpExpression(*rd->compareValue); indentLevel--;
            }
            indent(); std::cout << "INTO:\n";
            indentLevel++; dumpExpression(*rd->target); indentLevel--;
            indentLevel--;
        } else if (auto wr = dynamic_cast<const WriteNode*>(&stmt)) {
            indent(); std::cout << "WRITE " << wr->imageName << "\n";
            indentLevel++;
            indent(); std::cout << "Source:\n";
            indentLevel++; dumpExpression(*wr->source); indentLevel--;
            indent(); std::cout << "Coords:\n";
            indentLevel++; dumpExpression(*wr->coordinates); indentLevel--;
            indentLevel--;
        } else if (auto at = dynamic_cast<const AtomicOpNode*>(&stmt)) {
            indent(); std::cout << "ATOMIC OP (" << (int)at->opType << ")\n";
            indentLevel++;
            indent(); std::cout << "Target:\n";
            indentLevel++; dumpExpression(*at->target); indentLevel--;
            if (at->coordinates) {
                indent(); std::cout << "Coords:\n";
                indentLevel++; dumpExpression(*at->coordinates); indentLevel--;
            }
            indent(); std::cout << "Value:\n";
            indentLevel++; dumpExpression(*at->value); indentLevel--;
            if (at->destination) {
                indent(); std::cout << "GIVING:\n";
                indentLevel++; dumpExpression(*at->destination); indentLevel--;
            }
            indentLevel--;
        } else if (auto call = dynamic_cast<const CallNode*>(&stmt)) {
            indent(); std::cout << "CALL " << call->functionName << "\n";
            indentLevel++;
            for (auto const& arg : call->arguments) dumpExpression(*arg);
            if (!call->destinations.empty()) {
                indent(); std::cout << "GIVING\n";
                for (auto const& dst : call->destinations) dumpExpression(*dst);
            }
            indentLevel--;
        } else if (auto inq = dynamic_cast<const InquireStatementNode*>(&stmt)) {
            indent(); std::cout << "INQUIRE " << inq->resourceName << " FOR " << (int)inq->queryType << "\n";
            indentLevel++;
            if (inq->lod) {
                indent(); std::cout << "LOD:\n";
                indentLevel++; dumpExpression(*inq->lod); indentLevel--;
            }
            indent(); std::cout << "INTO:\n";
            indentLevel++; dumpExpression(*inq->destination); indentLevel--;
            indentLevel--;
        } else if (auto sync = dynamic_cast<const SyncStatementNode*>(&stmt)) {
            indent(); std::cout << "SYNC WORKGROUP\n";
        } else if (auto gb = dynamic_cast<const GoBackNode*>(&stmt)) {
            indent(); std::cout << "GOBACK\n";
        } else if (auto ds = dynamic_cast<const DiscardNode*>(&stmt)) {
            indent(); std::cout << "DISCARD\n";
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
            indentLevel++; dumpExpression(*qid->parent); indentLevel--;
            indentLevel--;
        } else if (auto swiz = dynamic_cast<const SwizzleNode*>(&expr)) {
            indent(); std::cout << "Swizzle: ";
            for (int c : swiz->components) std::cout << c << " ";
            std::cout << "\n";
            indentLevel++; dumpExpression(*swiz->inner); indentLevel--;
        } else if (auto bin = dynamic_cast<const BinaryExpressionNode*>(&expr)) {
            indent(); std::cout << "Binary Op: " << tokenTypeToString(bin->op) << "\n";
            indentLevel++;
            dumpExpression(*bin->left);
            dumpExpression(*bin->right);
            indentLevel--;
        } else if (auto un = dynamic_cast<const UnaryExpressionNode*>(&expr)) {
            indent(); std::cout << "Unary Op: " << tokenTypeToString(un->op) << "\n";
            indentLevel++;
            dumpExpression(*un->expr);
            indentLevel--;
        } else if (auto vec = dynamic_cast<const VectorLiteralNode*>(&expr)) {
            indent(); std::cout << "Vector Literal\n";
            indentLevel++;
            for (auto const& el : vec->elements) dumpExpression(*el);
            indentLevel--;
        } else if (auto conv = dynamic_cast<const ConversionNode*>(&expr)) {
            indent(); std::cout << "Conversion: " << tokenTypeToString(conv->convType) << "\n";
            indentLevel++;
            dumpExpression(*conv->expr);
            indentLevel--;
        } else {
            indent(); std::cout << "Expression (Other)\n";
        }
    }
};

} // namespace cobolv
