using Crowny;

namespace Sandbox
{
    public class ConstructorReentryProbe : EntityBehaviour
    {
        public ConstructorReentryProbe()
        {
            Entity host = Entity.FindByName("Mono constructor reentry");
            if (host != null && !host.HasComponent<CameraFollow>())
                host.AddComponent<CameraFollow>();
        }
    }
}
