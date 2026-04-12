using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public static class AssetDatabase
    {
        /// <summary>Loads an asset by project-relative path (e.g. "Assets/player.texture").</summary>
        public static T Load<T>(string path) where T : Asset
        {
            return Internal_Load(path) as T;
        }

        /// <summary>Loads an asset by UUID.</summary>
        public static T LoadFromUUID<T>(UUID uuid) where T : Asset
        {
            return Internal_LoadFromUUID(uuid) as T;
        }

        /// <summary>Returns the project-relative path for a loaded asset, or null if not found.</summary>
        public static string GetAssetPath(Asset asset)
        {
            if (asset == null)
                return null;
            return Internal_GetAssetPath(asset.uuid);
        }

        /// <summary>Returns true if the asset is non-null and backed by a valid, loaded asset handle.</summary>
        public static bool IsValidAsset(Asset asset)
        {
            if (asset == null)
                return false;
            return Internal_IsValid(asset.uuid);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset Internal_Load(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset Internal_LoadFromUUID(UUID uuid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetAssetPath(UUID uuid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsValid(UUID uuid);
    }
}
