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
