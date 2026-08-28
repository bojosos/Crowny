using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>
    /// Provides access to Crowny's input system.
    /// </summary>
    public static class Input
    {
        /// <summary>
        /// Checks whether a raw key is held. Raw input is not filtered by action-map or UI capture state.
        /// </summary>
        /// <param name="code">The key code.</param>
        /// <returns>True if the key is pressed, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetKey(KeyCode code);
#else
        public static bool GetKey(KeyCode code) => ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetKey, (uint)code);
#endif

        /// <summary>
        /// Checks if a key wasn't pressed last frame and now is.
        /// </summary>
        /// <param name="code">The key code.</param>
        /// <returns>True if the key wasn't pressed last frame and now is, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetKeyDown(KeyCode code);
#else
        public static bool GetKeyDown(KeyCode code) => ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetKeyDown, (uint)code);
#endif

        /// <summary>
        /// Checks if a key was pressed last frame and now isn't.
        /// </summary>
        /// <param name="code">The key code.</param>
        /// <returns>True if the key was pressed last frame and now isn't, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetKeyUp(KeyCode code);
#else
        public static bool GetKeyUp(KeyCode code) => ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetKeyUp, (uint)code);
#endif

        /// <summary>
        /// Checks whether a raw mouse button is held. Raw input is not filtered by action-map or UI capture state.
        /// </summary>
        /// <param name="code">Mouse button code.</param>
        /// <returns>True if the mouse button is pressed, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetMouseButton(MouseCode code);
#else
        public static bool GetMouseButton(MouseCode code) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetMouseButton, (uint)code);
#endif

        /// <summary>
        /// Checks if a mouse button is pressed now and wasn't last frame.
        /// </summary>
        /// <param name="code">Mouse button code.</param>
        /// <returns>True if the mouse button was pressed during this frame, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetMouseButtonDown(MouseCode code);
#else
        public static bool GetMouseButtonDown(MouseCode code) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetMouseButtonDown, (uint)code);
#endif

        /// <summary>
        /// Checks if a mouse button was pressed last frame and now isn't.
        /// </summary>
        /// <param name="code">Mouse button code.</param>
        /// <returns>True if the mouse button was pressed last frame and now isn't, false otherwise.</returns>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetMouseButtonUp(MouseCode code);
#else
        public static bool GetMouseButtonUp(MouseCode code) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetMouseButtonUp, (uint)code);
#endif

        /// <summary>
        /// The horizontal delta of mouse wheel.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static float GetMouseScrollX();
#else
        public static float GetMouseScrollX() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.InputGetMouseScrollX);
#endif

        /// <summary>
        /// The vertical delta of the mouse wheel.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static float GetMouseScrollY();
#else
        public static float GetMouseScrollY() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.InputGetMouseScrollY);
#endif

        /// <summary>
        /// The current mouse position.
        /// </summary>
        public static Vector2 mousePosition
        {
            get
            {
#if CROWNY_MONO
                Internal_GetMousePosition(out Vector2 pos);
                return pos;
#else
                return ManagedRuntimeContext.GetBindingVector2(ManagedBindingId.InputGetMousePosition);
#endif
            }
        }

        /// <summary>
        /// The cursor movement accumulated during the current frame.
        /// </summary>
        public static Vector2 mouseDelta
        {
            get
            {
#if CROWNY_MONO
                Internal_GetMouseDelta(out Vector2 delta);
                return delta;
#else
                return ManagedRuntimeContext.GetBindingVector2(ManagedBindingId.InputGetMouseDelta);
#endif
            }
        }

        /// <summary>
        /// Returns whether a standardized gamepad is connected at the given player index.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool IsGamepadConnected(uint gamepad = 0);
#else
        public static bool IsGamepadConnected(uint gamepad = 0) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputIsGamepadConnected, gamepad);
#endif

        /// <summary>
        /// Returns whether a gamepad button is held.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetGamepadButton(GamepadButtonCode code, uint gamepad = 0);
#else
        public static bool GetGamepadButton(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetGamepadButton, gamepad, (uint)code);
#endif

        /// <summary>
        /// Returns whether a gamepad button was pressed during the current frame.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetGamepadButtonDown(GamepadButtonCode code, uint gamepad = 0);
#else
        public static bool GetGamepadButtonDown(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetGamepadButtonDown, gamepad, (uint)code);
#endif

        /// <summary>
        /// Returns whether a gamepad button was released during the current frame.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetGamepadButtonUp(GamepadButtonCode code, uint gamepad = 0);
#else
        public static bool GetGamepadButtonUp(GamepadButtonCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetGamepadButtonUp, gamepad, (uint)code);
#endif

        /// <summary>
        /// Returns a standardized gamepad axis in the range -1 to 1.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static float GetGamepadAxis(GamepadAxisCode code, uint gamepad = 0);
#else
        public static float GetGamepadAxis(GamepadAxisCode code, uint gamepad = 0) =>
            ManagedRuntimeContext.GetInputFloat(ManagedBindingId.InputGetGamepadAxis, gamepad, (uint)code);
#endif

        /// <summary>
        /// Returns whether a named button action is held. Action queries respect active maps, priorities, and UI capture.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetAction(string actionName);
#else
        public static bool GetAction(string actionName) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetAction, actionName);
#endif

        /// <summary>
        /// Returns whether a named button action was pressed during the current frame.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetActionDown(string actionName);
#else
        public static bool GetActionDown(string actionName) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetActionDown, actionName);
#endif

        /// <summary>
        /// Returns whether a named button action was released during the current frame.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool GetActionUp(string actionName);
#else
        public static bool GetActionUp(string actionName) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputGetActionUp, actionName);
#endif

        /// <summary>
        /// Returns the value of a named one-dimensional axis action.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static float GetAxis(string actionName);
#else
        public static float GetAxis(string actionName) =>
            ManagedRuntimeContext.GetInputFloat(ManagedBindingId.InputGetAxis, actionName);
#endif

        /// <summary>
        /// Returns the value of a named two-dimensional axis action.
        /// </summary>
        public static Vector2 GetVector2(string actionName)
        {
#if CROWNY_MONO
            Internal_GetActionVector(actionName, out Vector2 value);
            return value;
#else
            return ManagedRuntimeContext.GetInputVector2(ManagedBindingId.InputGetActionVector, actionName);
#endif
        }

        /// <summary>
        /// Enables an action map by name and returns whether it was found.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool EnableActionMap(string mapName);
#else
        public static bool EnableActionMap(string mapName) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputEnableActionMap, mapName);
#endif

        /// <summary>
        /// Disables an action map by name and returns whether it was found.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static bool DisableActionMap(string mapName);
#else
        public static bool DisableActionMap(string mapName) =>
            ManagedRuntimeContext.GetInputBoolean(ManagedBindingId.InputDisableActionMap, mapName);
#endif

        /// <summary>
        /// Restores every runtime binding override to its authored project value.
        /// </summary>
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] public extern static void ClearActionRebinds();
#else
        public static void ClearActionRebinds() => ManagedRuntimeContext.InvokeInput(ManagedBindingId.InputClearActionRebinds);
#endif

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_GetMousePosition(out Vector2 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_GetMouseDelta(out Vector2 delta);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static void Internal_GetActionVector(string actionName, out Vector2 value);
#endif
    }
}
