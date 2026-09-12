# Viewport asset drops

Explorer and the asset browser both call `ViewportPanel::SubmitDrop`. The panel converts the cursor into a world position and captures the scene and picked entity. It submits a path and that context to `ViewportAssetDrop`.

`ViewportAssetDrop` owns acceptance, import waiting, cancellation and asset actions. Its handler table in `Crowny-Editor/Source/Editor/ViewportAssetDrop.cpp` keeps each asset type's source classification, hover label and action together. Entity-producing handlers return the new entity; the module applies placement, registers creation with Undo and selects it. Material drops use the entity captured when the drop occurred.

`ViewportAssetDropProject.cpp` adapts the project library to the module. It copies external sources and sidecars into the project, requests import and resolves completed metadata. Existing project assets are used directly. The module stores paths and captured scene context while waiting, rather than retaining asset-browser entry pointers.

To add a supported asset type:

1. Add an action and a handler-table entry in `ViewportAssetDrop.cpp`.
2. For a new source format, extend `ClassifyViewportDropFile` and the relevant importer.
3. Add interface-level coverage in `ViewportAssetDropTests.cpp`. The in-memory library adapter can complete or fail imports without a window, disk imports or ImGui.

`Describe` returns a label or null for rejection. `Submit` returns whether a supported request was accepted; import or load failures can still prevent an action. `Update` processes pending requests and cancels them after scene changes or leaving Edit mode. Imports may run longer than a minute while the importer remains active. Scene-open actions clear requests captured for the previous scene.

Validation commands are `Scripts\crowny.bat build Editor` and `Scripts\crowny.bat test`. For manual Windows validation, drop an external mesh on an off-center viewport location, repeat from the asset browser with both imported and pending files, and check selection and Undo/Redo. Move the camera during import and switch scenes during another import to check captured placement and cancellation.
