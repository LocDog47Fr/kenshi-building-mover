# BuildingMover (Kenshi)

*[English version](README.md)*

Un plugin [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) pour Kenshi. Quand on déplace
ou tourne un bâtiment avec l'éditeur du jeu (**Maj+F12**), tout ce qui lui appartient suit :

- les meubles et stockages, avec leur contenu ;
- les éléments de l'agencement extérieur, comme les enseignes ;
- les éclairages muraux fixés à l'extérieur du bâtiment ;
- les objets posés au sol à l'intérieur du bâtiment.

Une petite fenêtre **BuildingMover** apparaît dans l'éditeur. Son bouton fige l'intérieur pour
déplacer le bâtiment seul, comme dans le jeu d'origine. Le texte du bouton est en français ou en
anglais, selon la langue du jeu.

## Prérequis

- Kenshi **1.0.65** (Steam). D'autres versions peuvent fonctionner si RE_Kenshi les gère, mais elles
  n'ont pas été testées.
- **RE_Kenshi**. Ce mod est un plugin natif (DLL) que seul RE_Kenshi sait charger.

## Installation

1. Télécharger `BuildingMover.zip` sur la page [Releases](../../releases).
2. Extraire le dossier `BuildingMover` dans `Kenshi\mods\`.
3. Activer **BuildingMover** dans la liste des mods du lanceur de Kenshi.
4. En jeu, ouvrir l'éditeur avec Maj+F12 et déplacer un bâtiment.

Les changements sont enregistrés avec la partie, et les meubles restent en place après un rechargement.

## Limites connues

- Les objets posés par terre **à l'extérieur** d'un bâtiment ne suivent pas : le jeu ne les
  rattache à aucun bâtiment.
- Les éclairages muraux sont repérés par leur position (petits objets avec une lumière, contre la
  boîte englobante du bâtiment). Une lampe d'un bâtiment voisin très proche peut parfois partir
  aussi ; dans ce cas, figer l'intérieur.
- Les objets au sol sont replacés quand on relâche la souris (environ 0,3 s après le dernier
  mouvement), pas pendant le glissement.

## Compiler depuis les sources

Le plugin est écrit en C++ avec [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) et
doit être compilé avec l'outil **Visual C++ 2010 x64 (v100)**, le même que Kenshi.

1. Installer **Visual Studio 2022** (Développement Desktop en C++) et **Git for Windows**.
2. Installer le compilateur v100 depuis l'ISO du **Windows SDK 7.1** (`GRMSDKX_EN_DVD.iso`).
   L'installeur du SDK échoue sous Windows 10/11 : installer directement les paquets du
   compilateur depuis l'ISO, `Setup\vc_stdx86\vc_stdx86.msi` et `Setup\vc_stdamd64\vc_stdamd64.msi`,
   puis la **Visual C++ 2010 SP1 Compiler Update for the Windows SDK 7.1** (KB2519277).
3. Copier le contenu de l'ISO dans `deps\iso-sdk71\` et lancer `outils\extraire-sdk71.bat`.
   Il extrait les en-têtes et bibliothèques du SDK 7.1 dans `deps\winsdk71\`.
4. Lancer `installer-dependances.bat`. Il télécharge KenshiLib et Boost 1.60 dans `deps\`.
5. Ouvrir `BuildingMover.vcxproj` dans Visual Studio. S'il propose de recibler le projet vers un
   outil plus récent, **refuser**. Choisir **Release | x64**.
6. Utiliser **Générer > Régénérer la solution** (Ctrl+Alt+F7). Une simple génération peut dire
   que le projet est à jour, car le suivi des fichiers est désactivé pour l'outil v100.
7. Copier `x64\Release\BuildingMover.dll` dans `mod\BuildingMover\`, puis copier ce dossier dans
   `Kenshi\mods\`.

`src/BuildingMover.cpp` explique le fonctionnement : il s'accroche à `TransformWindow::updateGizmo`
(le gizmo de Maj+F12) et à `LevelEditor::levelEditMode`, puis applique la translation et la
rotation du bâtiment à tout ce qui lui est rattaché. Certains décalages mémoire sont propres à
Kenshi 1.0.65.

## Licence

GPLv3, voir [LICENSE](LICENSE). KenshiLib est sous GPLv3, ce plugin l'est donc aussi.
