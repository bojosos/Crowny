#pragma once

namespace Crowny
{
    class ShaderDefines
    {
    public:
        void Set(const String& name, int value);
        void Set(const String& name, float value);
        void Set(const String& name, const String& value);
        void Remove(const String& name);
        bool Has(const String& name) const;
        const UnorderedMap<String, String>& Get() const;
        String GetCanonicalKey() const;
        uint64_t GetHash() const;

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
            Specifier() : Type(SpecifierType::Int), I(0) {}
            Specifier(const String& name, int32_t value) : I(value), Name(name), Type(Int) {}
            Specifier(const String& name, bool value) : I(value ? 1 : 0), Name(name), Type(Bool) {}
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
        ShaderVariation(std::initializer_list<Specifier> specifiers);

        void Set(const String& name, int32_t value);
        void Set(const String& name, float value);
        void Set(const String& name, bool value);

        int32_t GetInt(const String& name) const;
        float GetFloat(const String& name) const;
        bool GetBool(const String& name) const;
        bool Has(const String& name) const;

        bool Matches(const ShaderVariation& other, bool exact = true) const;
        ShaderDefines GetDefines() const;
        String GetCanonicalKey() const;
        uint64_t GetHash() const;
        size_t GetParameterCount() const { return m_Parameters.size(); }
        bool IsEmpty() const { return m_Parameters.empty(); }

        static const ShaderVariation EMPTY;

    private:
        CW_SIMPLESERIALIZABLE(ShaderVariation);
        UnorderedMap<String, Specifier> m_Parameters;
    };
} // namespace Crowny
