/**
 * ATP Prover - Propositional Logic Resolution Theorem Prover (C++)
 *
 * Ported from prover.js — same algorithm:
 *   1. Tokenizer / Lexer
 *   2. Recursive-descent Parser (AST construction)
 *   3. CNF (Conjunctive Normal Form) conversion
 *   4. Resolution-based refutation proof
 *
 * Usage: ./resolution "P | !P"
 * Output: JSON with proof steps to stdout
 */
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <sstream>
#include <algorithm>
#include <stdexcept>

// =========================================================
// JSON helper — minimal JSON output
// =========================================================
static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

// =========================================================
// Token types
// =========================================================
enum class TokType { VAR, NOT, AND, OR, IMPLIES, IFF, LPAREN, RPAREN, EOFINPUT };

struct Token {
    TokType type;
    std::string value;
};

// =========================================================
// AST
// =========================================================
enum class NodeType { VAR, NOT, AND, OR, IMPLIES, IFF };

struct Node {
    NodeType type;
    std::string name;                            // VAR
    std::shared_ptr<Node> operand;               // NOT
    std::shared_ptr<Node> left, right;           // binary
};

using NodePtr = std::shared_ptr<Node>;

static NodePtr mkVar(const std::string& n) {
    return std::make_shared<Node>(Node{NodeType::VAR, n, nullptr, nullptr, nullptr});
}
static NodePtr mkNot(NodePtr op) {
    return std::make_shared<Node>(Node{NodeType::NOT, "", op, nullptr, nullptr});
}
static NodePtr mkBin(NodeType t, NodePtr l, NodePtr r) {
    return std::make_shared<Node>(Node{t, "", nullptr, l, r});
}
static NodePtr cloneAST(NodePtr n) {
    if (!n) return nullptr;
    if (n->type == NodeType::VAR) return mkVar(n->name);
    if (n->type == NodeType::NOT) return mkNot(cloneAST(n->operand));
    return mkBin(n->type, cloneAST(n->left), cloneAST(n->right));
}

// =========================================================
// Tokenizer
// =========================================================
static std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < input.size()) {
        char ch = input[i];
        if (std::isspace(ch)) { i++; continue; }
        if (ch == '!' || ch == '~') { tokens.push_back({TokType::NOT}); i++; continue; }
        // UTF-8 NOT ¬ (C2 AC)
        if ((unsigned char)ch == 0xC2 && i+1 < input.size() && (unsigned char)input[i+1] == 0xAC) {
            tokens.push_back({TokType::NOT}); i += 2; continue;
        }
        if (ch == '&') {
            if (i+1 < input.size() && input[i+1] == '&') i++;
            tokens.push_back({TokType::AND}); i++; continue;
        }
        // UTF-8 ∧ (E2 88 A7)
        if ((unsigned char)ch == 0xE2 && i+2 < input.size() && (unsigned char)input[i+1] == 0x88 && (unsigned char)input[i+2] == 0xA7) {
            tokens.push_back({TokType::AND}); i += 3; continue;
        }
        if (ch == '|') {
            if (i+1 < input.size() && input[i+1] == '|') i++;
            tokens.push_back({TokType::OR}); i++; continue;
        }
        // UTF-8 ∨ (E2 88 A8)
        if ((unsigned char)ch == 0xE2 && i+2 < input.size() && (unsigned char)input[i+1] == 0x88 && (unsigned char)input[i+2] == 0xA8) {
            tokens.push_back({TokType::OR}); i += 3; continue;
        }
        // IFF <->
        if (ch == '<' && i+2 < input.size() && input[i+1] == '-' && input[i+2] == '>') {
            tokens.push_back({TokType::IFF}); i += 3; continue;
        }
        // UTF-8 ↔ (E2 86 94)
        if ((unsigned char)ch == 0xE2 && i+2 < input.size() && (unsigned char)input[i+1] == 0x86 && (unsigned char)input[i+2] == 0x94) {
            tokens.push_back({TokType::IFF}); i += 3; continue;
        }
        // IMPLIES ->
        if (ch == '-' && i+1 < input.size() && input[i+1] == '>') {
            tokens.push_back({TokType::IMPLIES}); i += 2; continue;
        }
        // UTF-8 → (E2 86 92)
        if ((unsigned char)ch == 0xE2 && i+2 < input.size() && (unsigned char)input[i+1] == 0x86 && (unsigned char)input[i+2] == 0x92) {
            tokens.push_back({TokType::IMPLIES}); i += 3; continue;
        }
        if (ch == '(') { tokens.push_back({TokType::LPAREN}); i++; continue; }
        if (ch == ')') { tokens.push_back({TokType::RPAREN}); i++; continue; }
        if (std::isupper(ch)) {
            std::string name(1, ch); i++;
            while (i < input.size() && std::isalnum(input[i])) { name += input[i]; i++; }
            tokens.push_back({TokType::VAR, name}); continue;
        }
        throw std::runtime_error(std::string("Unknown character: '") + ch + "'");
    }
    tokens.push_back({TokType::EOFINPUT});
    return tokens;
}

// =========================================================
// Parser
// =========================================================
class Parser {
    std::vector<Token>& tokens;
    size_t pos = 0;
    Token& peek() { return tokens[pos]; }
    Token advance() { return tokens[pos++]; }
public:
    Parser(std::vector<Token>& t) : tokens(t) {}

    NodePtr parseIff() {
        auto left = parseImplies();
        while (peek().type == TokType::IFF) { advance(); left = mkBin(NodeType::IFF, left, parseImplies()); }
        return left;
    }
    NodePtr parseImplies() {
        auto left = parseOr();
        if (peek().type == TokType::IMPLIES) { advance(); return mkBin(NodeType::IMPLIES, left, parseImplies()); }
        return left;
    }
    NodePtr parseOr() {
        auto left = parseAnd();
        while (peek().type == TokType::OR) { advance(); left = mkBin(NodeType::OR, left, parseAnd()); }
        return left;
    }
    NodePtr parseAnd() {
        auto left = parseUnary();
        while (peek().type == TokType::AND) { advance(); left = mkBin(NodeType::AND, left, parseUnary()); }
        return left;
    }
    NodePtr parseUnary() {
        if (peek().type == TokType::NOT) { advance(); return mkNot(parseUnary()); }
        return parseAtom();
    }
    NodePtr parseAtom() {
        if (peek().type == TokType::VAR) { auto t = advance(); return mkVar(t.value); }
        if (peek().type == TokType::LPAREN) {
            advance();
            auto n = parseIff();
            if (peek().type != TokType::RPAREN) throw std::runtime_error("Expected )");
            advance();
            return n;
        }
        throw std::runtime_error("Unexpected token");
    }
    NodePtr parse() {
        auto ast = parseIff();
        if (peek().type != TokType::EOFINPUT) throw std::runtime_error("Extra tokens after expression");
        return ast;
    }
};

// =========================================================
// AST to string
// =========================================================
static int prec(NodeType t) {
    switch (t) {
        case NodeType::IFF: return 1; case NodeType::IMPLIES: return 2;
        case NodeType::OR: return 3; case NodeType::AND: return 4;
        case NodeType::NOT: return 5; case NodeType::VAR: return 6;
    }
    return 0;
}

static std::string astToString(NodePtr n);
static std::string wrapIfLower(NodePtr n, NodeType parent) {
    auto s = astToString(n);
    return prec(n->type) < prec(parent) ? "(" + s + ")" : s;
}
static std::string astToString(NodePtr n) {
    if (!n) return "?";
    switch (n->type) {
        case NodeType::VAR: return n->name;
        case NodeType::NOT: {
            auto inner = astToString(n->operand);
            if (n->operand->type != NodeType::VAR) inner = "(" + inner + ")";
            return "\xC2\xAC" + inner; // ¬
        }
        case NodeType::AND: return wrapIfLower(n->left, NodeType::AND) + " \xE2\x88\xA7 " + wrapIfLower(n->right, NodeType::AND);
        case NodeType::OR:  return wrapIfLower(n->left, NodeType::OR) + " \xE2\x88\xA8 " + wrapIfLower(n->right, NodeType::OR);
        case NodeType::IMPLIES: return wrapIfLower(n->left, NodeType::IMPLIES) + " \xE2\x86\x92 " + wrapIfLower(n->right, NodeType::IMPLIES);
        case NodeType::IFF: return wrapIfLower(n->left, NodeType::IFF) + " \xE2\x86\x94 " + wrapIfLower(n->right, NodeType::IFF);
    }
    return "?";
}

// =========================================================
// CNF Conversion
// =========================================================
static NodePtr eliminateIff(NodePtr n) {
    if (!n) return n;
    if (n->type == NodeType::VAR) return n;
    if (n->type == NodeType::NOT) return mkNot(eliminateIff(n->operand));
    if (n->type == NodeType::IFF) {
        auto l = eliminateIff(n->left), r = eliminateIff(n->right);
        return mkBin(NodeType::AND,
            mkBin(NodeType::IMPLIES, cloneAST(l), cloneAST(r)),
            mkBin(NodeType::IMPLIES, cloneAST(r), cloneAST(l)));
    }
    return mkBin(n->type, eliminateIff(n->left), eliminateIff(n->right));
}

static NodePtr eliminateImplies(NodePtr n) {
    if (!n) return n;
    if (n->type == NodeType::VAR) return n;
    if (n->type == NodeType::NOT) return mkNot(eliminateImplies(n->operand));
    if (n->type == NodeType::IMPLIES) {
        auto l = eliminateImplies(n->left), r = eliminateImplies(n->right);
        return mkBin(NodeType::OR, mkNot(l), r);
    }
    return mkBin(n->type, eliminateImplies(n->left), eliminateImplies(n->right));
}

static NodePtr pushNegation(NodePtr n) {
    if (!n) return n;
    if (n->type == NodeType::VAR) return n;
    if (n->type == NodeType::NOT) {
        auto inner = n->operand;
        if (inner->type == NodeType::NOT) return pushNegation(inner->operand);
        if (inner->type == NodeType::AND)
            return pushNegation(mkBin(NodeType::OR, mkNot(inner->left), mkNot(inner->right)));
        if (inner->type == NodeType::OR)
            return pushNegation(mkBin(NodeType::AND, mkNot(inner->left), mkNot(inner->right)));
        return mkNot(pushNegation(inner));
    }
    return mkBin(n->type, pushNegation(n->left), pushNegation(n->right));
}

static NodePtr distribute(NodePtr n) {
    if (!n) return n;
    if (n->type == NodeType::VAR || n->type == NodeType::NOT) return n;
    auto left = distribute(n->left), right = distribute(n->right);
    if (n->type == NodeType::AND) return mkBin(NodeType::AND, left, right);
    if (n->type == NodeType::OR) {
        if (right->type == NodeType::AND)
            return distribute(mkBin(NodeType::AND,
                mkBin(NodeType::OR, cloneAST(left), right->left),
                mkBin(NodeType::OR, cloneAST(left), right->right)));
        if (left->type == NodeType::AND)
            return distribute(mkBin(NodeType::AND,
                mkBin(NodeType::OR, left->left, cloneAST(right)),
                mkBin(NodeType::OR, left->right, cloneAST(right))));
        return mkBin(NodeType::OR, left, right);
    }
    return mkBin(n->type, left, right);
}

static NodePtr toCNF(NodePtr ast) {
    auto n = eliminateIff(ast);
    n = eliminateImplies(n);
    n = pushNegation(n);
    n = distribute(n);
    return n;
}

// =========================================================
// Clause representation
// =========================================================
struct Literal {
    std::string name;
    bool negated;
    bool operator<(const Literal& o) const {
        if (name != o.name) return name < o.name;
        return negated < o.negated;
    }
    bool operator==(const Literal& o) const { return name == o.name && negated == o.negated; }
};

using Clause = std::vector<Literal>;

static std::string litToString(const Literal& l) {
    return l.negated ? std::string("\xC2\xAC") + l.name : l.name;
}
static std::string clauseToString(const Clause& c) {
    if (c.empty()) return "\xE2\x96\xA1"; // □
    std::string s = "{ ";
    for (size_t i = 0; i < c.size(); i++) {
        if (i > 0) s += ", ";
        s += litToString(c[i]);
    }
    return s + " }";
}

static std::string clauseKey(Clause c) {
    std::vector<std::string> parts;
    for (auto& l : c) parts.push_back((l.negated ? "~" : "") + l.name);
    std::sort(parts.begin(), parts.end());
    std::string key;
    for (size_t i = 0; i < parts.size(); i++) { if (i) key += "|"; key += parts[i]; }
    return key;
}

static Clause dedup(const Clause& c) {
    std::set<std::string> seen;
    Clause out;
    for (auto& l : c) {
        std::string k = (l.negated ? "~" : "") + l.name;
        if (seen.insert(k).second) out.push_back(l);
    }
    return out;
}

static bool isTautology(const Clause& c) {
    for (auto& l : c)
        for (auto& m : c)
            if (l.name == m.name && l.negated != m.negated) return true;
    return false;
}

// =========================================================
// Extract clauses from CNF AST
// =========================================================
static void collectDisjuncts(NodePtr n, Clause& lits) {
    if (n->type == NodeType::OR) {
        collectDisjuncts(n->left, lits);
        collectDisjuncts(n->right, lits);
    } else if (n->type == NodeType::NOT && n->operand->type == NodeType::VAR) {
        lits.push_back({n->operand->name, true});
    } else if (n->type == NodeType::VAR) {
        lits.push_back({n->name, false});
    } else {
        throw std::runtime_error("CNF error: unexpected node");
    }
}

static void collectConjuncts(NodePtr n, std::vector<Clause>& clauses) {
    if (n->type == NodeType::AND) {
        collectConjuncts(n->left, clauses);
        collectConjuncts(n->right, clauses);
    } else {
        Clause c;
        collectDisjuncts(n, c);
        clauses.push_back(c);
    }
}

// =========================================================
// Resolution
// =========================================================
struct ResStep {
    int index;
    int from1, from2;
    std::string literal;
    std::string result;
};

static std::vector<Clause> resolveClause(const Clause& c1, const Clause& c2) {
    std::vector<Clause> resolvents;
    for (auto& l : c1) {
        for (auto& m : c2) {
            if (l.name == m.name && l.negated != m.negated) {
                Clause nc;
                for (auto& x : c1) if (!(x.name == l.name && x.negated == l.negated)) nc.push_back(x);
                for (auto& x : c2) if (!(x.name == m.name && x.negated == m.negated)) nc.push_back(x);
                nc = dedup(nc);
                if (!isTautology(nc)) resolvents.push_back(nc);
            }
        }
    }
    return resolvents;
}

// =========================================================
// Main prover
// =========================================================
static void prove(const std::string& formulaStr) {
    std::cout << "{" << std::endl;

    try {
        // Step 1: Parse
        auto tokens = tokenize(formulaStr);
        Parser parser(tokens);
        auto ast = parser.parse();
        std::string parsedStr = astToString(ast);

        // Step 2: Negate
        auto negated = mkNot(cloneAST(ast));
        std::string negStr = astToString(negated);

        // Step 3: CNF
        auto cnf = toCNF(negated);
        std::string cnfStr = astToString(cnf);

        // Step 4: Extract clauses
        std::vector<Clause> clauses;
        collectConjuncts(cnf, clauses);
        for (auto& c : clauses) c = dedup(c);

        // Step 5: Resolution
        std::set<std::string> knownKeys;
        for (auto& c : clauses) knownKeys.insert(clauseKey(c));
        std::vector<ResStep> resSteps;
        bool found = false;
        int clauseCount = (int)clauses.size();
        const int MAX_ITER = 5000;
        int iter = 0;

        for (auto& c : clauses) if (c.empty()) { found = true; break; }

        if (!found) {
            bool changed = true;
            while (changed && !found && iter < MAX_ITER) {
                changed = false;
                int len = (int)clauses.size();
                for (int i = 0; i < len && !found; i++) {
                    for (int j = i + 1; j < len && !found; j++) {
                        iter++;
                        if (iter > MAX_ITER) break;
                        auto resolvents = resolveClause(clauses[i], clauses[j]);
                        for (auto& r : resolvents) {
                            auto key = clauseKey(r);
                            if (knownKeys.find(key) == knownKeys.end()) {
                                knownKeys.insert(key);
                                clauseCount++;
                                clauses.push_back(r);
                                changed = true;
                                // Find which literal was resolved
                                std::string litName;
                                for (auto& l : clauses[i])
                                    for (auto& m : clauses[j])
                                        if (l.name == m.name && l.negated != m.negated) { litName = l.name; break; }
                                resSteps.push_back({clauseCount, i + 1, j + 1, litName, clauseToString(r)});
                                if (r.empty()) { found = true; break; }
                            }
                        }
                    }
                }
            }
        }

        // Output JSON
        std::cout << "  \"valid\": " << (found ? "true" : "false") << "," << std::endl;
        std::cout << "  \"steps\": [" << std::endl;

        // Step 1
        std::cout << "    {\"type\":\"parse\",\"title\":\"Step 1: 構文解析 (Parsing)\",\"formula\":\"" << jsonEscape(parsedStr) << "\"}," << std::endl;
        // Step 2
        std::cout << "    {\"type\":\"negate\",\"title\":\"Step 2: 否定 (Negation for Refutation)\",\"formula\":\"" << jsonEscape(negStr) << "\"}," << std::endl;
        // Step 3
        std::cout << "    {\"type\":\"cnf\",\"title\":\"Step 3: CNF変換 (Conjunctive Normal Form)\",\"formula\":\"" << jsonEscape(cnfStr) << "\"}," << std::endl;

        // Step 4: Clauses
        std::cout << "    {\"type\":\"clauses\",\"title\":\"Step 4: 節集合の生成 (Clause Set)\",\"clauses\":[";
        for (int i = 0; i < (int)clauses.size() && i < clauseCount - (int)resSteps.size(); i++) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"index\":" << (i+1) << ",\"text\":\"" << jsonEscape(clauseToString(clauses[i])) << "\"}";
        }
        std::cout << "]}," << std::endl;

        // Step 5: Resolution steps
        std::cout << "    {\"type\":\"resolve\",\"title\":\"Step 5: Resolution（導出）\",\"found\":" << (found ? "true" : "false")
                  << ",\"resolutionSteps\":[";
        for (size_t i = 0; i < resSteps.size(); i++) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"index\":" << resSteps[i].index
                      << ",\"from\":[" << resSteps[i].from1 << "," << resSteps[i].from2 << "]"
                      << ",\"literal\":\"" << jsonEscape(resSteps[i].literal) << "\""
                      << ",\"result\":\"" << jsonEscape(resSteps[i].result) << "\"}";
        }
        std::cout << "]}," << std::endl;

        // Step 6: Result
        if (found) {
            std::cout << "    {\"type\":\"result\",\"title\":\"Step 6: 結論\","
                      << "\"content\":\"否定から矛盾（空節 □）が導出されたため、元の式は恒真（トートロジー）であることが証明されました。\","
                      << "\"valid\":true}" << std::endl;
        } else {
            std::cout << "    {\"type\":\"result\",\"title\":\"Step 6: 結論\","
                      << "\"content\":\"否定から空節を導出できなかったため、この式は恒真ではありません（反例が存在します）。\","
                      << "\"valid\":false}" << std::endl;
        }

        std::cout << "  ]" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "  \"error\": \"" << jsonEscape(e.what()) << "\"" << std::endl;
    }

    std::cout << "}" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: resolution <formula>" << std::endl;
        std::cerr << "  Example: resolution \"P | !P\"" << std::endl;
        return 1;
    }
    prove(argv[1]);
    return 0;
}
