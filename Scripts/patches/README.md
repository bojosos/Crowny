# GLFW active-window ownership fix

`glfw-active-window-ownership.patch` fixes the Windows event-loop crash reproduced while switching between Crowny editor instances. GLFW queried the active HWND's `GLFW` property and dereferenced it without establishing ownership. Attached input queues can expose a window belonging to another process. The fix resolves the HWND through this GLFW instance's live window list.

The patch is applied to the local `Crowny/Dependencies/glfw` submodule. `Scripts\crowny.bat setup` applies it after initializing submodules and recognizes an already patched checkout. If local edits conflict, setup reports the conflict without overwriting them. Until the GLFW fork includes the fix, an existing checkout can also apply it manually:

```powershell
git -C Crowny/Dependencies/glfw apply ../../../Scripts/patches/glfw-active-window-ownership.patch
Scripts\crowny.bat test --configuration Debug --filter "GLFW polling ignores*"
```

The isolated Windows regression test gives an unrelated native window a deliberately invalid `GLFW` property, makes it active, and polls GLFW. Ownership must be checked before that value can be used as a pointer.

Windows associates the active window with an input queue. See Microsoft's [discussion of input-attached threads](https://devblogs.microsoft.com/oldnewthing/20081006-00/?p=20643).
