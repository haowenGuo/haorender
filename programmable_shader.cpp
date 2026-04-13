#include "programmable_shader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* kBuiltinNames[] = {
    "pi",
    "uv_u", "uv_v",
    "base_r", "base_g", "base_b", "base_luma",
    "n_x", "n_y", "n_z",
    "v_x", "v_y", "v_z",
    "l_x", "l_y", "l_z",
    "light_r", "light_g", "light_b",
    "ndotl", "ndotv", "half_lambert", "rim",
    "shadow", "metallic", "roughness", "ao",
    "emissive_r", "emissive_g", "emissive_b",
    "ambient", "specular", "exposure"
};

enum class TokenType {
    End,
    Number,
    Identifier,
    Plus,
    Minus,
    Star,
    Slash,
    LParen,
    RParen,
    Comma
};

struct Token {
    TokenType type = TokenType::End;
    double number = 0.0;
    std::string text;
    size_t position = 0;
};

struct ExpressionNode {
    virtual ~ExpressionNode() = default;
    virtual float eval(const std::vector<float>& values) const = 0;
};

struct ConstantNode final : ExpressionNode {
    explicit ConstantNode(float value0) : value(value0) {}
    float eval(const std::vector<float>&) const override { return value; }
    float value = 0.0f;
};

struct VariableNode final : ExpressionNode {
    explicit VariableNode(int index0) : index(index0) {}
    float eval(const std::vector<float>& values) const override {
        return (index >= 0 && index < static_cast<int>(values.size())) ? values[index] : 0.0f;
    }
    int index = -1;
};

struct UnaryNode final : ExpressionNode {
    UnaryNode(char op0, std::unique_ptr<ExpressionNode> child0)
        : op(op0), child(std::move(child0)) {}
    float eval(const std::vector<float>& values) const override {
        float v = child ? child->eval(values) : 0.0f;
        if (!std::isfinite(v)) {
            v = 0.0f;
        }
        return op == '-' ? -v : v;
    }
    char op = '+';
    std::unique_ptr<ExpressionNode> child;
};

struct BinaryNode final : ExpressionNode {
    BinaryNode(char op0, std::unique_ptr<ExpressionNode> lhs0, std::unique_ptr<ExpressionNode> rhs0)
        : op(op0), lhs(std::move(lhs0)), rhs(std::move(rhs0)) {}
    float eval(const std::vector<float>& values) const override {
        float a = lhs ? lhs->eval(values) : 0.0f;
        float b = rhs ? rhs->eval(values) : 0.0f;
        if (!std::isfinite(a)) {
            a = 0.0f;
        }
        if (!std::isfinite(b)) {
            b = 0.0f;
        }
        switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return std::abs(b) <= 1e-8f ? 0.0f : a / b;
        default: return 0.0f;
        }
    }
    char op = '+';
    std::unique_ptr<ExpressionNode> lhs;
    std::unique_ptr<ExpressionNode> rhs;
};

enum class FunctionId {
    Min,
    Max,
    Clamp,
    Mix,
    Pow,
    Abs,
    Sqrt,
    Sin,
    Cos,
    Tan,
    Floor,
    Ceil,
    Fract,
    Saturate,
    Step,
    Smoothstep,
    Sign
};

struct FunctionNode final : ExpressionNode {
    FunctionNode(FunctionId id0, std::vector<std::unique_ptr<ExpressionNode>> args0)
        : id(id0), args(std::move(args0)) {}

    float eval(const std::vector<float>& values) const override {
        auto arg = [&](size_t index) {
            if (index >= args.size() || !args[index]) {
                return 0.0f;
            }
            float v = args[index]->eval(values);
            return std::isfinite(v) ? v : 0.0f;
        };

        switch (id) {
        case FunctionId::Min: return std::min(arg(0), arg(1));
        case FunctionId::Max: return std::max(arg(0), arg(1));
        case FunctionId::Clamp: return std::clamp(arg(0), arg(1), arg(2));
        case FunctionId::Mix: {
            float t = std::clamp(arg(2), 0.0f, 1.0f);
            return arg(0) * (1.0f - t) + arg(1) * t;
        }
        case FunctionId::Pow: {
            float base = std::max(arg(0), 0.0f);
            return std::pow(base, arg(1));
        }
        case FunctionId::Abs: return std::abs(arg(0));
        case FunctionId::Sqrt: return std::sqrt(std::max(arg(0), 0.0f));
        case FunctionId::Sin: return std::sin(arg(0));
        case FunctionId::Cos: return std::cos(arg(0));
        case FunctionId::Tan: return std::tan(arg(0));
        case FunctionId::Floor: return std::floor(arg(0));
        case FunctionId::Ceil: return std::ceil(arg(0));
        case FunctionId::Fract: {
            float v = arg(0);
            return v - std::floor(v);
        }
        case FunctionId::Saturate: return std::clamp(arg(0), 0.0f, 1.0f);
        case FunctionId::Step: return arg(1) < arg(0) ? 0.0f : 1.0f;
        case FunctionId::Smoothstep: {
            float edge0 = arg(0);
            float edge1 = arg(1);
            float denom = edge1 - edge0;
            if (std::abs(denom) <= 1e-8f) {
                return arg(2) >= edge1 ? 1.0f : 0.0f;
            }
            float t = std::clamp((arg(2) - edge0) / denom, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
        case FunctionId::Sign: {
            float v = arg(0);
            return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f);
        }
        default:
            return 0.0f;
        }
    }

    FunctionId id = FunctionId::Min;
    std::vector<std::unique_ptr<ExpressionNode>> args;
};

struct FunctionSpec {
    FunctionId id;
    int min_args;
    int max_args;
};

const std::unordered_map<std::string, FunctionSpec>& functionTable() {
    static const std::unordered_map<std::string, FunctionSpec> table = {
        { "min", { FunctionId::Min, 2, 2 } },
        { "max", { FunctionId::Max, 2, 2 } },
        { "clamp", { FunctionId::Clamp, 3, 3 } },
        { "mix", { FunctionId::Mix, 3, 3 } },
        { "pow", { FunctionId::Pow, 2, 2 } },
        { "abs", { FunctionId::Abs, 1, 1 } },
        { "sqrt", { FunctionId::Sqrt, 1, 1 } },
        { "sin", { FunctionId::Sin, 1, 1 } },
        { "cos", { FunctionId::Cos, 1, 1 } },
        { "tan", { FunctionId::Tan, 1, 1 } },
        { "floor", { FunctionId::Floor, 1, 1 } },
        { "ceil", { FunctionId::Ceil, 1, 1 } },
        { "fract", { FunctionId::Fract, 1, 1 } },
        { "saturate", { FunctionId::Saturate, 1, 1 } },
        { "step", { FunctionId::Step, 2, 2 } },
        { "smoothstep", { FunctionId::Smoothstep, 3, 3 } },
        { "sign", { FunctionId::Sign, 1, 1 } }
    };
    return table;
}

std::string trim(const std::string& value) {
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

bool isIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
        return false;
    }
    for (char ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> splitStatements(const std::string& source) {
    std::vector<std::string> statements;
    std::stringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        size_t comment = line.find("//");
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        std::string current;
        for (char ch : line) {
            if (ch == ';') {
                std::string statement = trim(current);
                if (!statement.empty()) {
                    statements.push_back(statement);
                }
                current.clear();
            }
            else {
                current.push_back(ch);
            }
        }
        std::string statement = trim(current);
        if (!statement.empty()) {
            statements.push_back(statement);
        }
    }
    return statements;
}

class Parser {
public:
    Parser(const std::string& source0,
           std::unordered_map<std::string, int>& symbols0,
           std::string& error0)
        : source(source0), symbols(symbols0), error(error0) {
        advance();
    }

    std::unique_ptr<ExpressionNode> parseExpression() {
        return parseAdditive();
    }

    bool ok() const { return error.empty(); }

private:
    void advance() {
        token = nextToken();
    }

    Token nextToken() {
        Token result;
        while (position < source.size() && std::isspace(static_cast<unsigned char>(source[position]))) {
            ++position;
        }
        result.position = position;
        if (position >= source.size()) {
            result.type = TokenType::End;
            return result;
        }

        char ch = source[position];
        switch (ch) {
        case '+': ++position; result.type = TokenType::Plus; return result;
        case '-': ++position; result.type = TokenType::Minus; return result;
        case '*': ++position; result.type = TokenType::Star; return result;
        case '/': ++position; result.type = TokenType::Slash; return result;
        case '(': ++position; result.type = TokenType::LParen; return result;
        case ')': ++position; result.type = TokenType::RParen; return result;
        case ',': ++position; result.type = TokenType::Comma; return result;
        default:
            break;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
            char* end_ptr = nullptr;
            result.number = std::strtod(source.c_str() + position, &end_ptr);
            size_t consumed = static_cast<size_t>(end_ptr - (source.c_str() + position));
            position += consumed;
            result.type = TokenType::Number;
            return result;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            size_t start = position;
            ++position;
            while (position < source.size()) {
                char current = source[position];
                if (!(std::isalnum(static_cast<unsigned char>(current)) || current == '_')) {
                    break;
                }
                ++position;
            }
            result.type = TokenType::Identifier;
            result.text = source.substr(start, position - start);
            return result;
        }

        error = "Unexpected token near position " + std::to_string(position + 1);
        result.type = TokenType::End;
        return result;
    }

    std::unique_ptr<ExpressionNode> parseAdditive() {
        std::unique_ptr<ExpressionNode> node = parseMultiplicative();
        while (ok() && (token.type == TokenType::Plus || token.type == TokenType::Minus)) {
            char op = token.type == TokenType::Plus ? '+' : '-';
            advance();
            std::unique_ptr<ExpressionNode> rhs = parseMultiplicative();
            node = std::make_unique<BinaryNode>(op, std::move(node), std::move(rhs));
        }
        return node;
    }

    std::unique_ptr<ExpressionNode> parseMultiplicative() {
        std::unique_ptr<ExpressionNode> node = parseUnary();
        while (ok() && (token.type == TokenType::Star || token.type == TokenType::Slash)) {
            char op = token.type == TokenType::Star ? '*' : '/';
            advance();
            std::unique_ptr<ExpressionNode> rhs = parseUnary();
            node = std::make_unique<BinaryNode>(op, std::move(node), std::move(rhs));
        }
        return node;
    }

    std::unique_ptr<ExpressionNode> parseUnary() {
        if (token.type == TokenType::Plus || token.type == TokenType::Minus) {
            char op = token.type == TokenType::Minus ? '-' : '+';
            advance();
            return std::make_unique<UnaryNode>(op, parseUnary());
        }
        return parsePrimary();
    }

    std::unique_ptr<ExpressionNode> parsePrimary() {
        if (!ok()) {
            return nullptr;
        }
        if (token.type == TokenType::Number) {
            float value = static_cast<float>(token.number);
            advance();
            return std::make_unique<ConstantNode>(value);
        }
        if (token.type == TokenType::Identifier) {
            std::string name = token.text;
            advance();
            if (token.type == TokenType::LParen) {
                auto spec_it = functionTable().find(name);
                if (spec_it == functionTable().end()) {
                    error = "Unknown function: " + name;
                    return nullptr;
                }
                FunctionSpec spec = spec_it->second;
                advance();
                std::vector<std::unique_ptr<ExpressionNode>> args;
                if (token.type != TokenType::RParen) {
                    while (ok()) {
                        args.push_back(parseExpression());
                        if (token.type == TokenType::Comma) {
                            advance();
                            continue;
                        }
                        break;
                    }
                }
                if (token.type != TokenType::RParen) {
                    error = "Missing ')' in function call: " + name;
                    return nullptr;
                }
                advance();
                if (static_cast<int>(args.size()) < spec.min_args || static_cast<int>(args.size()) > spec.max_args) {
                    error = "Invalid argument count for function: " + name;
                    return nullptr;
                }
                return std::make_unique<FunctionNode>(spec.id, std::move(args));
            }

            auto symbol_it = symbols.find(name);
            if (symbol_it == symbols.end()) {
                error = "Unknown variable: " + name;
                return nullptr;
            }
            return std::make_unique<VariableNode>(symbol_it->second);
        }
        if (token.type == TokenType::LParen) {
            advance();
            std::unique_ptr<ExpressionNode> node = parseExpression();
            if (token.type != TokenType::RParen) {
                error = "Missing ')'";
                return nullptr;
            }
            advance();
            return node;
        }

        error = "Expected number, variable, or function near position " + std::to_string(token.position + 1);
        return nullptr;
    }

    const std::string& source;
    std::unordered_map<std::string, int>& symbols;
    std::string& error;
    size_t position = 0;
    Token token;
};

} // namespace

struct ProgrammableShaderProgram::Impl {
    struct Statement {
        int target_index = -1;
        std::unique_ptr<ExpressionNode> expression;
    };

    std::vector<std::string> symbol_names;
    std::vector<Statement> statements;
    int out_r_index = -1;
    int out_g_index = -1;
    int out_b_index = -1;
};

ProgrammableShaderProgram::ProgrammableShaderProgram(std::string source)
    : source_(std::move(source)), impl_(std::make_unique<Impl>()) {}

std::shared_ptr<ProgrammableShaderProgram> ProgrammableShaderProgram::compile(const std::string& source, std::string& error_message) {
    error_message.clear();
    auto program = std::shared_ptr<ProgrammableShaderProgram>(new ProgrammableShaderProgram(source));

    std::unordered_map<std::string, int> symbols;
    program->impl_->symbol_names.reserve(std::size(kBuiltinNames) + 16);
    for (size_t i = 0; i < std::size(kBuiltinNames); ++i) {
        symbols[kBuiltinNames[i]] = static_cast<int>(i);
        program->impl_->symbol_names.push_back(kBuiltinNames[i]);
    }

    std::vector<std::string> statements = splitStatements(source);
    if (statements.empty()) {
        error_message = "Shader source is empty. Write assignments such as r = base_r * shadow;";
        return nullptr;
    }

    for (size_t statement_index = 0; statement_index < statements.size(); ++statement_index) {
        const std::string& statement = statements[statement_index];
        size_t equals = statement.find('=');
        if (equals == std::string::npos) {
            error_message = "Statement " + std::to_string(statement_index + 1) + " is missing '='.";
            return nullptr;
        }

        std::string lhs = trim(statement.substr(0, equals));
        std::string rhs = trim(statement.substr(equals + 1));
        if (!isIdentifier(lhs)) {
            error_message = "Invalid assignment target in statement " + std::to_string(statement_index + 1) + ".";
            return nullptr;
        }
        if (rhs.empty()) {
            error_message = "Statement " + std::to_string(statement_index + 1) + " has an empty expression.";
            return nullptr;
        }

        int target_index = -1;
        auto existing = symbols.find(lhs);
        if (existing == symbols.end()) {
            target_index = static_cast<int>(symbols.size());
            symbols[lhs] = target_index;
            program->impl_->symbol_names.push_back(lhs);
        }
        else {
            target_index = existing->second;
        }

        std::string parse_error;
        Parser parser(rhs, symbols, parse_error);
        std::unique_ptr<ExpressionNode> expression = parser.parseExpression();
        if (!parse_error.empty() || !parser.ok() || !expression) {
            error_message = "Statement " + std::to_string(statement_index + 1) + ": " +
                            (parse_error.empty() ? "Failed to parse expression." : parse_error);
            return nullptr;
        }

        Impl::Statement compiled;
        compiled.target_index = target_index;
        compiled.expression = std::move(expression);
        program->impl_->statements.push_back(std::move(compiled));

        if (lhs == "r") {
            program->impl_->out_r_index = target_index;
        }
        else if (lhs == "g") {
            program->impl_->out_g_index = target_index;
        }
        else if (lhs == "b") {
            program->impl_->out_b_index = target_index;
        }
    }

    if (program->impl_->out_r_index < 0 || program->impl_->out_g_index < 0 || program->impl_->out_b_index < 0) {
        error_message = "Shader source must assign all of r, g, and b.";
        return nullptr;
    }

    return program;
}

bool ProgrammableShaderProgram::evaluate(const Inputs& inputs, float& out_r, float& out_g, float& out_b) const {
    if (!impl_) {
        return false;
    }

    std::vector<float> values(impl_->symbol_names.size(), 0.0f);
    values[0] = inputs.pi;
    values[1] = inputs.uv_u;
    values[2] = inputs.uv_v;
    values[3] = inputs.base_r;
    values[4] = inputs.base_g;
    values[5] = inputs.base_b;
    values[6] = inputs.base_luma;
    values[7] = inputs.n_x;
    values[8] = inputs.n_y;
    values[9] = inputs.n_z;
    values[10] = inputs.v_x;
    values[11] = inputs.v_y;
    values[12] = inputs.v_z;
    values[13] = inputs.l_x;
    values[14] = inputs.l_y;
    values[15] = inputs.l_z;
    values[16] = inputs.light_r;
    values[17] = inputs.light_g;
    values[18] = inputs.light_b;
    values[19] = inputs.ndotl;
    values[20] = inputs.ndotv;
    values[21] = inputs.half_lambert;
    values[22] = inputs.rim;
    values[23] = inputs.shadow;
    values[24] = inputs.metallic;
    values[25] = inputs.roughness;
    values[26] = inputs.ao;
    values[27] = inputs.emissive_r;
    values[28] = inputs.emissive_g;
    values[29] = inputs.emissive_b;
    values[30] = inputs.ambient;
    values[31] = inputs.specular;
    values[32] = inputs.exposure;

    for (const auto& statement : impl_->statements) {
        if (!statement.expression || statement.target_index < 0 || statement.target_index >= static_cast<int>(values.size())) {
            continue;
        }
        float value = statement.expression->eval(values);
        values[statement.target_index] = std::isfinite(value) ? value : 0.0f;
    }

    if (impl_->out_r_index < 0 || impl_->out_g_index < 0 || impl_->out_b_index < 0) {
        return false;
    }
    out_r = values[impl_->out_r_index];
    out_g = values[impl_->out_g_index];
    out_b = values[impl_->out_b_index];
    if (!std::isfinite(out_r)) {
        out_r = 0.0f;
    }
    if (!std::isfinite(out_g)) {
        out_g = 0.0f;
    }
    if (!std::isfinite(out_b)) {
        out_b = 0.0f;
    }
    return true;
}

std::string ProgrammableShaderProgram::defaultSource() {
    return
        "// Outputs are linear RGB. Exposure and tone mapping are applied after this program.\n"
        "lambert = (0.12 + ndotl * shadow) * ao;\n"
        "highlight = pow(max(ndotl, 0.0), 8.0) * specular * 0.35;\n"
        "rim_light = pow(rim, 2.5) * 0.18;\n"
        "r = base_r * lambert * light_r + emissive_r + highlight + rim_light;\n"
        "g = base_g * lambert * light_g + emissive_g + highlight + rim_light;\n"
        "b = base_b * lambert * light_b + emissive_b + highlight + rim_light;\n";
}

std::string ProgrammableShaderProgram::guideText() {
    return
        "Write one assignment per line. The program must assign r, g, and b.\n"
        "All color inputs are linear. Output linear RGB too; exposure and tone mapping happen after your code.\n"
        "You can define temporary variables before the final r/g/b assignments.\n"
        "\n"
        "Built-in variables:\n"
        "  uv_u, uv_v\n"
        "  base_r, base_g, base_b, base_luma\n"
        "  n_x, n_y, n_z\n"
        "  v_x, v_y, v_z\n"
        "  l_x, l_y, l_z\n"
        "  light_r, light_g, light_b\n"
        "  ndotl, ndotv, half_lambert, rim\n"
        "  shadow, metallic, roughness, ao\n"
        "  emissive_r, emissive_g, emissive_b\n"
        "  ambient, specular, exposure, pi\n"
        "\n"
        "Built-in functions:\n"
        "  min, max, clamp, mix, pow, abs, sqrt, sin, cos, tan,\n"
        "  floor, ceil, fract, saturate, step, smoothstep, sign\n"
        "\n"
        "Example:\n"
        "  lambert = (0.2 + ndotl * shadow) * ao;\n"
        "  r = base_r * lambert * light_r;\n"
        "  g = base_g * lambert * light_g;\n"
        "  b = base_b * lambert * light_b;\n"
        "\n"
        "Toon example:\n"
        "  band = smoothstep(0.35, 0.7, ndotl * shadow);\n"
        "  diffuse = mix(0.28, 1.0, band);\n"
        "  r = base_r * diffuse;\n"
        "  g = base_g * diffuse;\n"
        "  b = base_b * diffuse;\n"
        "\n"
        "Debug normal example:\n"
        "  r = n_x * 0.5 + 0.5;\n"
        "  g = n_y * 0.5 + 0.5;\n"
        "  b = n_z * 0.5 + 0.5;\n"
        "\n"
        "This mode is intended for look-dev and debugging. It is slower than the built-in PBR / Phong paths.";
}
