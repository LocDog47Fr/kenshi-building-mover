# BuildingMover (Kenshi)

*[Version française](README.fr.md)*

A [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) plugin for Kenshi. When you move or
rotate a building with the game's level editor (**Shift+F12**), everything that belongs to it
now moves with it:

- furniture and storage, with their contents;
- outside layout elements such as signs;
- wall lights mounted on the outside of the building;
- items lying on the floor inside the building.

A small **BuildingMover** window appears in the editor. Its button freezes the interior, so the
building can be moved on its own as in the vanilla game. The button text is in French or English,
matching the game's language.

## Requirements

- Kenshi **1.0.65** (Steam). Other versions may work if RE_Kenshi supports them, but they are untested.
- **RE_Kenshi**. This mod is a native plugin (DLL) and only RE_Kenshi can load it.

## Install

1. Download `BuildingMover.zip` from the [Releases](../../releases) page.
2. Extract the `BuildingMover` folder into `Kenshi\mods\`.
3. Enable **BuildingMover** in the Kenshi launcher's mod list.
4. In game, open the editor with Shift+F12 and move a building.

The changes are saved with your game, and furniture stays in place after a reload.

## Known limits

- Items lying on the ground **outside** a building do not follow: the game does not link them to
  any building.
- Wall lights are found by position (small objects with a light, touching the building's bounds).
  A light from a very close neighbouring building may occasionally be moved too; freeze the interior
  if that happens.
- Floor items are put in place when you release the mouse (about 0.3 s after the last movement),
  not during the drag.

## Building from source

The plugin is written in C++ with [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) and
must be compiled with the **Visual C++ 2010 x64 (v100)** toolset, the same one Kenshi uses.

1. Install **Visual Studio 2022** (Desktop development with C++) and **Git for Windows**.
2. Install the v100 compiler from the **Windows SDK 7.1** ISO (`GRMSDKX_EN_DVD.iso`). The SDK
   installer fails on Windows 10/11, so install the compiler packages directly from the ISO:
   `Setup\vc_stdx86\vc_stdx86.msi` and `Setup\vc_stdamd64\vc_stdamd64.msi`, then the
   **Visual C++ 2010 SP1 Compiler Update for the Windows SDK 7.1** (KB2519277).
3. Copy the content of the ISO to `deps\iso-sdk71\` and run `outils\extraire-sdk71.bat`.
   It extracts the SDK 7.1 headers and libraries into `deps\winsdk71\`.
4. Run `installer-dependances.bat`. It downloads KenshiLib and Boost 1.60 into `deps\`.
5. Open `BuildingMover.vcxproj` in Visual Studio. If it offers to retarget the project to a newer
   toolset, **refuse**. Select **Release | x64**.
6. Use **Build > Rebuild Solution** (Ctrl+Alt+F7). A plain Build may report the project as up to
   date because file tracking is disabled for the v100 toolset.
7. Copy `x64\Release\BuildingMover.dll` into `mod\BuildingMover\`, then copy that folder into
   `Kenshi\mods\`.

`src/BuildingMover.cpp` documents how it works: it hooks `TransformWindow::updateGizmo` (the
Shift+F12 gizmo) and `LevelEditor::levelEditMode`, then applies the building's translation and
rotation to everything attached to it. Some offsets are specific to Kenshi 1.0.65.

## License

GPLv3, see [LICENSE](LICENSE). KenshiLib is GPLv3, so this plugin is too.
