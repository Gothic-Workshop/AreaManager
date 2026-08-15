// API reference only. Do not include this file in Gothic.src; the plugin
// registers these externals before the game scripts are parsed.

func int AM_Create(var string world) {};
func int AM_AddPoint(var int area, var float x, var float z) {};
func int AM_AddPoint3D(var int area, var float x, var float y, var float z) {};
func int AM_AddPortalRoom(var int area, var string portalRoom) {};
func int AM_SetHeight(var int area, var float minY, var float maxY) {};
func int AM_SetMinHeight(var int area, var float minY) {};
func int AM_SetMaxHeight(var int area, var float maxY) {};
func int AM_ClearHeight(var int area) {};

func int AM_IsIn(
	var int area,
	var float x,
	var float y,
	var float z,
	var string world
) {};

func int AM_IsHeroIn(var int area) {};

func int AM_Add(var int area, var func onEnter, var func onExit) {};
func int AM_Remove(var int area) {};
func int AM_Destroy(var int area) {};
func int AM_IsRegistered(var int area) {};
func int AM_SetDebug(var int enabled) {};

func int AM_GetCurrentArea() {};
func int AM_SetProcessCallback(var func callback) {};
func void AM_ClearProcessCallback() {};
func void AM_Clear() {};
