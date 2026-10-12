// BuildingMover - plugin RE_Kenshi / KenshiLib
// Quand l'outil de deplacement (Maj+F12) bouge un batiment, tout ce qui lui
// est rattache suit avec la meme translation et rotation :
//  - meubles et stockages de l'interieur (avec leur contenu),
//  - elements de l'agencement exterieur (enseignes, PNJ),
//  - eclairages muraux exterieurs (reperes par leur lumiere et leur position),
//  - objets poses au sol a l'interieur du batiment.
// Une fenetre dans l'editeur permet de figer tout cela pour retrouver le
// comportement d'origine (le batiment bouge seul).
// Le texte du bouton est en francais ou en anglais selon la langue du jeu.

#include <Debug.h>
#include <core/Functions.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Enums.h>
#include <kenshi/Item.h>
#include <kenshi/gui/LevelEditor.h>
#include <kenshi/gui/TransformWindow.h>
#include <kenshi/Building/Building.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>
#include <kenshi/util/YesNoMaybe.h>
#include <kenshi/PhysicsCollection.h>

#include <ogre/OgreSceneNode.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Delegate.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <sstream>
#include <vector>

namespace
{
    // Rayon de recherche autour du batiment (unites du jeu).
    const float SEARCH_RADIUS = 3000.0f;
    // Un deplacement est considere termine apres ce delai sans mouvement :
    // c'est a ce moment que les objets au sol sont replaces.
    const DWORD DRAG_END_MS = 300;
    // Eclairages muraux : marge autour de la boite englobante du batiment et
    // taille maximale (demi-cote) d'un eclairage.
    const float LIGHT_MARGIN = 100.0f;
    const float LIGHT_MAX_HALF_SIZE = 300.0f;

    // Dans BuildingInterior, l'agencement interieur (Layout) est a +0x30 et
    // il pointe vers l'agencement exterieur a +0xA8 (Kenshi 1.0.65).
    const size_t INTERIOR_LAYOUT_OFFSET = 0x30;
    const size_t EXTERIOR_LAYOUT_OFFSET = 0xA8;

    struct ItemStart
    {
        Item* item;
        Ogre::Vector3 pos;
        Ogre::Quaternion rot;
    };

    // Un deplacement en cours (un glisser du gizmo).
    struct Drag
    {
        bool active;
        Building* building;
        Ogre::Vector3 startPos;
        Ogre::Quaternion startRot;
        DWORD lastMove;
        std::vector<Building*> furniture;
        std::vector<ItemStart> items;
    };

    bool g_interiorFrozen = false;
    Drag g_drag;
    lektor<RootObject*> g_queryResults;
    MyGUI::Window* g_window = NULL;

    Building* GetEditedBuilding()
    {
        if (!ou || !ou->player)
            return NULL;
        LevelEditor* editor = ou->player->levelEditor;
        if (!editor || !editor->levelEditModeOn)
            return NULL;
        return editor->selectedObject.getBuilding();
    }

    Layout* GetInteriorLayout(Building* b)
    {
        if (!b->myInterior)
            return NULL;
        return reinterpret_cast<Layout*>(reinterpret_cast<char*>(b->myInterior) + INTERIOR_LAYOUT_OFFSET);
    }

    Layout* GetExteriorLayout(Building* b)
    {
        Layout* interior = GetInteriorLayout(b);
        if (!interior)
            return NULL;
        return *reinterpret_cast<Layout**>(reinterpret_cast<char*>(interior) + EXTERIOR_LAYOUT_OFFSET);
    }

    // Point dans la boite englobante (monde) du batiment, agrandie de MARGIN.
    bool InsideBounds(Building* b, const Ogre::Vector3& p)
    {
        const Ogre::Vector3& c = b->AABB.mCenter;
        const Ogre::Vector3& h = b->AABB.mHalfSize;
        return fabs(p.x - c.x) <= h.x + LIGHT_MARGIN
            && fabs(p.y - c.y) <= h.y + LIGHT_MARGIN
            && fabs(p.z - c.z) <= h.z + LIGHT_MARGIN;
    }

    bool IsDoorOf(Building* b, Building* f)
    {
        for (uint32_t i = 0; i < b->doors.size(); ++i)
            if (b->doors.stuff[i] == f)
                return true;
        return false;
    }

    // Eclairage pose contre le batiment (applique murale...) : le jeu ne le
    // rattache a rien, on le reconnait a sa lumiere et a sa position dans la
    // boite englobante du batiment.
    bool IsWallLight(Building* b, Building* f)
    {
        if (f->isFurnitureOf || f->isFoliage || f->furnitureParentBuilding())
            return false;
        if (!f->physical || f->physical->lights.size() == 0)
            return false;
        if (f->AABB.mHalfSize.x > LIGHT_MAX_HALF_SIZE || f->AABB.mHalfSize.y > LIGHT_MAX_HALF_SIZE
            || f->AABB.mHalfSize.z > LIGHT_MAX_HALF_SIZE)
            return false;
        return InsideBounds(b, f->pos) && !IsDoorOf(b, f);
    }

    // Meubles (interieur), elements de l'agencement exterieur et eclairages
    // muraux du batiment.
    void FindFurniture(Building* b, std::vector<Building*>& out)
    {
        Layout* interior = GetInteriorLayout(b);
        Layout* exterior = GetExteriorLayout(b);
        const bool boundsValid = b->AABB.mHalfSize.x > 0.0f && b->AABB.mHalfSize.z > 0.0f;

        g_queryResults.count = 0;
        ou->getObjectsWithinSphere(g_queryResults, b->pos, SEARCH_RADIUS, BUILDING, 4096, b);

        size_t numInterior = 0, numExterior = 0, numLights = 0;
        for (uint32_t i = 0; i < g_queryResults.size(); ++i)
        {
            Building* f = static_cast<Building*>(g_queryResults.stuff[i]);
            if (!f || f == b)
                continue;
            if (exterior && f->isFurnitureOf == exterior)
                ++numExterior;
            else if (f->furnitureParentBuilding() == b || (interior && f->isFurnitureOf == interior))
                ++numInterior;
            else if (boundsValid && IsWallLight(b, f))
                ++numLights;
            else
                continue;
            out.push_back(f);
        }

        std::ostringstream s;
        s << "BuildingMover : " << numInterior << " meuble(s) interieur(s), " << numExterior
          << " element(s) exterieur(s), " << numLights << " eclairage(s) mural(aux) sur "
          << g_queryResults.size() << " batiment(s) proches ; boite du batiment centre("
          << b->AABB.mCenter.x << ", " << b->AABB.mCenter.y << ", " << b->AABB.mCenter.z
          << ") demi-taille(" << b->AABB.mHalfSize.x << ", " << b->AABB.mHalfSize.y << ", "
          << b->AABB.mHalfSize.z << ")";
        DebugLog(s.str());
    }

    // Objets poses au sol a l'interieur du batiment.
    void FindItems(Building* b, std::vector<ItemStart>& out)
    {
        g_queryResults.count = 0;
        ou->getObjectsWithinSphere(g_queryResults, b->pos, SEARCH_RADIUS, ITEM, 4096, NULL);

        size_t numItems = 0;
        for (uint32_t i = 0; i < g_queryResults.size(); ++i)
        {
            RootObject* o = g_queryResults.stuff[i];
            if (!o)
                continue;
            Item* item = o->handle.getItem();
            if (!item || static_cast<RootObject*>(item) != o)
                continue;
            ++numItems;
            if (item->isInInventory || !item->physicalShouldExist)
                continue;
            if (item->isInsideBuilding.getBuilding() != b)
                continue;
            ItemStart start = { item, item->pos, item->rot };
            out.push_back(start);
        }

        std::ostringstream s;
        s << "BuildingMover : " << out.size() << " objet(s) au sol rattache(s) sur "
          << numItems << " objet(s) proches";
        DebugLog(s.str());
    }

    // Fin du glisser : replace les objets au sol avec le deplacement total.
    void FinishDrag()
    {
        if (!g_drag.active)
            return;
        g_drag.active = false;

        Building* b = g_drag.building;
        const Ogre::Quaternion delta = b->rot * g_drag.startRot.Inverse();
        for (size_t i = 0; i < g_drag.items.size(); ++i)
        {
            const ItemStart& e = g_drag.items[i];
            Item* item = e.item;
            if (item->isInInventory)
                continue;
            const Ogre::Vector3 newPos = b->pos + delta * (e.pos - g_drag.startPos);
            const Ogre::Quaternion newRot = delta * e.rot;
            const bool fixed = item->fixedPhysicalPosition;
            const bool dynamic = item->useDynamicPhysics;
            YesNoMaybe bareWeapon(item->existAsBareWeapon ? YesNoMaybe::YES : YesNoMaybe::NO);
            // Recree l'objet physique a la nouvelle position.
            item->_NV_deactivate();
            item->_NV_activate(true, newPos, newRot, fixed, bareWeapon, dynamic);
        }
        g_drag.furniture.clear();
        g_drag.items.clear();
    }

    void StartDrag(Building* b, const Ogre::Vector3& posBefore, const Ogre::Quaternion& rotBefore)
    {
        g_drag.active = true;
        g_drag.building = b;
        g_drag.startPos = posBefore;
        g_drag.startRot = rotBefore;
        g_drag.furniture.clear();
        g_drag.items.clear();
        FindFurniture(b, g_drag.furniture);
        FindItems(b, g_drag.items);
    }

    // Applique aux meubles le meme deplacement que celui que le gizmo vient
    // d'appliquer au batiment (rotation autour de l'ancienne position du
    // batiment, puis translation).
    void MoveFurniture(Building* b, const Ogre::Vector3& posBefore, const Ogre::Quaternion& rotBefore)
    {
        const Ogre::Quaternion delta = b->rot * rotBefore.Inverse();
        const bool rotated = !(delta == Ogre::Quaternion::IDENTITY);

        for (size_t i = 0; i < g_drag.furniture.size(); ++i)
        {
            Building* f = g_drag.furniture[i];
            const Ogre::Vector3 newPos = b->pos + delta * (f->pos - posBefore);
            const Ogre::Quaternion newRot = delta * f->rot;

            if (f->rootNode)
            {
                f->rootNode->translate(newPos - f->pos, Ogre::Node::TS_WORLD);
                if (rotated)
                    f->rootNode->rotate(delta, Ogre::Node::TS_WORLD);
            }
            f->pos = newPos;
            f->rot = newRot;
            f->notifyChange();
        }
    }

    void OnGizmoUpdate(Building* b, const Ogre::Vector3& posBefore, const Ogre::Quaternion& rotBefore)
    {
        const DWORD now = GetTickCount();
        if (g_drag.active && (b != g_drag.building || now - g_drag.lastMove > DRAG_END_MS))
            FinishDrag();

        if (!b || (b->pos == posBefore && b->rot == rotBefore) || g_interiorFrozen)
            return;

        if (!g_drag.active)
            StartDrag(b, posBefore, rotBefore);
        g_drag.lastMove = now;
        MoveFurniture(b, posBefore, rotBefore);
    }

    // Langue de l'interface du mod : francais si le jeu est en francais,
    // anglais sinon.
    bool g_french = false;

    std::string ToLower(std::string s)
    {
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = (char)tolower((unsigned char)s[i]);
        return s;
    }

    // Valeur de la cle "language" de settings.cfg (dossier du jeu), ou "".
    std::string ReadLanguageSetting()
    {
        FILE* file = fopen("settings.cfg", "r");
        if (!file)
            return "";
        std::string value;
        char line[512];
        while (fgets(line, sizeof(line), file))
        {
            std::string s = line;
            size_t start = s.find_first_not_of(" \t");
            if (start == std::string::npos || ToLower(s.substr(start, 8)) != "language")
                continue;
            size_t sep = s.find_first_of("= \t", start + 8);
            if (sep != start + 8)
                continue;
            size_t begin = s.find_first_not_of("= \t", sep);
            size_t end = s.find_last_not_of(" \t\r\n");
            if (begin != std::string::npos && end != std::string::npos && end >= begin)
                value = s.substr(begin, end - begin + 1);
            break;
        }
        fclose(file);
        return value;
    }

    // Langue choisie dans Steam pour le jeu (ISteamApps::GetCurrentGameLanguage), ou "".
    std::string ReadSteamLanguage()
    {
        HMODULE steam = GetModuleHandleA("steam_api64.dll");
        if (!steam)
            return "";
        typedef void* (*SteamAppsFn)();
        SteamAppsFn steamApps = (SteamAppsFn)GetProcAddress(steam, "SteamApps");
        void* apps = steamApps ? steamApps() : NULL;
        if (!apps)
            return "";
        typedef const char* (*GetLanguageFn)(void*);
        GetLanguageFn getLanguage = (*reinterpret_cast<GetLanguageFn**>(apps))[4];
        const char* language = getLanguage ? getLanguage(apps) : NULL;
        return language ? language : "";
    }

    bool IsFrench(const std::string& language)
    {
        const std::string l = ToLower(language);
        return l.compare(0, 2, "fr") == 0;
    }

    void DetectLanguage()
    {
        std::string source = "settings.cfg";
        std::string language = ReadLanguageSetting();
        if (language.empty() || ToLower(language).find("steam") != std::string::npos)
        {
            source = "Steam";
            language = ReadSteamLanguage();
        }
        if (language.empty())
        {
            source = "Windows";
            language = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_FRENCH ? "french" : "english";
        }
        g_french = IsFrench(language);
        DebugLog("BuildingMover : langue du jeu '" + language + "' (" + source + ") -> "
                 + (g_french ? "francais" : "anglais"));
    }

    std::string ButtonCaption()
    {
        if (!g_french)
            return g_interiorFrozen
                ? "Interior: frozen (click to make it follow)"
                : "Interior: follows the building (click to freeze)";
        // UTF-8 : "Interieur : fige" / "Interieur : suit le batiment" avec accents
        return g_interiorFrozen
            ? "Int\xC3\xA9rieur : fig\xC3\xA9 (cliquer pour le faire suivre)"
            : "Int\xC3\xA9rieur : suit le b\xC3\xA2timent (cliquer pour figer)";
    }

    void OnToggle(MyGUI::WidgetPtr sender)
    {
        FinishDrag();
        g_interiorFrozen = !g_interiorFrozen;
        static_cast<MyGUI::Button*>(sender)->setCaption(ButtonCaption());
        DebugLog(g_interiorFrozen ? "BuildingMover : interieur fige"
                                  : "BuildingMover : interieur suit le batiment");
    }

    void CreateToggleWindow()
    {
        if (g_window)
            return;
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (!gui)
            return;

        DetectLanguage();
        g_window = gui->createWidgetReal<MyGUI::Window>("Kenshi_WindowCX", 0.36f, 0.11f, 0.28f, 0.08f,
            MyGUI::Align::Default, "Window", "BuildingMoverWindow");
        g_window->setCaption("BuildingMover");
        MyGUI::Button* button = g_window->getClientWidget()->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.03f, 0.08f, 0.94f, 0.84f, MyGUI::Align::Stretch, "BuildingMoverToggle");
        button->setCaption(ButtonCaption());
        button->eventMouseButtonClick += MyGUI::newDelegate(OnToggle);
        g_window->setVisible(false);
    }

    bool (*TransformWindow_updateGizmo_orig)(TransformWindow*) = NULL;
    bool TransformWindow_updateGizmo_hook(TransformWindow* self)
    {
        Building* b = GetEditedBuilding();
        if (!b)
        {
            FinishDrag();
            return TransformWindow_updateGizmo_orig(self);
        }

        const Ogre::Vector3 pos = b->pos;
        const Ogre::Quaternion rot = b->rot;
        bool result = TransformWindow_updateGizmo_orig(self);
        OnGizmoUpdate(b, pos, rot);
        return result;
    }

    // Maj+F12 : affiche la fenetre du mod a l'entree dans l'editeur, la cache a la sortie.
    void (*LevelEditor_levelEditMode_orig)(LevelEditor*, bool) = NULL;
    void LevelEditor_levelEditMode_hook(LevelEditor* self, bool on)
    {
        FinishDrag();
        LevelEditor_levelEditMode_orig(self, on);
        CreateToggleWindow();
        if (g_window)
            g_window->setVisible(on);
    }
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("BuildingMover : chargement (phase 4, FR/EN)");
    g_drag.active = false;
    g_drag.building = NULL;

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&TransformWindow::updateGizmo),
            TransformWindow_updateGizmo_hook, &TransformWindow_updateGizmo_orig))
        ErrorLog("BuildingMover : hook TransformWindow::updateGizmo impossible");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&LevelEditor::levelEditMode),
            LevelEditor_levelEditMode_hook, &LevelEditor_levelEditMode_orig))
        ErrorLog("BuildingMover : hook LevelEditor::levelEditMode impossible");
}
