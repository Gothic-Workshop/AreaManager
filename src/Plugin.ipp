// This file is compiled separately for every selected Gothic engine.

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GOTHIC_NAMESPACE
{
	namespace AreaManagerPlugin
	{
		constexpr DWORD ProcessIntervalMs = 500;
		constexpr int InvalidAreaHandle = 0;
		constexpr int InvalidFunctionIndex = -1;

		std::string NormalizeWorldName(const char* value)
		{
			std::string normalized = value != nullptr ? value : "";

			std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character)
			{
				if (character == '/')
					return '\\';

				return static_cast<char>(std::toupper(character));
			});

			return normalized;
		}

		struct Point
		{
			float x;
			float z;
		};

		class Area
		{
		public:
			explicit Area(std::string world)
				: world_(std::move(world))
			{
			}

			void AddPoint(float x, float z)
			{
				points_.push_back({ x, z });

				if (!hasBounds_)
				{
					minX_ = maxX_ = x;
					minZ_ = maxZ_ = z;
					hasBounds_ = true;
					return;
				}

				if (x < minX_)
					minX_ = x;

				if (x > maxX_)
					maxX_ = x;

				if (z < minZ_)
					minZ_ = z;

				if (z > maxZ_)
					maxZ_ = z;
			}

			bool SetHeight(float minY, float maxY)
			{
				if (!std::isfinite(minY) || !std::isfinite(maxY) || minY > maxY)
					return false;

				minY_ = minY;
				maxY_ = maxY;
				hasMinHeight_ = true;
				hasMaxHeight_ = true;
				return true;
			}

			bool SetMinHeight(float minY)
			{
				if (!std::isfinite(minY) || (hasMaxHeight_ && minY > maxY_))
					return false;

				minY_ = minY;
				hasMinHeight_ = true;
				return true;
			}

			bool SetMaxHeight(float maxY)
			{
				if (!std::isfinite(maxY) || (hasMinHeight_ && maxY < minY_))
					return false;

				maxY_ = maxY;
				hasMaxHeight_ = true;
				return true;
			}

			void ClearHeight()
			{
				hasMinHeight_ = false;
				hasMaxHeight_ = false;
			}

			bool IsValid() const
			{
				return points_.size() >= 3 && !world_.empty();
			}

			bool IsIn(float x, float y, float z, const std::string& world) const
			{
				return world == world_ && ContainsPosition(x, y, z);
			}

			bool ContainsPosition(float x, float y, float z) const
			{
				if (!IsValid())
					return false;

				if (hasMinHeight_ && y < minY_)
					return false;

				if (hasMaxHeight_ && y > maxY_)
					return false;

				if (x < minX_ || x > maxX_ || z < minZ_ || z > maxZ_)
					return false;

				bool isIn = false;
				std::size_t previous = points_.size() - 1;

				for (std::size_t current = 0; current < points_.size(); previous = current++)
				{
					const Point& currentPoint = points_[current];
					const Point& previousPoint = points_[previous];

					if ((currentPoint.z > z) != (previousPoint.z > z)
						&& x < (previousPoint.x - currentPoint.x) * (z - currentPoint.z)
							/ (previousPoint.z - currentPoint.z) + currentPoint.x)
					{
						isIn = !isIn;
					}
				}

				return isIn;
			}

			const std::string& World() const
			{
				return world_;
			}

		private:
			std::vector<Point> points_;
			std::string world_;

			bool hasMinHeight_ = false;
			bool hasMaxHeight_ = false;
			float minY_ = 0.0F;
			float maxY_ = 0.0F;

			bool hasBounds_ = false;
			float minX_ = 0.0F;
			float maxX_ = 0.0F;
			float minZ_ = 0.0F;
			float maxZ_ = 0.0F;
		};

		struct Registration
		{
			int onEnter;
			int onExit;
			bool heroInside = false;
		};

		struct Transition
		{
			int areaHandle;
			int callback;
			bool entering;
		};

		class AreaManager
		{
		public:
			static AreaManager& Instance()
			{
				static AreaManager instance;
				return instance;
			}

			int Create(const zSTRING& world)
			{
				const std::string normalizedWorld = NormalizeWorldName(world.ToChar());
				if (normalizedWorld.empty())
					return InvalidAreaHandle;

				const int handle = nextHandle_++;
				areas_.emplace(handle, Area(normalizedWorld));
				return handle;
			}

			bool AddPoint(int handle, float x, float z)
			{
				auto* area = FindArea(handle);
				if (area == nullptr || !std::isfinite(x) || !std::isfinite(z))
					return false;

				area->AddPoint(x, z);
				return true;
			}

			bool SetHeight(int handle, float minY, float maxY)
			{
				auto* area = FindArea(handle);
				return area != nullptr && area->SetHeight(minY, maxY);
			}

			bool SetMinHeight(int handle, float minY)
			{
				auto* area = FindArea(handle);
				return area != nullptr && area->SetMinHeight(minY);
			}

			bool SetMaxHeight(int handle, float maxY)
			{
				auto* area = FindArea(handle);
				return area != nullptr && area->SetMaxHeight(maxY);
			}

			bool ClearHeight(int handle)
			{
				auto* area = FindArea(handle);
				if (area == nullptr)
					return false;

				area->ClearHeight();
				return true;
			}

			bool IsIn(int handle, float x, float y, float z, const zSTRING& world) const
			{
				const auto* area = FindArea(handle);
				return area != nullptr
					&& area->IsIn(x, y, z, NormalizeWorldName(world.ToChar()));
			}

			bool IsHeroIn(int handle) const
			{
				const auto* area = FindArea(handle);
				if (area == nullptr || player == nullptr || ogame == nullptr || ogame->GetWorld() == nullptr)
					return false;

				if (!IsCurrentWorld(*area))
					return false;

				const zVEC3 position = player->GetPositionWorld();
				return area->ContainsPosition(position[0], position[1], position[2]);
			}

			bool Add(int handle, int onEnter, int onExit)
			{
				const auto* area = FindArea(handle);
				if (area == nullptr || !area->IsValid() || registrations_.contains(handle))
					return false;

				if (!IsFunction(onEnter) || !IsFunction(onExit))
					return false;

				registrations_.emplace(handle, Registration{ onEnter, onExit });
				return true;
			}

			bool Remove(int handle)
			{
				return registrations_.erase(handle) != 0;
			}

			bool Destroy(int handle)
			{
				registrations_.erase(handle);
				return areas_.erase(handle) != 0;
			}

			bool IsRegistered(int handle) const
			{
				return registrations_.contains(handle);
			}

			int CurrentCallbackArea() const
			{
				return currentCallbackArea_;
			}

			bool SetProcessCallback(int callback)
			{
				if (!IsFunction(callback))
					return false;

				processCallback_ = callback;
				return true;
			}

			void ClearProcessCallback()
			{
				processCallback_ = InvalidFunctionIndex;
			}

			void Clear()
			{
				registrations_.clear();
				areas_.clear();
			}

			void Tick()
			{
				const DWORD now = GetTickCount();

				if (!timerStarted_)
				{
					timerStarted_ = true;
					lastProcessTime_ = now;
					return;
				}

				if (now - lastProcessTime_ < ProcessIntervalMs)
					return;

				lastProcessTime_ = now;
				Process();
			}

		private:
			AreaManager() = default;

			Area* FindArea(int handle)
			{
				const auto iterator = areas_.find(handle);
				return iterator != areas_.end() ? &iterator->second : nullptr;
			}

			const Area* FindArea(int handle) const
			{
				const auto iterator = areas_.find(handle);
				return iterator != areas_.end() ? &iterator->second : nullptr;
			}

			bool IsFunction(int index) const
			{
				if (parser == nullptr || index < 0)
					return false;

				const zCPar_Symbol* symbol = parser->GetSymbol(index);
				return symbol != nullptr && symbol->type == zPAR_TYPE_FUNC;
			}

			bool IsCurrentWorld(const Area& area) const
			{
				zSTRING world(area.World().c_str());
				if (ogame->IsInWorld(world) != 0)
					return true;

				const std::size_t separator = area.World().find_last_of('\\');
				if (separator == std::string::npos)
					return false;

				zSTRING filename(area.World().substr(separator + 1).c_str());
				return ogame->IsInWorld(filename) != 0;
			}

			void Process()
			{
				if (player == nullptr || ogame == nullptr || ogame->GetWorld() == nullptr)
					return;

				const zVEC3 position = player->GetPositionWorld();
				std::vector<Transition> transitions;
				transitions.reserve(registrations_.size());

				for (auto& [handle, registration] : registrations_)
				{
					const auto* area = FindArea(handle);
					if (area == nullptr || !IsCurrentWorld(*area))
						continue;

					const bool isIn = area->ContainsPosition(position[0], position[1], position[2]);
					if (isIn == registration.heroInside)
						continue;

					registration.heroInside = isIn;
					transitions.push_back({
						handle,
						isIn ? registration.onEnter : registration.onExit,
						isIn
					});
				}

				for (const auto& transition : transitions)
					Dispatch(transition);

				DispatchProcessCallback();
			}

			void Dispatch(const Transition& transition)
			{
				const auto registration = registrations_.find(transition.areaHandle);
				if (registration == registrations_.end())
					return;

				if (registration->second.heroInside != transition.entering)
					return;

				const int currentCallback = transition.entering
					? registration->second.onEnter
					: registration->second.onExit;

				if (currentCallback != transition.callback || !IsFunction(currentCallback))
					return;

				const int previousCallbackArea = currentCallbackArea_;
				currentCallbackArea_ = transition.areaHandle;
				parser->CallFunc(currentCallback);
				currentCallbackArea_ = previousCallbackArea;
			}

			void DispatchProcessCallback()
			{
				const int callback = processCallback_;
				if (!IsFunction(callback))
					return;

				parser->CallFunc(callback);
			}

			std::unordered_map<int, Area> areas_;
			std::unordered_map<int, Registration> registrations_;
			int nextHandle_ = 1;
			int currentCallbackArea_ = InvalidAreaHandle;
			int processCallback_ = InvalidFunctionIndex;
			DWORD lastProcessTime_ = 0;
			bool timerStarted_ = false;
		};

		int __cdecl External_Create()
		{
			zSTRING world;
			parser->GetParameter(world);
			parser->SetReturn(AreaManager::Instance().Create(world));
			return 0;
		}

		int __cdecl External_AddPoint()
		{
			float z;
			float x;
			int handle;
			parser->GetParameter(z);
			parser->GetParameter(x);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().AddPoint(handle, x, z)));
			return 0;
		}

		int __cdecl External_SetHeight()
		{
			float maxY;
			float minY;
			int handle;
			parser->GetParameter(maxY);
			parser->GetParameter(minY);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().SetHeight(handle, minY, maxY)));
			return 0;
		}

		int __cdecl External_ClearHeight()
		{
			int handle;
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().ClearHeight(handle)));
			return 0;
		}

		int __cdecl External_SetMinHeight()
		{
			float minY;
			int handle;
			parser->GetParameter(minY);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().SetMinHeight(handle, minY)));
			return 0;
		}

		int __cdecl External_SetMaxHeight()
		{
			float maxY;
			int handle;
			parser->GetParameter(maxY);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().SetMaxHeight(handle, maxY)));
			return 0;
		}

		int __cdecl External_IsIn()
		{
			zSTRING world;
			float z;
			float y;
			float x;
			int handle;
			parser->GetParameter(world);
			parser->GetParameter(z);
			parser->GetParameter(y);
			parser->GetParameter(x);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().IsIn(handle, x, y, z, world)));
			return 0;
		}

		int __cdecl External_IsHeroIn()
		{
			int handle;
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().IsHeroIn(handle)));
			return 0;
		}

		int __cdecl External_Add()
		{
			int onExit;
			int onEnter;
			int handle;
			parser->GetParameter(onExit);
			parser->GetParameter(onEnter);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().Add(handle, onEnter, onExit)));
			return 0;
		}

		int __cdecl External_Remove()
		{
			int handle;
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().Remove(handle)));
			return 0;
		}

		int __cdecl External_Destroy()
		{
			int handle;
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().Destroy(handle)));
			return 0;
		}

		int __cdecl External_IsRegistered()
		{
			int handle;
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().IsRegistered(handle)));
			return 0;
		}

		int __cdecl External_GetCurrentArea()
		{
			parser->SetReturn(AreaManager::Instance().CurrentCallbackArea());
			return 0;
		}

		int __cdecl External_SetProcessCallback()
		{
			int callback;
			parser->GetParameter(callback);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().SetProcessCallback(callback)));
			return 0;
		}

		int __cdecl External_ClearProcessCallback()
		{
			AreaManager::Instance().ClearProcessCallback();
			return 0;
		}

		int __cdecl External_Clear()
		{
			AreaManager::Instance().Clear();
			return 0;
		}

		void RegisterExternals(zCParser* scriptParser)
		{
			scriptParser->DefineExternal("AM_CREATE", External_Create,
				zPAR_TYPE_INT, zPAR_TYPE_STRING, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ADDPOINT", External_AddPoint,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_FLOAT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_SETHEIGHT", External_SetHeight,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_FLOAT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_SETMINHEIGHT", External_SetMinHeight,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_SETMAXHEIGHT", External_SetMaxHeight,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_CLEARHEIGHT", External_ClearHeight,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ISIN", External_IsIn,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_FLOAT,
				zPAR_TYPE_FLOAT, zPAR_TYPE_STRING, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ISHEROIN", External_IsHeroIn,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ADD", External_Add,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FUNC, zPAR_TYPE_FUNC, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_REMOVE", External_Remove,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_DESTROY", External_Destroy,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ISREGISTERED", External_IsRegistered,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_GETCURRENTAREA", External_GetCurrentArea,
				zPAR_TYPE_INT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_SETPROCESSCALLBACK", External_SetProcessCallback,
				zPAR_TYPE_INT, zPAR_TYPE_FUNC, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_CLEARPROCESSCALLBACK", External_ClearProcessCallback,
				zPAR_TYPE_VOID, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_CLEAR", External_Clear,
				zPAR_TYPE_VOID, zPAR_TYPE_VOID);
		}
	}

	void __fastcall oCGame_Render_AreaManager(oCGame* self, void* vtable);
	auto Hook_oCGame_Render_AreaManager = Union::CreateHook(
		SIGNATURE_OF(&oCGame::Render),
		&oCGame_Render_AreaManager,
		Union::HookType::Hook_Detours);

	void __fastcall oCGame_Render_AreaManager(oCGame* self, void* vtable)
	{
		Hook_oCGame_Render_AreaManager(self, vtable);
		AreaManagerPlugin::AreaManager::Instance().Tick();
	}

	void __fastcall oCGame_DefineExternals_AreaManager(oCGame* self, void* vtable, zCParser* scriptParser);
	auto Hook_oCGame_DefineExternals_AreaManager = Union::CreateHook(
		SIGNATURE_OF(&oCGame::DefineExternals_Ulfi),
		&oCGame_DefineExternals_AreaManager,
		Union::HookType::Hook_Detours);

	void __fastcall oCGame_DefineExternals_AreaManager(oCGame* self, void* vtable, zCParser* scriptParser)
	{
		Hook_oCGame_DefineExternals_AreaManager(self, vtable, scriptParser);
		AreaManagerPlugin::RegisterExternals(scriptParser);
	}
}
