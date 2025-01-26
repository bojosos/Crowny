#pragma once

namespace Crowny
{
    class ShaderPass
    {
    };

    class ShaderVariationDesc
    {
    };

    class ShaderDefines
    {
    public:
        void Set(const String& name, int value);
        void Set(const String& name, float value);
        void Set(const String& name, const String& value);
        const UnorderedMap<String, String>& Get() const;

    private:
        UnorderedMap<String, String> m_Defines;
    };

    class ShaderVariation
    {
    public:
        struct Specifier
        {
            enum SpecifierType
            {
                Int,
                Float,
                Bool
            };
            Specifier(const String& name, int32_t value) : I(value), Name(name), Type(Int) {}
            Specifier(const String& name, bool value) : I(value), Name(name), Type(Bool) {}
            Specifier(const String& name, float value) : F(value), Name(name), Type(Float) {}
            union {
                int32_t I;
                float F;
            };
            String Name;
            SpecifierType Type;
        };

        ShaderVariation() = default;
        ShaderVariation(const Vector<Specifier>& specifiers);

        bool Matches(const ShaderVariation& other, bool exact = true) const;
        ShaderDefines GetDefines() const;

    private:
        // TODO: Replace with StringId
        UnorderedMap<String, Specifier> m_Parameters;
    };
} // namespace Crowny