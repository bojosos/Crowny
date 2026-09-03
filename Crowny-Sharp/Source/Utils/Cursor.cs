namespace Crowny
{
    /// <summary>Describes how the operating-system cursor behaves inside the application window.</summary>
    public enum CursorLockMode : byte
    {
        /// <summary>The cursor is free to move and visible.</summary>
        None = 0,

        /// <summary>The cursor is pinned to the center of the window and hidden, e.g. for first-person cameras.</summary>
        Locked = 1,
    }

    /// <summary>Describes the operating-system cursor shape shown inside the application window.</summary>
    public enum CursorType : byte
    {
        /// <summary>No cursor; the cursor is hidden.</summary>
        None = 0,

        /// <summary>The default arrow cursor.</summary>
        Pointer = 1,

        /// <summary>The text-selection I-beam cursor.</summary>
        IBeam = 2,

        /// <summary>The crosshair cursor.</summary>
        Crosshair = 3,

        /// <summary>The pointing hand cursor.</summary>
        Hand = 4,

        /// <summary>The horizontal resize cursor.</summary>
        HResize = 5,

        /// <summary>The vertical resize cursor.</summary>
        VResize = 6,

        /// <summary>The no-entry / stop cursor.</summary>
        StopSign = 7,
    }

    /// <summary>Controls the cursor lock state and shape of the main window.</summary>
    public static class Cursor
    {
        /// <summary>Whether and how the cursor is confined to the window.</summary>
        public static CursorLockMode lockState
        {
            get { return ManagedRuntimeContext.InputIsMouseGrabbed() ? CursorLockMode.Locked : CursorLockMode.None; }
            set { ManagedRuntimeContext.InputSetMouseGrabbed(value == CursorLockMode.Locked); }
        }

        /// <summary>The cursor shape shown inside the window.</summary>
        public static CursorType type
        {
            get { return (CursorType)ManagedRuntimeContext.InputGetCursorType(); }
            set { ManagedRuntimeContext.InputSetCursorType((uint)value); }
        }
    }
}
