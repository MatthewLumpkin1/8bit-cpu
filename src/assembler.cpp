#include "assembler.hpp"
#include "instruction.hpp"
#include "memory.hpp"
#include "registers.hpp"

#include <cctype>
#include <cstdio>
#include <sstream>

// One source line after pass 1 has looked at it.
struct Statement {
    int lineNumber;
    int address;
    std::vector<std::string> tokens;
    int tableIndex;
    std::string sourceText;
};

static std::string toUpper(std::string text) {
    for (int i = 0; i < (int)text.size(); i++) {
        text[i] = (char)toupper((unsigned char)text[i]);
    }
    return text;
}

static std::string trim(const std::string& text) {
    int start = 0;
    int end = (int)text.size() - 1;
    while (start <= end && isspace((unsigned char)text[start])) start++;
    while (end >= start && isspace((unsigned char)text[end])) end--;
    if (start > end) return "";
    return text.substr(start, end - start + 1);
}

// Brackets are treated as whitespace, so LOAD R0, [200] and LOAD R0, 200 both
// work. The brackets are there for the reader, not the parser.
static std::vector<std::string> splitIntoTokens(std::string line) {
    size_t commentStart = line.find(';');
    if (commentStart != std::string::npos) {
        line = line.substr(0, commentStart);
    }
    for (int i = 0; i < (int)line.size(); i++) {
        if (line[i] == ',' || line[i] == '[' || line[i] == ']') {
            line[i] = ' ';
        }
    }

    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string word;
    while (stream >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

static int parseRegister(const std::string& token, int lineNumber) {
    std::string upper = toUpper(token);
    if (upper.size() == 2 && upper[0] == 'R') {
        int number = upper[1] - '0';
        if (number >= 0 && number < REGISTER_COUNT) {
            return number;
        }
    }
    AsmError error;
    error.line = lineNumber;
    error.message = "invalid register '" + token + "' (valid: R0-R3)";
    throw error;
}

static int parseNumber(const std::string& token, int lineNumber) {
    AsmError error;
    error.line = lineNumber;

    std::string digits = token;
    int base = 10;
    if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        digits = token.substr(2);
        base = 16;
    } else if (token.size() > 2 && token[0] == '0' && (token[1] == 'b' || token[1] == 'B')) {
        digits = token.substr(2);
        base = 2;
    }

    long value = 0;
    size_t charactersUsed = 0;
    try {
        value = std::stol(digits, &charactersUsed, base);
    } catch (const std::exception&) {
        error.message = "'" + token + "' is not a number";
        throw error;
    }
    if (charactersUsed != digits.size()) {
        error.message = "'" + token + "' is not a number";
        throw error;
    }
    if (value < 0 || value > 255) {
        error.message = "value " + token + " does not fit in 8 bits (0-255)";
        throw error;
    }
    return (int)value;
}

// Labels are resolved here, so a bare number still works as a jump target.
static int resolveTarget(const std::string& token, const std::map<std::string, int>& labels,
                         int lineNumber) {
    std::string upper = toUpper(token);
    if (labels.count(upper) > 0) {
        return labels.at(upper);
    }
    if (isdigit((unsigned char)token[0])) {
        return parseNumber(token, lineNumber);
    }
    AsmError error;
    error.line = lineNumber;
    error.message = "undefined label '" + token + "'";
    throw error;
}

static void requireOperandCount(const Statement& statement, int expected,
                                const std::string& mnemonic) {
    int actual = (int)statement.tokens.size() - 1;
    if (actual != expected) {
        AsmError error;
        error.line = statement.lineNumber;
        error.message = mnemonic + " expects " + std::to_string(expected)
                        + " operand(s), got " + std::to_string(actual);
        throw error;
    }
}

// Pass 1: record where every label sits and how many bytes each instruction takes.
// No code is emitted yet.
static std::vector<Statement> firstPass(const std::string& source,
                                        std::map<std::string, int>& labels) {
    std::vector<Statement> statements;
    std::istringstream sourceStream(source);
    std::string rawLine;
    int lineNumber = 0;
    int address = 0;

    while (std::getline(sourceStream, rawLine)) {
        lineNumber++;
        std::vector<std::string> tokens = splitIntoTokens(rawLine);
        if (tokens.empty()) {
            continue;
        }

        AsmError error;
        error.line = lineNumber;

        // A label can sit alone on its line or share it with an instruction.
        if (tokens[0][tokens[0].size() - 1] == ':') {
            std::string label = toUpper(tokens[0].substr(0, tokens[0].size() - 1));
            if (label.empty()) {
                error.message = "empty label";
                throw error;
            }
            if (labels.count(label) > 0) {
                error.message = "duplicate label '" + label + "' (first defined at address "
                                + std::to_string(labels[label]) + ")";
                throw error;
            }
            if (findInstructionByName(label) >= 0) {
                error.message = "'" + label + "' is an instruction mnemonic and cannot be a label";
                throw error;
            }
            labels[label] = address;
            tokens.erase(tokens.begin());
            if (tokens.empty()) {
                continue;
            }
        }

        int tableIndex = findInstructionByName(toUpper(tokens[0]));
        if (tableIndex < 0) {
            error.message = "unknown instruction '" + tokens[0] + "'";
            throw error;
        }

        Statement statement;
        statement.lineNumber = lineNumber;
        statement.address = address;
        statement.tokens = tokens;
        statement.tableIndex = tableIndex;
        statement.sourceText = trim(rawLine);
        statements.push_back(statement);

        address += getInstruction(tableIndex).width;
        if (address > MEMORY_SIZE) {
            error.message = "program exceeds the " + std::to_string(MEMORY_SIZE)
                            + "-byte address space";
            throw error;
        }
    }

    return statements;
}

static void emitPrimaryByte(std::vector<unsigned char>& code, int opcode,
                            int destination, int source) {
    code.push_back((unsigned char)((opcode << 4) | (destination << 2) | source));
}

static void emitExtendedBytes(std::vector<unsigned char>& code, int subOpcode,
                              int destination, int source) {
    code.push_back((unsigned char)(OP_EXT << 4));
    code.push_back((unsigned char)((subOpcode << 4) | (destination << 2) | source));
}

static void encodeStatement(const Statement& statement, const std::map<std::string, int>& labels,
                            std::vector<unsigned char>& code) {
    Instruction instruction = getInstruction(statement.tableIndex);
    int lineNumber = statement.lineNumber;

    if (instruction.form == FORM_NONE) {
        requireOperandCount(statement, 0, instruction.mnemonic);
        if (instruction.opcode == OP_EXT) {
            emitExtendedBytes(code, instruction.subOpcode, 0, 0);
        } else {
            emitPrimaryByte(code, instruction.opcode, 0, 0);
        }

    } else if (instruction.form == FORM_REG_REG) {
        requireOperandCount(statement, 2, instruction.mnemonic);
        int destination = parseRegister(statement.tokens[1], lineNumber);
        int source = parseRegister(statement.tokens[2], lineNumber);
        emitPrimaryByte(code, instruction.opcode, destination, source);

    } else if (instruction.form == FORM_REG_IMM) {
        requireOperandCount(statement, 2, instruction.mnemonic);
        int destination = parseRegister(statement.tokens[1], lineNumber);
        emitPrimaryByte(code, instruction.opcode, destination, 0);
        code.push_back((unsigned char)parseNumber(statement.tokens[2], lineNumber));

    } else if (instruction.form == FORM_REG_ADDR) {
        requireOperandCount(statement, 2, instruction.mnemonic);
        int destination = parseRegister(statement.tokens[1], lineNumber);
        emitPrimaryByte(code, instruction.opcode, destination, 0);
        code.push_back((unsigned char)resolveTarget(statement.tokens[2], labels, lineNumber));

    } else if (instruction.form == FORM_ADDR) {
        requireOperandCount(statement, 1, instruction.mnemonic);
        if (instruction.opcode == OP_EXT) {
            emitExtendedBytes(code, instruction.subOpcode, 0, 0);
        } else {
            emitPrimaryByte(code, instruction.opcode, 0, 0);
        }
        code.push_back((unsigned char)resolveTarget(statement.tokens[1], labels, lineNumber));

    } else if (instruction.form == FORM_REG) {
        requireOperandCount(statement, 1, instruction.mnemonic);
        int destination = parseRegister(statement.tokens[1], lineNumber);
        emitExtendedBytes(code, instruction.subOpcode, destination, 0);

    } else if (instruction.form == FORM_REG_INDIRECT) {
        requireOperandCount(statement, 2, instruction.mnemonic);
        int destination = parseRegister(statement.tokens[1], lineNumber);
        int source = parseRegister(statement.tokens[2], lineNumber);
        emitExtendedBytes(code, instruction.subOpcode, destination, source);
    }
}

static std::string makeListingLine(const Statement& statement,
                                   const std::vector<unsigned char>& code, int firstByte) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%04X:  ", statement.address);
    std::string line = buffer;

    for (int i = firstByte; i < (int)code.size(); i++) {
        snprintf(buffer, sizeof(buffer), "%02X ", code[i]);
        line += buffer;
    }
    while (line.size() < 23) {
        line += " ";
    }
    return line + statement.sourceText;
}

AsmResult assemble(const std::string& source) {
    AsmResult result;
    std::vector<Statement> statements = firstPass(source, result.labels);

    // Pass 2: emit machine code. Every label address is known now, so a forward
    // jump resolves exactly like a backward one.
    for (int i = 0; i < (int)statements.size(); i++) {
        Statement statement = statements[i];
        Instruction instruction = getInstruction(statement.tableIndex);
        int firstByte = (int)result.code.size();

        encodeStatement(statement, result.labels, result.code);

        // Pass 1 sized this instruction and pass 2 emitted it. If they ever
        // disagreed, every label after this point would be wrong.
        int bytesEmitted = (int)result.code.size() - firstByte;
        if (bytesEmitted != instruction.width) {
            AsmError error;
            error.line = statement.lineNumber;
            error.message = "internal: " + instruction.mnemonic + " sized as "
                            + std::to_string(instruction.width) + " bytes but emitted "
                            + std::to_string(bytesEmitted);
            throw error;
        }

        result.listing.push_back(makeListingLine(statement, result.code, firstByte));
    }

    return result;
}
