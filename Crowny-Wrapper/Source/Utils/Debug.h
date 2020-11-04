
namespace Crowny
{
    class Debug
    {
    public:
        static void InitRuntimeFunctions();

    private:
        static void Internal_DebugLog(MonoString* message);
        static void Internal_Warning(MonoString* message);
        static void Internal_Error(MonoString* message);
    }
}