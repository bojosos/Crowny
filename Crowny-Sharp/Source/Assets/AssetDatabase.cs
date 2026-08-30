namespace Crowny
{
    public static class AssetDatabase
    {
        /// <summary>Loads an asset by project-relative path (e.g. "Assets/player.texture").</summary>
        public static T Load<T>(string path) where T : Asset
        {
            return ManagedRuntimeContext.CreateAsset<T>(ManagedRuntimeContext.AssetDatabaseLoad(path), true);
        }

        /// <summary>Loads an asset by UUID.</summary>
        public static T LoadFromUUID<T>(UUID uuid) where T : Asset
        {
            return ManagedRuntimeContext.CreateAsset<T>(ManagedRuntimeContext.AssetDatabaseLoadFromUuid(uuid), true);
        }

        /// <summary>Returns the project-relative path for a loaded asset, or null if not found.</summary>
        public static string GetAssetPath(Asset asset)
        {
            if (asset == null)
                return null;
            string path = ManagedRuntimeContext.AssetDatabaseGetPath(asset.uuid);
            return path.Length == 0 ? null : path;
        }

        /// <summary>Returns true if the asset is non-null and backed by a valid, loaded asset handle.</summary>
        public static bool IsValidAsset(Asset asset)
        {
            if (asset == null)
                return false;
            return ManagedRuntimeContext.AssetDatabaseIsValid(asset.uuid);
        }

    }
}
