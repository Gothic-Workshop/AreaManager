# AreaManager

AreaManager is a Union plugin implementation of the `area.nut` and
`areamanager.nut` behavior. Daedalus code creates polygon, portal-room, or mixed
areas through integer handles and can register enter/exit callbacks for the
hero.

The manager checks the hero every 500 ms, matching the Squirrel implementation.
Polygons can use X/Z coordinates with flat height bounds, or X/Y/Z vertices
whose Y values form a variable ceiling. Global height bounds can also restrict
the polygon component of a mixed area; they do not restrict its portal rooms.
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

	AM_SetDebug(TRUE);
	AM_Add(MonasteryArea, Monastery_OnEnter, Monastery_OnExit);
};
```

Call the setup function once per process. `AM_Clear` is available when scripts
need to recreate every area after a development-time script reload.

A portal-room component is defined by adding one or more room names. It may be
used alone or combined with polygon points:

```d
func void Monastery_CreateMixedArea()
{
	MonasteryArea = AM_Create("NEWWORLD\\NEWWORLD.ZEN");

	AM_AddPortalRoom(MonasteryArea, "MONASTERY_LIBRARY");
	AM_AddPortalRoom(MonasteryArea, "MONASTERY_CHAPEL");
	AM_AddPoint(MonasteryArea, 45078.16, 20583.71);
	AM_AddPoint(MonasteryArea, 46043.58, 18982.20);
	AM_AddPoint(MonasteryArea, 46671.66, 19360.81);

	AM_Add(MonasteryArea, Monastery_OnEnter, Monastery_OnExit);
};
```

Use the BSP sector names defined by the world portals. In a mixed area, portal
rooms and the polygon are combined as a union: either component can make the
hero enter the area. Polygon height limits do not apply when the hero matches
one of the assigned portal rooms.

For a polygon with a variable ceiling, use X/Y/Z points consistently:

```d
AM_AddPoint3D(area, 31411.8, 3465.7, -18821.0);
AM_AddPoint3D(area, 31135.8, 3263.01, -8778.73);
AM_AddPoint3D(area, 23351.7, 3002.57, -8166.29);
AM_AddPoint3D(area, 21004.5, 3002.57, -10267.5);
AM_AddPoint3D(area, 20334.7, 5556.29, -9904.75);
AM_AddPoint3D(area, 17803.0, 5761.09, -11013.9);
AM_AddPoint3D(area, 21325.7, 5114.56, -20234.9);
AM_AddPoint3D(area, 24000.6, 3061.89, -19984.2);
AM_AddPoint3D(area, 29421.7, 3190.53, -19631.1);
```

The polygon is triangulated in X/Z and its ceiling height is interpolated over
each triangle. The hero must be below that local ceiling. `AM_SetMinHeight` can
provide a flat floor, while `AM_SetMaxHeight` remains an optional global cap.
Vertices must trace one simple boundary, clockwise or counterclockwise; holes
and self-intersecting outlines are not supported. Do not mix `AM_AddPoint` and
`AM_AddPoint3D` within one polygon.

## Externals

- `AM_Create(world)` creates an area and returns a nonzero handle.
- `AM_AddPoint(area, x, z)` adds a polygon vertex.
- `AM_AddPoint3D(area, x, y, z)` adds a polygon vertex whose Y coordinate is
  the local ceiling height.
- `AM_AddPortalRoom(area, portalRoom)` adds a BSP portal-room name. Repeat it
  to make the area the union of multiple rooms; polygon points may be added to
  the same area.
- `AM_SetHeight(area, minY, maxY)` enables the polygon's optional global height
  interval.
- `AM_SetMinHeight(area, minY)` enables only the polygon's global lower bound.
- `AM_SetMaxHeight(area, maxY)` enables only the polygon's global upper bound.
  For an X/Y/Z polygon, this is an additional cap on the interpolated ceiling.
- `AM_ClearHeight(area)` removes the global height bounds. Per-point ceilings
  remain active.
- `AM_IsIn(area, x, y, z, world)` performs the same point-in-polygon test as
  `Area.isIn`, including a variable ceiling when present. It tests only the
  polygon component because an arbitrary coordinate has no vob whose BSP
  sector can be queried.
- `AM_IsHeroIn(area)` tests the hero immediately.
- `AM_Add(area, onEnter, onExit)` registers a completed area with the
  manager. It returns false when the handle is already registered, the area has
  neither a valid polygon nor a portal room, or a callback is invalid.
- `AM_Remove(area)` stops processing an area without destroying it.
- `AM_Destroy(area)` removes and destroys an area handle.
- `AM_IsRegistered(area)` reports whether the manager processes the area.
- `AM_SetDebug(enabled)` globally enables or disables continuously rendered red
  borders for all completed polygon areas in the current world. Each closing
  edge from the last point to the first is included. Portal-only areas are
  ignored, while mixed areas render their polygon component. X/Y/Z borders use
  their supplied ceiling heights; X/Z borders follow the hero's current Y with
  a small visibility offset clamped to the area's height bounds. Areas do not
  need to be registered.
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
