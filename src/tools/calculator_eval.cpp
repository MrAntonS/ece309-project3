// src/tools/calculator_eval.cpp — iterative two-stack evaluation (spec §4.6).
#include "tools/calculator.h"

#include <vector>

namespace {
using Kind = CalcToken::Kind;

int precedence(Kind op) {
    switch (op) {
        case Kind::Plus:
        case Kind::Minus:   return 1;
        case Kind::Times:
        case Kind::Divide:  return 2;
        default:            return 0;
    }
}
}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;

    // Returns an error as data instead of letting a stack underflow.
    auto apply = [&]() -> bool {
        if (ops.empty() || values.size() < 2) {
            *error = "invalid expression: missing operand or operator";
            return false;
        }
        const Kind op = ops.back();
        ops.pop_back();
        const double right = values.back();
        values.pop_back();
        const double left = values.back();
        values.pop_back();

        double answer = 0.0;
        switch (op) {
            case Kind::Plus:    answer = left + right; break;
            case Kind::Minus:   answer = left - right; break;
            case Kind::Times:   answer = left * right; break;
            case Kind::Divide:
                if (right == 0.0) {
                    *error = "division by zero";
                    return false;
                }
                answer = left / right;
                break;
            default:
                *error = "invalid expression: unexpected operator";
                return false;
        }
        values.push_back(answer);
        return true;
    };

    for (const CalcToken& token : tokens) {
        switch (token.kind) {
            case Kind::Number:
                values.push_back(token.value);
                break;
            case Kind::LParen:
                ops.push_back(token.kind);
                break;
            case Kind::RParen:
                while (!ops.empty() && ops.back() != Kind::LParen)
                    if (!apply()) return false;
                if (ops.empty()) {
                    *error = "invalid expression: unmatched ')'";
                    return false;
                }
                ops.pop_back();
                break;
            case Kind::Plus:
            case Kind::Minus:
            case Kind::Times:
            case Kind::Divide:
                while (!ops.empty() && ops.back() != Kind::LParen &&
                       precedence(ops.back()) >= precedence(token.kind))
                    if (!apply()) return false;
                ops.push_back(token.kind);
                break;
        }
    }
    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "invalid expression: missing ')'";
            return false;
        }
        if (!apply()) return false;
    }
    if (values.size() != 1) {
        *error = "invalid expression: expected one result";
        return false;
    }
    *out = values.back();
    error->clear();
    return true;
}
