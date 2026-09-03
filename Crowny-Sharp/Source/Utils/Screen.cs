namespace Crowny
{
    /// <summary>Information about the render target the game is rendering into.</summary>
    public static class Screen
    {
        /// <summary>The render target size in pixels. In the editor this is the game view; at runtime the window framebuffer.</summary>
        public static Vector2 size => ManagedRuntimeContext.ScreenGetSize();

        /// <summary>The render target width in pixels.</summary>
        public static int width { get { return (int)size.x; } }

        /// <summary>The render target height in pixels.</summary>
        public static int height { get { return (int)size.y; } }
    }
}
