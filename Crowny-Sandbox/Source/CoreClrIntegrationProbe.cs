using Crowny;

namespace Sandbox
{
    [RequireComponent(typeof(Camera))]
    public sealed class CoreClrIntegrationProbe : EntityBehaviour
    {
        [SerializeField]
        private int value;

        [SerializeField]
        private bool advanced;

        [SerializeField, ShowIf(nameof(advanced)), EnableIf(nameof(CanEdit))]
        [OnValueChanged(nameof(ValueChanged))]
        private int conditionalValue;

        [SerializeField]
        private int callbackValue;

        public string entityUuidText = "";
        public string otherUuidText = "";
        public bool wrapperCacheStable;
        public bool transformRoundTrip;

        public CoreClrIntegrationProbe()
        {
            // Deliberately do not unsubscribe. The host must release every shared event on reload.
            SceneManager.sceneLoaded += OnSceneEvent;
            SceneManager.sceneUnloaded += OnSceneEvent;
            SceneManager.sceneReloaded += OnSceneEvent;
            SceneManager.activeSceneChanged += OnSceneEvent;
            SceneManager.executionStateChanged += OnExecutionStateChanged;
        }

        private void Start()
        {
            entityUuidText = entity.uuid.ToString();
            Entity owner = entity;
            Transform cached = transform;
            wrapperCacheStable = ReferenceEquals(owner, entity) && ReferenceEquals(cached, transform) &&
                                 ReferenceEquals(cached, owner.GetComponent<Transform>()) &&
                                 ReferenceEquals(GetComponent<Camera>(), GetComponent<Camera>());
            Vector3 previous = cached.position;
            cached.position = new Vector3(1.25f, 2.5f, 3.75f);
            Vector3 actual = transform.position;
            transformRoundTrip = actual.x == 1.25f && actual.y == 2.5f && actual.z == 3.75f;
            cached.position = previous;
        }

        private void Update()
        {
            throw new System.InvalidOperationException("quoted \"callback\"\\path\n\u03a9");
        }

        protected override void OnTriggerEnter3D(Entity other)
        {
            otherUuidText = other.uuid.ToString();
        }

        private void OnSceneEvent(UUID scene) { }
        private void OnExecutionStateChanged(SceneExecutionState state) { }

        [Button("Add Value", ButtonSizes.Medium, ButtonStyle.FoldoutButton, Expanded = true)]
        private int AddValue(int amount = 3)
        {
            value += amount;
            return value;
        }

        private bool CanEdit()
        {
            return advanced;
        }

        private void ValueChanged(int currentValue)
        {
            callbackValue = currentValue;
        }
    }
}
