#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{

    class ExpressionEvaluator
    {
    public:
        static float Evaluate(const String& text);

    private:
        struct Context
        {
            const char* Ptr;
            void SkipWhitespace()
            {
                while (*Ptr && std::isspace(*Ptr))
                    ++Ptr;
            }
        };

        static float ParseExpression(Context& ctx);
        static float ParseTerm(Context& ctx);
        static float ParseFactor(Context& ctx);
        static float ParsePower(Context& ctx);
        static float ParseUnary(Context& ctx);
        static float ParsePrimary(Context& ctx);
        static bool Matches(Context& ctx, const char* name);
    };

}
