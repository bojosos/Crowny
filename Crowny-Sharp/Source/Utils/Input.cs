namespace Crowny
{
    /// <summary>
    /// Provides raw device state and capture-aware action queries.
    /// </summary>
    public static class Input
    {
        /// <summary>
        /// Checks whether a raw key is held. Raw input is not filtered by action-map or UI capture state.
        /// </summary>
        public static bool GetKey(KeyCode code) => ManagedRuntimeContext.InputGetKey((uint)code);

        /// <summary>
        /// Checks whether a raw key was pressed during the current frame.
        /// </summary>
        public static bool GetKeyDown(KeyCode code) => ManagedRuntimeContext.InputGetKeyDown((uint)code);

        /// <summary>
        /// Checks whether a raw key was released during the current frame.
        /// </summary>
        public static bool GetKeyUp(KeyCode code) => ManagedRuntimeContext.InputGetKeyUp((uint)code);

        /// <summary>
        /// Checks whether a raw mouse button is held. Raw input is not filtered by action-map or UI capture state.
        /// </summary>
        public static bool GetMouseButton(MouseCode code) => ManagedRuntimeContext.InputGetMouseButton((uint)code);

        /// <summary>
        /// Checks whether a raw mouse button was pressed during the current frame.
        /// </summary>
        public static bool GetMouseButtonDown(MouseCode code) => ManagedRuntimeContext.InputGetMouseButtonDown((uint)code);

        /// <summary>
        /// Checks whether a raw mouse button was released during the current frame.
        /// </summary>
        public static bool GetMouseButtonUp(MouseCode code) => ManagedRuntimeContext.InputGetMouseButtonUp((uint)code);

        /// <summary>
        /// The horizontal mouse-wheel movement accumulated during the current frame.
        /// </summary>
        public static float GetMouseScrollX() => ManagedRuntimeContext.InputGetMouseScrollX();

        /// <summary>
        /// The vertical mouse-wheel movement accumulated during the current frame.
        /// </summary>
        public static float GetMouseScrollY() => ManagedRuntimeContext.InputGetMouseScrollY();

        /// <summary>
        /// The current mouse position in game-screen pixel coordinates, with the origin at the bottom-left
        /// of the render target (<see cref="Screen.width"/>, <see cref="Screen.height"/>). While the game
        /// runs in the editor, positions are mapped into the game view.
        /// </summary>
        public static Vector2 mousePosition => ManagedRuntimeContext.InputGetMousePosition();

        /// <summary>
        /// The cursor movement accumulated during the current frame.
        /// </summary>
        public static Vector2 mouseDelta => ManagedRuntimeContext.InputGetMouseDelta();

        /// <summary>
        /// Returns whether a standardized gamepad is connected at the given player index.
        /// </summary>
        public static bool IsGamepadConnected(uint gamepad = 0) => ManagedRuntimeContext.InputIsGamepadConnected(gamepad);

        /// <summary>
        /// Returns whether a raw gamepad button is held.
        /// </summary>
        public static bool GetGamepadButton(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.InputGetGamepadButton(gamepad, (uint)code);

        /// <summary>
        /// Returns whether a raw gamepad button was pressed during the current frame.
        /// </summary>
        public static bool GetGamepadButtonDown(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.InputGetGamepadButtonDown(gamepad, (uint)code);

        /// <summary>
        /// Returns whether a raw gamepad button was released during the current frame.
        /// </summary>
        public static bool GetGamepadButtonUp(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.InputGetGamepadButtonUp(gamepad, (uint)code);

        /// <summary>
        /// Returns a standardized gamepad axis in the range -1 to 1.
        /// </summary>
        public static float GetGamepadAxis(GamepadAxisCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.InputGetGamepadAxis(gamepad, (uint)code);

        /// <summary>
        /// Returns whether a named button action is held. Action queries respect active maps, priorities, and UI capture.
        /// </summary>
        public static bool GetAction(string actionName) => ManagedRuntimeContext.InputGetAction(actionName);

        /// <summary>
        /// Returns whether a named button action was pressed during the current frame.
        /// </summary>
        public static bool GetActionDown(string actionName) => ManagedRuntimeContext.InputGetActionDown(actionName);

        /// <summary>
        /// Returns whether a named button action was released during the current frame.
        /// </summary>
        public static bool GetActionUp(string actionName) => ManagedRuntimeContext.InputGetActionUp(actionName);

        /// <summary>
        /// Returns the value of a named one-dimensional axis action.
        /// </summary>
        public static float GetAxis(string actionName) => ManagedRuntimeContext.InputGetAxis(actionName);

        /// <summary>
        /// Returns the value of a named two-dimensional axis action.
        /// </summary>
        public static Vector2 GetVector2(string actionName) => ManagedRuntimeContext.InputGetActionVector(actionName);

        /// <summary>
        /// Enables an action map by name and returns whether it was found.
        /// </summary>
        public static bool EnableActionMap(string mapName) => ManagedRuntimeContext.InputEnableActionMap(mapName);

        /// <summary>
        /// Disables an action map by name and returns whether it was found.
        /// </summary>
        public static bool DisableActionMap(string mapName) => ManagedRuntimeContext.InputDisableActionMap(mapName);

        /// <summary>
        /// Restores every runtime binding override to its authored project value.
        /// </summary>
        public static void ClearActionRebinds() => ManagedRuntimeContext.InputClearActionRebinds();
    }
}
