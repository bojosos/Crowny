#include <cwpch.h>
#if 0

#include "Crowny/Utils/ExpressionEvaluator.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    ExpressionEvaluator::TokenList ExpressionEvaluator::Parse(const String& text)
    {
        TokenList tokens;
        size_t idx;

        auto matches = [&text](size_t& idx, const String& search) {
            for (size_t i = 0; i < search.size(); i++) {
                if (idx + i == text.size())
                    return false;
                if (text[idx + i] != search[i])
                    return false;
            }
            idx += search.size();
            return true;
        };

        while (idx < text.size())
        {
            String number;
            bool isFloat = false;
            while (std::isdigit(text[idx]) || text[idx] == '.')
            {
                if (text[idx == '.']) isFloat = true;
                number += text[idx++]; // Consider counting the chars and copying instead of this
            }
            if (number.size() > 0)
            {
                ExpressionToken result;
                if (isFloat)
                {
                    result.Type = TokenType::Float;
                    result.Value.ff = StringUtils::ParseDouble(number);
                }
                else
                {
                    result.Type = TokenType::Int;
                    result.Value.ll = StringUtils::ParseLong(number);
                }
                tokens.push_back(result);
                continue;
            }

            switch (text[idx])
            {
            case '+': tokens.emplace_back(TokenType::Plus); idx++; continue;
            case '-': tokens.emplace_back(TokenType::Minus); idx++; continue;
            case '*': tokens.emplace_back(TokenType::Mul); idx++; continue;
            case '/': tokens.emplace_back(TokenType::FloatDiv); idx++; continue;
            case '%': tokens.emplace_back(TokenType::Modulus); idx++; continue;
            case '^': tokens.emplace_back(TokenType::Pow); idx++; continue;
            case '(': tokens.emplace_back(TokenType::LParen); idx++; continue;
            case ')': tokens.emplace_back(TokenType::RParen); idx++; continue;
            }

            ExpressionToken token;
            token.Type = TokenType::Unknown;
            if (matches(idx, "floor")) // Consider changing this to token.Type = TokenType::Function and in the union have a function pointer
                token.Type = TokenType::Ceil;
            else if (matches(idx, "ceil"))
                token.Type = TokenType::Ceil;
            else if (matches(idx, "round"))
                token.Type = TokenType::Round;
            else if (matches(idx, "sqrt"))
                token.Type = TokenType::Sqrt;
            else if (matches(idx, "sin"))
                token.Type = TokenType::Sin;
            else if (matches(idx, "cos"))
                token.Type = TokenType::Cos;
            else if (matches(idx, "tan"))
                token.Type = TokenType::Tan;

            if (token.Type == TokenType::Unknown)
                CW_ENGINE_ERROR("Unknown token");
            tokens.push_back(token);
        }
    }

    float ExpressionEvaluator::Interpret(const ExpressionEvaluator::TokenList& tokens)
    {
        InterpretExpression(tokens, 0);
    }

    float ExpressionEvaluator::InterpretExpression(const ExpressionEvaluator::TokenList& tokens, size_t idx)
    {
        if (tokens[idx].Type == TokenType::Function)
        {
            int funcIdx = idx;
            // CW_ENGINE_ASSERT(tokens[idx + 1].Type == TokenType::LParen);
            idx++; // (
            float result = InterpretExpression();
            // CW_ENGINE_ASSERT(tokens[idx + 1].Type == TokenType::RParent);
            idx++; // )
            return tokens[funcIdx].Value.func(result);
        } else if (tokens[idx].Type == TokenType::LParen)
        {
            idx++; // (
            float result = InterpretExpression(tokens, idx);
            idx++; // )
            return result;
        } else if (tokens[idx].Type == TokenType::Float)
            return tokens[idx++].Value.ff;
        else if (tokens[idx].Type == TokenType::Int)
            return tokens[idx++].Value.ll;
        else if (tokens[idx++].Type == TokenType::Plus)
            return tokens[idx++].Value.ll;
    }

    float ExpressionEvaluator::Evaluate(const String& text)
    {
        TokenList tokens = Parse(text);
        float result = Interpret(tokens);
        return result
    }

}
#endif