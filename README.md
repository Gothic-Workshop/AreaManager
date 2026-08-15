# AreaManager

AreaManager is a Union plugin implementation of the `area.nut` and
`areamanager.nut` behavior. Daedalus code creates polygon or portal-room areas
through integer handles and can register enter/exit callbacks for the hero.

The manager checks the hero every 500 ms, matching the Squirrel implementation.
Polygons use X/Z coordinates. Either area type may optionally be restricted to
a Y interval.
World names are compared case-insensitively and `/` is normalized to `\`.
Portal-room names are also compared case-insensitively. One area may contain
multiple portal rooms.

## Daedalus setup

Do not add `userapi/AreaManager.d` to `Gothic.src`. The plugin registers its
externals before `Gothic.dat` is parsed; the declaration file is an API reference
for editors and other tooling only. AreaManager must therefore be loaded while
the scripts are compiled.

Install either the generated `AreaManager.vdf` in `Gothic/Data`, or install
`AreaManager.dll` in `Gothic/System/Autorun` together with the Union API runtime.

## Example

```d
var int MonasteryArea;

func void Monastery_OnEnter()
{
	PrintScreen("Entered monastery", -1, -1, "FONT_OLD_10_WHITE.TGA", 2);
};

func void Monastery_OnExit()
{
	PrintScreen("Left monastery", -1, -1, "FONT_OLD_10_WHITE.TGA", 2);
};

func void Monastery_CreateArea()
{
	MonasteryArea = AM_Create("NEWWORLD\\NEWWORLD.ZEN");

	AM_AddPoint(MonasteryArea, 45078.16, 20583.71);
	AM_AddPoint(MonasteryArea, 46043.58, 18982.20);
	AM_AddPoint(MonasteryArea, 46671.66, 19360.81);
	AM_AddPoint(MonasteryArea, 47497.69, 17990.53);
	AM_AddPoint(MonasteryArea, 46869.62, 17611.92);
	AM_AddPoint(MonasteryArea, 47835.04, 16010.40);
	AM_AddPoint(MonasteryArea, 52353.40, 18734.15);
	AM_AddPoint(MonasteryArea, 49596.51, 23307.47);

	AM_Add(MonasteryArea, Monastery_OnEnter, Monastery_OnExit);
};
```

Call the setup function once per process. `AM_Clear` is available when scripts
need to recreate every area after a development-time script reload.

A portal-room area is defined by adding one or more room names instead of
polygon points:

```d
func void Monastery_CreateInteriorArea()
{
	MonasteryArea = AM_Create("NEWWORLD\\NEWWORLD.ZEN");

	AM_AddPortalRoom(MonasteryArea, "MONASTERY_LIBRARY");
	AM_AddPortalRoom(MonasteryArea, "MONASTERY_CHAPEL");

	AM_Add(MonasteryArea, Monastery_OnEnter, Monastery_OnExit);
};
```

Use the BSP sector names defined by the world portals. Polygon points and
portal rooms cannot be mixed in the same area; the first definition call
selects the area's mode.

## Externals

- `AM_Create(world)` creates an area and returns a nonzero handle.
- `AM_AddPoint(area, x, z)` adds a polygon vertex.
- `AM_AddPortalRoom(area, portalRoom)` adds a BSP portal-room name. Repeat it
  to make the area the union of multiple rooms.
- `AM_SetHeight(area, minY, maxY)` enables the optional height interval.
- `AM_SetMinHeight(area, minY)` enables only the lower height bound.
- `AM_SetMaxHeight(area, maxY)` enables only the upper height bound.
- `AM_ClearHeight(area)` restores infinite height.
- `AM_IsIn(area, x, y, z, world)` performs the same point-in-polygon test as
  `Area.isIn`. It returns false for portal-room areas because an arbitrary
  coordinate has no vob whose BSP sector can be queried.
- `AM_IsHeroIn(area)` tests the hero immediately.
- `AM_Add(area, onEnter, onExit)` registers a completed area with the
  manager. It returns false when the handle is already registered, the area has
  neither three polygon points nor a portal room, or a callback is invalid.
- `AM_Remove(area)` stops processing an area without destroying it.
- `AM_Destroy(area)` removes and destroys an area handle.
- `AM_IsRegistered(area)` reports whether the manager processes the area.
- `AM_GetCurrentArea()` returns the active area handle from inside an enter or
  exit callback and zero outside callbacks.
- `AM_SetProcessCallback(callback)` registers one parameterless Daedalus
  function that is called after area transitions on every 500 ms manager tick.
- `AM_ClearProcessCallback()` removes the periodic callback.
- `AM_Clear()` removes and destroys all areas.

All mutating/query operations except `AM_Clear` and `AM_GetCurrentArea` return
`TRUE` on success and `FALSE` on failure.

## Building

Configure and build the preset for the target executable, for example
`G2A-Release`. The project retains all G1, G1A, G2, G2A, and MP presets from the
Union plugin template.
