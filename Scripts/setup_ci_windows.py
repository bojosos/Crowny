import os
import urllib.request
import zipfile
import shutil

def download_and_extract(url, dest_dir):
    print(f"Downloading {url}...")
    file_path, _ = urllib.request.urlretrieve(url)
    print(f"Extracting to {dest_dir}...")
    with zipfile.ZipFile(file_path, 'r') as zip_ref:
        zip_ref.extractall(dest_dir)
    os.remove(file_path)

def setup_openal():
    # We need OpenAL SDK for linking. The one from openal.org is an installer.
    # OpenAL-soft is a good alternative and easier to handle.
    # However, if the project expects the original OpenAL SDK layout, we should mimic it.
    openal_dir = "C:/OpenAL_SDK"
    if not os.path.exists(openal_dir):
        os.makedirs(openal_dir)
    
    # Using a known zip that contains the libs and headers
    # This is a bit of a hack, but common for CI.
    # Alternatively, use OpenAL Soft which is what most people use now.
    url = "https://github.com/kcat/openal-soft/releases/download/1.23.1/openal-soft-1.23.1-bin.zip"
    dest = "temp_openal"
    download_and_extract(url, dest)
    
    # Mimic the layout expected by premake if possible
    # Crowny/premake5.lua expects:
    # (os.getenv("OPENAL_SDK") or "C:/Program Files (x86)/OpenAL 1.1 SDK") .. "/libs/Win64"
    # and includes from IncludeDir["openal"] = "%{wks.location}/Crowny/Dependencies/openal-soft/include"
    
    # Actually, root premake5.lua says:
    # IncludeDir["openal"] = "%{wks.location}/Crowny/Dependencies/openal-soft/include"
    # So it uses the headers from the repo! That's good.
    # We only need the .lib for linking.
    
    lib_src = os.path.join(dest, "openal-soft-1.23.1-bin", "libs", "Win64", "OpenAL32.lib")
    lib_dest_dir = os.path.join(openal_dir, "libs", "Win64")
    os.makedirs(lib_dest_dir, exist_ok=True)
    shutil.copy(lib_src, os.path.join(lib_dest_dir, "OpenAL32.lib"))
    
    # Also need the DLL for running tests later (if any)
    dll_src = os.path.join(dest, "openal-soft-1.23.1-bin", "bin", "Win64", "soft_oal.dll")
    # The project expects OpenAL32.dll
    shutil.copy(dll_src, os.path.join(openal_dir, "libs", "Win64", "OpenAL32.dll"))

    print(f"OpenAL SDK setup complete in {openal_dir}")
    shutil.rmtree(dest)

if __name__ == "__main__":
    setup_openal()
