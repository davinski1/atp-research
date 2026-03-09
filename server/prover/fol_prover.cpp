/**
 * ATP Prover - First-Order Logic Prover (C++)
 *
 * Ported from fol-prover.js — same algorithm:
 *   1. Tokenizer (Prover9-style syntax)
 *   2. Recursive-descent Parser
 *   3. NNF + Skolemization + CNF
 *   4. Unification-based Resolution
 *
 * Usage: ./fol_prover "all x (P(x) -> Q(x)) & all x P(x) -> all x Q(x)"
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
#include <functional>
#include <stdexcept>

// =========================================================
// JSON helper
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
enum class TokType { IDENT, ALL, EXISTS, NOT, AND, OR, IMPLIES, IFF, LPAREN, RPAREN, COMMA, DOT, EOFINPUT };

struct Token {
    TokType type;
    std::string value;
};

// =========================================================
// AST
// =========================================================
enum class NType { PRED, NOT, AND, OR, IMPLIES, IFF, ALL, EXISTS, VAR, FUNC };

struct Node {
    NType type;
    std::string name;
    std::vector<std::shared_ptr<Node>> args; // PRED, FUNC
    std::shared_ptr<Node> operand;           // NOT
    std::shared_ptr<Node> left, right;       // binary
    std::string variable;                    // ALL, EXISTS
    std::shared_ptr<Node> body;              // ALL, EXISTS
    bool negated = false;                    // for literal in clause
};

using NP = std::shared_ptr<Node>;

static NP mkVar(const std::string& n) { auto p = std::make_shared<Node>(); p->type = NType::VAR; p->name = n; return p; }
static NP mkFunc(const std::string& n, std::vector<NP> a) { auto p = std::make_shared<Node>(); p->type = NType::FUNC; p->name = n; p->args = std::move(a); return p; }
static NP mkPred(const std::string& n, std::vector<NP> a) { auto p = std::make_shared<Node>(); p->type = NType::PRED; p->name = n; p->args = std::move(a); return p; }
static NP mkNot(NP op) { auto p = std::make_shared<Node>(); p->type = NType::NOT; p->operand = op; return p; }
static NP mkBin(NType t, NP l, NP r) { auto p = std::make_shared<Node>(); p->type = t; p->left = l; p->right = r; return p; }
static NP mkQuant(NType t, const std::string& v, NP b) { auto p = std::make_shared<Node>(); p->type = t; p->variable = v; p->body = b; return p; }

static NP clone(NP n) {
    if (!n) return nullptr;
    auto p = std::make_shared<Node>();
    p->type = n->type; p->name = n->name; p->variable = n->variable; p->negated = n->negated;
    for (auto& a : n->args) p->args.push_back(clone(a));
    p->operand = clone(n->operand); p->left = clone(n->left); p->right = clone(n->right); p->body = clone(n->body);
    return p;
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
        if (ch == '(') { tokens.push_back({TokType::LPAREN}); i++; continue; }
        if (ch == ')') { tokens.push_back({TokType::RPAREN}); i++; continue; }
        if (ch == ',') { tokens.push_back({TokType::COMMA}); i++; continue; }
        if (ch == '.') { tokens.push_back({TokType::DOT}); i++; continue; }
        if (ch == '<' && i+2 < input.size() && input[i+1] == '-' && input[i+2] == '>') { tokens.push_back({TokType::IFF}); i+=3; continue; }
        if (ch == '-' && i+1 < input.size() && input[i+1] == '>') { tokens.push_back({TokType::IMPLIES}); i+=2; continue; }
        if (ch == '-' || ch == '!' || ch == '~') { tokens.push_back({TokType::NOT}); i++; continue; }
        if (ch == '&') { tokens.push_back({TokType::AND}); i++; continue; }
        if (ch == '|') { tokens.push_back({TokType::OR}); i++; continue; }
        // UTF-8 operators
        if ((unsigned char)ch == 0xC2 && i+1 < input.size() && (unsigned char)input[i+1] == 0xAC) { tokens.push_back({TokType::NOT}); i+=2; continue; }
        if ((unsigned char)ch == 0xE2 && i+2 < input.size()) {
            unsigned char b1 = input[i+1], b2 = input[i+2];
            if (b1 == 0x88 && b2 == 0xA7) { tokens.push_back({TokType::AND}); i+=3; continue; }
            if (b1 == 0x88 && b2 == 0xA8) { tokens.push_back({TokType::OR}); i+=3; continue; }
            if (b1 == 0x86 && b2 == 0x92) { tokens.push_back({TokType::IMPLIES}); i+=3; continue; }
            if (b1 == 0x86 && b2 == 0x94) { tokens.push_back({TokType::IFF}); i+=3; continue; }
        }
        if (std::isalpha(ch) || ch == '_') {
            std::string name; while (i < input.size() && (std::isalnum(input[i]) || input[i] == '_')) { name += input[i]; i++; }
            if (name == "all" || name == "ALL") { tokens.push_back({TokType::ALL}); continue; }
            if (name == "exists" || name == "EXISTS") { tokens.push_back({TokType::EXISTS}); continue; }
            tokens.push_back({TokType::IDENT, name}); continue;
        }
        throw std::runtime_error(std::string("FOL: unknown char '") + ch + "'");
    }
    tokens.push_back({TokType::EOFINPUT});
    return tokens;
}

// =========================================================
// Parser
// =========================================================
class Parser {
    std::vector<Token>& tk;
    size_t pos = 0;
    Token& peek() { return tk[pos]; }
    Token advance() { return tk[pos++]; }
public:
    Parser(std::vector<Token>& t) : tk(t) {}

    NP parseIff() {
        auto l = parseImplies();
        while (peek().type == TokType::IFF) { advance(); l = mkBin(NType::IFF, l, parseImplies()); }
        return l;
    }
    NP parseImplies() {
        auto l = parseOr();
        if (peek().type == TokType::IMPLIES) { advance(); return mkBin(NType::IMPLIES, l, parseImplies()); }
        return l;
    }
    NP parseOr() {
        auto l = parseAnd();
        while (peek().type == TokType::OR) { advance(); l = mkBin(NType::OR, l, parseAnd()); }
        return l;
    }
    NP parseAnd() {
        auto l = parseUnary();
        while (peek().type == TokType::AND) { advance(); l = mkBin(NType::AND, l, parseUnary()); }
        return l;
    }
    NP parseUnary() {
        if (peek().type == TokType::NOT) { advance(); return mkNot(parseUnary()); }
        if (peek().type == TokType::ALL) { advance(); auto v = advance(); return mkQuant(NType::ALL, v.value, parseUnary()); }
        if (peek().type == TokType::EXISTS) { advance(); auto v = advance(); return mkQuant(NType::EXISTS, v.value, parseUnary()); }
        return parseAtom();
    }
    NP parseAtom() {
        if (peek().type == TokType::LPAREN) { advance(); auto n = parseIff(); if (peek().type == TokType::RPAREN) advance(); return n; }
        if (peek().type == TokType::IDENT) {
            auto name = advance().value;
            if (peek().type == TokType::LPAREN) {
                advance();
                std::vector<NP> args;
                if (peek().type != TokType::RPAREN) {
                    args.push_back(parseTerm());
                    while (peek().type == TokType::COMMA) { advance(); args.push_back(parseTerm()); }
                }
                if (peek().type == TokType::RPAREN) advance();
                return mkPred(name, std::move(args));
            }
            return mkPred(name, {});
        }
        throw std::runtime_error("FOL: unexpected token");
    }
    NP parseTerm() {
        if (peek().type == TokType::IDENT) {
            auto name = advance().value;
            if (peek().type == TokType::LPAREN) {
                advance();
                std::vector<NP> args;
                if (peek().type != TokType::RPAREN) {
                    args.push_back(parseTerm());
                    while (peek().type == TokType::COMMA) { advance(); args.push_back(parseTerm()); }
                }
                if (peek().type == TokType::RPAREN) advance();
                return mkFunc(name, std::move(args));
            }
            return mkVar(name);
        }
        throw std::runtime_error("FOL: expected term");
    }
    NP parse() {
        auto ast = parseIff();
        // Allow trailing dot
        if (peek().type == TokType::DOT) advance();
        return ast;
    }
};

// =========================================================
// AST to string
// =========================================================
static std::string termStr(NP n) {
    if (n->type == NType::VAR) return n->name;
    if (n->type == NType::FUNC) {
        std::string s = n->name + "(";
        for (size_t i = 0; i < n->args.size(); i++) { if (i) s += ", "; s += termStr(n->args[i]); }
        return s + ")";
    }
    return "?";
}

static std::string nodeStr(NP n) {
    if (!n) return "?";
    switch (n->type) {
        case NType::PRED:
            if (n->args.empty()) return n->name;
            { std::string s = n->name + "("; for (size_t i = 0; i < n->args.size(); i++) { if (i) s += ", "; s += termStr(n->args[i]); } return s + ")"; }
        case NType::NOT: {
            auto inner = nodeStr(n->operand);
            if (n->operand->type != NType::PRED) inner = "(" + inner + ")";
            return "\xC2\xAC" + inner;
        }
        case NType::AND: return "(" + nodeStr(n->left) + " \xE2\x88\xA7 " + nodeStr(n->right) + ")";
        case NType::OR:  return "(" + nodeStr(n->left) + " \xE2\x88\xA8 " + nodeStr(n->right) + ")";
        case NType::IMPLIES: return "(" + nodeStr(n->left) + " \xE2\x86\x92 " + nodeStr(n->right) + ")";
        case NType::IFF: return "(" + nodeStr(n->left) + " \xE2\x86\x94 " + nodeStr(n->right) + ")";
        case NType::ALL: return "\xE2\x88\x80" + n->variable + ". " + nodeStr(n->body);
        case NType::EXISTS: return "\xE2\x88\x83" + n->variable + ". " + nodeStr(n->body);
        default: return "?";
    }
}

// =========================================================
// Substitution
// =========================================================
static NP subst(NP node, const std::string& v, NP t) {
    if (!node) return nullptr;
    switch (node->type) {
        case NType::VAR: return node->name == v ? clone(t) : clone(node);
        case NType::FUNC: { std::vector<NP> a; for (auto& x : node->args) a.push_back(subst(x, v, t)); return mkFunc(node->name, std::move(a)); }
        case NType::PRED: { std::vector<NP> a; for (auto& x : node->args) a.push_back(subst(x, v, t)); return mkPred(node->name, std::move(a)); }
        case NType::NOT: return mkNot(subst(node->operand, v, t));
        case NType::ALL: if (node->variable == v) return clone(node); return mkQuant(NType::ALL, node->variable, subst(node->body, v, t));
        case NType::EXISTS: if (node->variable == v) return clone(node); return mkQuant(NType::EXISTS, node->variable, subst(node->body, v, t));
        default: return mkBin(node->type, subst(node->left, v, t), subst(node->right, v, t));
    }
}

// =========================================================
// NNF: Eliminate IFF/IMPLIES, push negation
// =========================================================
static NP elimIff(NP n) {
    if (!n) return n;
    switch (n->type) {
        case NType::PRED: case NType::VAR: case NType::FUNC: return n;
        case NType::NOT: return mkNot(elimIff(n->operand));
        case NType::IFF: { auto l = elimIff(n->left), r = elimIff(n->right);
            return mkBin(NType::AND, mkBin(NType::IMPLIES, clone(l), clone(r)), mkBin(NType::IMPLIES, clone(r), clone(l))); }
        case NType::IMPLIES: { auto l = elimIff(n->left), r = elimIff(n->right);
            return mkBin(NType::OR, mkNot(l), r); }
        case NType::ALL: return mkQuant(NType::ALL, n->variable, elimIff(n->body));
        case NType::EXISTS: return mkQuant(NType::EXISTS, n->variable, elimIff(n->body));
        default: return mkBin(n->type, elimIff(n->left), elimIff(n->right));
    }
}

static NP nnf(NP n) {
    if (!n) return n;
    switch (n->type) {
        case NType::PRED: case NType::VAR: case NType::FUNC: return n;
        case NType::NOT: {
            auto inner = n->operand;
            if (inner->type == NType::NOT) return nnf(inner->operand);
            if (inner->type == NType::AND) return nnf(mkBin(NType::OR, mkNot(inner->left), mkNot(inner->right)));
            if (inner->type == NType::OR) return nnf(mkBin(NType::AND, mkNot(inner->left), mkNot(inner->right)));
            if (inner->type == NType::ALL) return nnf(mkQuant(NType::EXISTS, inner->variable, mkNot(inner->body)));
            if (inner->type == NType::EXISTS) return nnf(mkQuant(NType::ALL, inner->variable, mkNot(inner->body)));
            return mkNot(nnf(inner));
        }
        case NType::ALL: return mkQuant(NType::ALL, n->variable, nnf(n->body));
        case NType::EXISTS: return mkQuant(NType::EXISTS, n->variable, nnf(n->body));
        default: return mkBin(n->type, nnf(n->left), nnf(n->right));
    }
}

// =========================================================
// Skolemization
// =========================================================
static int skolemCtr = 0;

static NP skolemize(NP n, std::vector<std::string>& univVars) {
    if (!n) return n;
    switch (n->type) {
        case NType::ALL: {
            univVars.push_back(n->variable);
            auto r = skolemize(n->body, univVars);
            univVars.pop_back();
            return mkQuant(NType::ALL, n->variable, r);
        }
        case NType::EXISTS: {
            skolemCtr++;
            std::string skName = "sk" + std::to_string(skolemCtr);
            NP replacement;
            if (univVars.empty()) {
                replacement = mkVar(skName);
            } else {
                std::vector<NP> a;
                for (auto& v : univVars) a.push_back(mkVar(v));
                replacement = mkFunc(skName, std::move(a));
            }
            auto body = subst(n->body, n->variable, replacement);
            return skolemize(body, univVars);
        }
        case NType::NOT: return mkNot(skolemize(n->operand, univVars));
        case NType::PRED: case NType::VAR: case NType::FUNC: return n;
        default: return mkBin(n->type, skolemize(n->left, univVars), skolemize(n->right, univVars));
    }
}

static NP dropUniv(NP n) {
    if (!n) return n;
    if (n->type == NType::ALL) return dropUniv(n->body);
    if (n->type == NType::NOT) return mkNot(dropUniv(n->operand));
    if (n->type == NType::PRED || n->type == NType::VAR || n->type == NType::FUNC) return n;
    return mkBin(n->type, dropUniv(n->left), dropUniv(n->right));
}

static NP distrib(NP n) {
    if (!n) return n;
    if (n->type == NType::PRED || n->type == NType::NOT) return n;
    auto l = distrib(n->left), r = distrib(n->right);
    if (n->type == NType::AND) return mkBin(NType::AND, l, r);
    if (n->type == NType::OR) {
        if (r->type == NType::AND) return distrib(mkBin(NType::AND, mkBin(NType::OR, clone(l), r->left), mkBin(NType::OR, clone(l), r->right)));
        if (l->type == NType::AND) return distrib(mkBin(NType::AND, mkBin(NType::OR, l->left, clone(r)), mkBin(NType::OR, l->right, clone(r))));
        return mkBin(NType::OR, l, r);
    }
    return mkBin(n->type, l, r);
}

// =========================================================
// Clause representation for FOL
// =========================================================
struct Lit {
    bool negated;
    std::string predName;
    std::vector<NP> args;
};

static std::string litStr(const Lit& l) {
    std::string s;
    if (l.negated) s += "\xC2\xAC";
    s += l.predName;
    if (!l.args.empty()) {
        s += "(";
        for (size_t i = 0; i < l.args.size(); i++) { if (i) s += ", "; s += termStr(l.args[i]); }
        s += ")";
    }
    return s;
}

static std::string clauseStr(const std::vector<Lit>& c) {
    if (c.empty()) return "\xE2\x96\xA1";
    std::string s = "{ ";
    for (size_t i = 0; i < c.size(); i++) { if (i) s += ", "; s += litStr(c[i]); }
    return s + " }";
}

using Clause = std::vector<Lit>;

// Extract clauses from CNF
static void collectOr(NP n, Clause& lits) {
    if (n->type == NType::OR) { collectOr(n->left, lits); collectOr(n->right, lits); }
    else if (n->type == NType::NOT && n->operand->type == NType::PRED) {
        lits.push_back({true, n->operand->name, n->operand->args});
    }
    else if (n->type == NType::PRED) {
        lits.push_back({false, n->name, n->args});
    }
}

static void collectAnd(NP n, std::vector<Clause>& clauses) {
    if (n->type == NType::AND) { collectAnd(n->left, clauses); collectAnd(n->right, clauses); }
    else { Clause c; collectOr(n, c); clauses.push_back(c); }
}

// =========================================================
// Unification
// =========================================================
using Subst = std::map<std::string, NP>;

static bool termEq(NP a, NP b) {
    if (a->type != b->type) return false;
    if (a->type == NType::VAR) return a->name == b->name;
    if (a->type == NType::FUNC) {
        if (a->name != b->name || a->args.size() != b->args.size()) return false;
        for (size_t i = 0; i < a->args.size(); i++) if (!termEq(a->args[i], b->args[i])) return false;
        return true;
    }
    return false;
}

static bool occurs(const std::string& v, NP t) {
    if (t->type == NType::VAR) return t->name == v;
    if (t->type == NType::FUNC) for (auto& a : t->args) if (occurs(v, a)) return true;
    return false;
}

static NP applySubst(NP t, const Subst& s) {
    if (t->type == NType::VAR) {
        auto it = s.find(t->name);
        if (it != s.end()) return applySubst(it->second, s);
        return t;
    }
    if (t->type == NType::FUNC) {
        std::vector<NP> a;
        for (auto& x : t->args) a.push_back(applySubst(x, s));
        return mkFunc(t->name, std::move(a));
    }
    return t;
}

static bool unify(NP t1, NP t2, Subst& s) {
    t1 = applySubst(t1, s);
    t2 = applySubst(t2, s);
    if (termEq(t1, t2)) return true;
    if (t1->type == NType::VAR) { if (occurs(t1->name, t2)) return false; s[t1->name] = t2; return true; }
    if (t2->type == NType::VAR) { if (occurs(t2->name, t1)) return false; s[t2->name] = t1; return true; }
    if (t1->type == NType::FUNC && t2->type == NType::FUNC) {
        if (t1->name != t2->name || t1->args.size() != t2->args.size()) return false;
        for (size_t i = 0; i < t1->args.size(); i++) if (!unify(t1->args[i], t2->args[i], s)) return false;
        return true;
    }
    return false;
}

static bool unifyPreds(const Lit& a, const Lit& b, Subst& s) {
    if (a.predName != b.predName || a.args.size() != b.args.size()) return false;
    for (size_t i = 0; i < a.args.size(); i++) if (!unify(a.args[i], b.args[i], s)) return false;
    return true;
}

static Lit applySubstLit(const Lit& l, const Subst& s) {
    std::vector<NP> a;
    for (auto& x : l.args) a.push_back(applySubst(x, s));
    return {l.negated, l.predName, std::move(a)};
}

// Variable renaming
static int renameCtr = 0;
static Clause renameClause(const Clause& c) {
    renameCtr++;
    std::map<std::string, std::string> mapping;
    std::function<NP(NP)> ren = [&](NP t) -> NP {
        if (t->type == NType::VAR) {
            auto it = mapping.find(t->name);
            if (it == mapping.end()) { mapping[t->name] = t->name + "_" + std::to_string(renameCtr); }
            return mkVar(mapping[t->name]);
        }
        if (t->type == NType::FUNC) { std::vector<NP> a; for (auto& x : t->args) a.push_back(ren(x)); return mkFunc(t->name, std::move(a)); }
        return t;
    };
    Clause out;
    for (auto& l : c) {
        std::vector<NP> a;
        for (auto& x : l.args) a.push_back(ren(x));
        out.push_back({l.negated, l.predName, std::move(a)});
    }
    return out;
}

// =========================================================
// FOL Resolution
// =========================================================
struct ResStep {
    int index, from1, from2;
    std::string literal, result;
};

static std::vector<Clause> folResolve(const Clause& c1orig, const Clause& c2orig) {
    auto c1 = renameClause(c1orig);
    auto c2 = renameClause(c2orig);
    std::vector<Clause> resolvents;
    for (size_t i = 0; i < c1.size(); i++) {
        for (size_t j = 0; j < c2.size(); j++) {
            if (c1[i].negated != c2[j].negated) {
                Subst s;
                if (unifyPreds(c1[i], c2[j], s)) {
                    Clause nc;
                    for (size_t k = 0; k < c1.size(); k++) if (k != i) nc.push_back(applySubstLit(c1[k], s));
                    for (size_t k = 0; k < c2.size(); k++) if (k != j) nc.push_back(applySubstLit(c2[k], s));
                    // Dedup
                    std::set<std::string> seen;
                    Clause deduped;
                    for (auto& l : nc) { auto k = litStr(l); if (seen.insert(k).second) deduped.push_back(l); }
                    resolvents.push_back(deduped);
                }
            }
        }
    }
    return resolvents;
}

// =========================================================
// Main prover
// =========================================================
static void prove(const std::string& formulaStr) {
    skolemCtr = 0;
    renameCtr = 0;
    std::cout << "{" << std::endl;

    try {
        // Step 1: Parse
        auto tokens = tokenize(formulaStr);
        Parser parser(tokens);
        auto ast = parser.parse();
        std::string parsedStr = nodeStr(ast);

        // Step 2: Negate
        auto negated = mkNot(clone(ast));
        std::string negStr = nodeStr(negated);

        // Step 3: NNF
        auto processed = elimIff(negated);
        processed = nnf(processed);
        std::string nnfStr = nodeStr(processed);

        // Step 4: Skolemize
        std::vector<std::string> univVars;
        processed = skolemize(processed, univVars);
        processed = dropUniv(processed);
        std::string skolStr = nodeStr(processed);

        // Step 5: CNF + clauses
        processed = distrib(processed);
        std::vector<Clause> clauses;
        collectAnd(processed, clauses);
        int origCount = (int)clauses.size();

        // Step 6: Resolution
        std::set<std::string> knownKeys;
        for (auto& c : clauses) knownKeys.insert(clauseStr(c));
        std::vector<ResStep> resSteps;
        bool found = false;
        int clauseCount = origCount;
        const int MAX_ITER = 3000;
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
                        auto resolvents = folResolve(clauses[i], clauses[j]);
                        for (auto& r : resolvents) {
                            auto key = clauseStr(r);
                            if (knownKeys.find(key) == knownKeys.end()) {
                                knownKeys.insert(key);
                                clauseCount++;
                                clauses.push_back(r);
                                changed = true;
                                resSteps.push_back({clauseCount, i+1, j+1, "", clauseStr(r)});
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
        std::cout << "    {\"type\":\"parse\",\"title\":\"Step 1: 構文解析 (Parsing)\",\"formula\":\"" << jsonEscape(parsedStr) << "\"}," << std::endl;
        std::cout << "    {\"type\":\"negate\",\"title\":\"Step 2: 否定 (Negation for Refutation)\",\"formula\":\"" << jsonEscape(negStr) << "\"}," << std::endl;
        std::cout << "    {\"type\":\"cnf\",\"title\":\"Step 3: 否定標準形 (NNF) への変換\",\"formula\":\"" << jsonEscape(nnfStr) << "\"}," << std::endl;
        std::cout << "    {\"type\":\"cnf\",\"title\":\"Step 4: スコーレム化 & 全称量化子の除去\",\"formula\":\"" << jsonEscape(skolStr) << "\"}," << std::endl;

        // Clauses
        std::cout << "    {\"type\":\"clauses\",\"title\":\"Step 5: 節集合の生成 (Clause Set)\",\"clauses\":[";
        for (int i = 0; i < origCount; i++) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"index\":" << (i+1) << ",\"text\":\"" << jsonEscape(clauseStr(clauses[i])) << "\"}";
        }
        std::cout << "]}," << std::endl;

        // Resolution
        std::cout << "    {\"type\":\"resolve\",\"title\":\"Step 6: Resolution（導出）\",\"found\":" << (found?"true":"false") << ",\"resolutionSteps\":[";
        for (size_t i = 0; i < resSteps.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"index\":" << resSteps[i].index
                      << ",\"from\":[" << resSteps[i].from1 << "," << resSteps[i].from2 << "]"
                      << ",\"literal\":\"" << jsonEscape(resSteps[i].literal) << "\""
                      << ",\"result\":\"" << jsonEscape(resSteps[i].result) << "\"}";
        }
        std::cout << "]}," << std::endl;

        if (found) {
            std::cout << "    {\"type\":\"result\",\"title\":\"Step 7: 結論\","
                      << "\"content\":\"否定から矛盾が導出されたため、元の式は恒真です。\","
                      << "\"valid\":true}" << std::endl;
        } else {
            std::cout << "    {\"type\":\"result\",\"title\":\"Step 7: 結論\","
                      << "\"content\":\"空節を導出できなかったため、恒真性は確認できません。\","
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
        std::cerr << "Usage: fol_prover <formula>" << std::endl;
        std::cerr << "  Example: fol_prover \"all x (P(x) -> Q(x)) & all x P(x) -> all x Q(x)\"" << std::endl;
        return 1;
    }
    prove(argv[1]);
    return 0;
}
