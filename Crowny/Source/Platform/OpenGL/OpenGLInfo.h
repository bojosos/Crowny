#pragma once

namespace Crowny
{
    struct OpenGLDetail
    {
        String Name;
        String GLName;
        String Value;
    };

    class OpenGLInfo
    {

    public:
        static Vector<OpenGLDetail>& GetInformation();
        static void RetrieveInformation();

    private:
        static Vector<OpenGLDetail> s_Information;
    };
} // namespace Crowny