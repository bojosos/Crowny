#include "cwpch.h"

#include "Crowny/Utils/ExpressionEvaluator.h"
#include <cmath>
#include <cstdlib>

namespace Crowny
{

    float ExpressionEvaluator::Evaluate(const String& text)
    {
        Context ctx;
        ctx.Ptr = text.c_str();
        return ParseExpression(ctx);
    }

    float ExpressionEvaluator::ParseExpression(Context& ctx)
    {
        float result = ParseTerm(ctx);
        ctx.SkipWhitespace();
        while (*ctx.Ptr == '+' || *ctx.Ptr == '-')
        {
            char op = *ctx.Ptr++;
            float right = ParseTerm(ctx);
            if (op == '+')
                result += right;
            else
                result -= right;
            ctx.SkipWhitespace();
        }
        return result;
    }

    float ExpressionEvaluator::ParseTerm(Context& ctx)
    {
        float result = ParseFactor(ctx);
        ctx.SkipWhitespace();
        while (*ctx.Ptr == '*' || *ctx.Ptr == '/' || *ctx.Ptr == '%')
        {
            char op = *ctx.Ptr++;
            float right = ParseFactor(ctx);
            if (op == '*')
                result *= right;
            else if (op == '/')
                result /= right;
            else
                result = std::fmod(result, right);
            ctx.SkipWhitespace();
        }
        return result;
    }

    float ExpressionEvaluator::ParseFactor(Context& ctx)
    {
        float result = ParsePower(ctx);
        return result;
    }

    float ExpressionEvaluator::ParsePower(Context& ctx)
    {
        float result = ParseUnary(ctx);
        ctx.SkipWhitespace();
        while (*ctx.Ptr == '^')
        {
            ctx.Ptr++;
            float right = ParsePower(ctx); // Right associative
            result = std::pow(result, right);
            ctx.SkipWhitespace();
        }
        return result;
    }

    float ExpressionEvaluator::ParseUnary(Context& ctx)
    {
        ctx.SkipWhitespace();
        if (*ctx.Ptr == '+')
        {
            ctx.Ptr++;
            return ParseUnary(ctx);
        }
        if (*ctx.Ptr == '-')
        {
            ctx.Ptr++;
            return -ParseUnary(ctx);
        }
        return ParsePrimary(ctx);
    }

    bool ExpressionEvaluator::Matches(ExpressionEvaluator::Context& ctx, const char* name)
    {
        size_t len = std::strlen(name);
        if (std::strncmp(ctx.Ptr, name, len) == 0)
        {
            // Ensure it's not a prefix of a longer name
            char next = ctx.Ptr[len];
            if (!std::isalnum(next) && next != '_')
            {
                ctx.Ptr += len;
                return true;
            }
        }
        return false;
    }

    float ExpressionEvaluator::ParsePrimary(Context& ctx)
    {
        ctx.SkipWhitespace();
        if (*ctx.Ptr == '(')
        {
            ctx.Ptr++;
            float result = ParseExpression(ctx);
            ctx.SkipWhitespace();
            if (*ctx.Ptr == ')')
                ctx.Ptr++;
            return result;
        }

        if (std::isdigit(*ctx.Ptr) || *ctx.Ptr == '.')
        {
            char* end;
            float result = std::strtof(ctx.Ptr, &end);
            ctx.Ptr = end;
            return result;
        }

        if (Matches(ctx, "sqrt"))
            return std::sqrt(ParsePrimary(ctx));
        if (Matches(ctx, "sin"))
            return std::sin(ParsePrimary(ctx));
        if (Matches(ctx, "cos"))
            return std::cos(ParsePrimary(ctx));
        if (Matches(ctx, "tan"))
            return std::tan(ParsePrimary(ctx));
        if (Matches(ctx, "floor"))
            return std::floor(ParsePrimary(ctx));
        if (Matches(ctx, "ceil"))
            return std::ceil(ParsePrimary(ctx));
        if (Matches(ctx, "round"))
            return std::round(ParsePrimary(ctx));
        if (Matches(ctx, "abs"))
            return std::abs(ParsePrimary(ctx));

        // Skip unknown identifiers
        if (std::isalpha(*ctx.Ptr) || *ctx.Ptr == '_')
        {
            ctx.Ptr++;
            while (std::isalnum(*ctx.Ptr) || *ctx.Ptr == '_')
                ctx.Ptr++;
        }

        return 0.0f;
    }

} // namespace Crowny
