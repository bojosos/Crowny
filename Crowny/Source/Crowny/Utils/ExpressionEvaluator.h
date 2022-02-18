#if 0
#pragma once

namespace Crowny
{

    class ExpressionEvaluator
    {
        enum class TokenType
        {
            Plus,        // +
            Minus,       // -
            FloatDiv,    // /
            Mul,         // *
            Modulus,     // % -> fmod
            Pow,         // ^
  
            Int,         // integer number
            Float,       // float number
  
            Sqrt,        // sqrt
            Floor,       // floor
            Ceil,        // ceil
            Round,       // floor
            Sin,         // sin
            Cos,         // cos
            Tan,         // tan
 
            LParen,      // (
            RParen,      // )

            Unknown      // utility token
        };
        struct ExpressionToken
        {
            ExpressionToken() = default;
            ExpressionToken(TokenType type) : Type(type) { }

            TokenType Type;
            union {
                long ll;
                double ff;
                std::function<float(float)> func;

            } Value;
        };
    public:
        static float Evaluate(const String& text);
        using TokenList = Vector<ExpressionToken>;

    private:
        static float Interpret(const TokenList& tokens);
        static float InterpretExpression(const TokenList& tokens, size_t idx);
        static TokenList Parse(const String& text);
    };

}
#endif