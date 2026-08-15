// This file is compiled separately for every selected Gothic engine.

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace GOTHIC_NAMESPACE
{
	namespace AreaManagerPlugin
	{
		constexpr DWORD ProcessIntervalMs = 500;
		constexpr float DebugLineHeightOffset = 20.0F;
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

		std::string NormalizePortalRoomName(const char* value)
		{
			std::string normalized = value != nullptr ? value : "";

			std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::toupper(character));
			});

			return normalized;
		}

		struct Point
		{
			float x;
			float ceilingY;
			float z;
		};

		struct Triangle
		{
			std::size_t a;
			std::size_t b;
			std::size_t c;
		};

		class Area
		{
		public:
			explicit Area(std::string world)
				: world_(std::move(world))
			{
			}

			bool AddPoint(float x, float z)
			{
				if (!points_.empty() && hasPointCeilings_)
					return false;

				points_.push_back({ x, 0.0F, z });
				UpdateBounds(x, z);
				return true;
			}

			bool AddPoint3D(float x, float y, float z)
			{
				if (!points_.empty() && !hasPointCeilings_)
					return false;

				hasPointCeilings_ = true;
				points_.push_back({ x, y, z });
				UpdateBounds(x, z);
				ceilingTrianglesDirty_ = true;
				return true;
			}

			bool AddPortalRoom(std::string portalRoom)
			{
				if (portalRoom.empty())
					return false;

				return portalRooms_.insert(std::move(portalRoom)).second;
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
				const bool polygonValid = points_.size() >= 3
					&& (!hasPointCeilings_ || EnsureCeilingTriangles());
				return (polygonValid || !portalRooms_.empty()) && !world_.empty();
			}

			bool IsIn(float x, float y, float z, const std::string& world) const
			{
				return world == world_ && ContainsPosition(x, y, z);
			}

			bool ContainsPosition(float x, float y, float z) const
			{
				if (points_.size() < 3 || !ContainsHeight(y))
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

				if (!isIn)
					return false;

				if (hasPointCeilings_)
				{
					float ceilingY;
					if (!GetCeilingHeight(x, z, ceilingY) || y > ceilingY)
						return false;
				}

				return true;
			}

			bool ContainsHero(float x, float y, float z, const std::string& portalRoom) const
			{
				const bool isInPortalRoom = ContainsHeight(y)
					&& portalRooms_.contains(portalRoom);
				return isInPortalRoom || ContainsPosition(x, y, z);
			}

			bool CanRenderDebug() const
			{
				return points_.size() >= 3;
			}

			void RenderDebug(float height) const
			{
				if (!CanRenderDebug() || zlineCache == nullptr)
					return;

				if (hasMinHeight_ && height < minY_)
					height = minY_;

				if (hasMaxHeight_ && height > maxY_)
					height = maxY_;

				const zCOLOR color(255, 64, 64);
				std::size_t previous = points_.size() - 1;

				for (std::size_t current = 0; current < points_.size(); previous = current++)
				{
					const Point& from = points_[previous];
					const Point& to = points_[current];
					const float fromHeight = hasPointCeilings_ ? from.ceilingY : height;
					const float toHeight = hasPointCeilings_ ? to.ceilingY : height;
					zlineCache->Line3D(
						zVEC3(from.x, fromHeight, from.z),
						zVEC3(to.x, toHeight, to.z),
						color,
						0);
				}
			}

			const std::string& World() const
			{
				return world_;
			}

		private:
			bool ContainsHeight(float y) const
			{
				if (hasMinHeight_ && y < minY_)
					return false;

				if (hasMaxHeight_ && y > maxY_)
					return false;

				return true;
			}

			void UpdateBounds(float x, float z)
			{
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

			static double CrossXZ(const Point& a, const Point& b, const Point& c)
			{
				return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.z) - a.z)
					- (static_cast<double>(b.z) - a.z) * (static_cast<double>(c.x) - a.x);
			}

			double GeometryEpsilon() const
			{
				const double spanX = static_cast<double>(maxX_) - minX_;
				const double spanZ = static_cast<double>(maxZ_) - minZ_;
				const double span = std::max(spanX, spanZ);
				return std::max(1.0, span * span) * 1.0e-10;
			}

			static bool IsPointInTriangleXZ(
				const Point& point,
				const Point& a,
				const Point& b,
				const Point& c,
				double orientation,
				double epsilon)
			{
				return orientation * CrossXZ(a, b, point) >= -epsilon
					&& orientation * CrossXZ(b, c, point) >= -epsilon
					&& orientation * CrossXZ(c, a, point) >= -epsilon;
			}

			bool EnsureCeilingTriangles() const
			{
				if (!ceilingTrianglesDirty_)
					return ceilingTrianglesValid_;

				ceilingTrianglesDirty_ = false;
				ceilingTrianglesValid_ = false;
				ceilingTriangles_.clear();

				if (!hasPointCeilings_ || points_.size() < 3)
					return false;

				std::vector<std::size_t> remaining;
				remaining.reserve(points_.size());

				for (std::size_t index = 0; index < points_.size(); ++index)
					remaining.push_back(index);

				const double epsilon = GeometryEpsilon();

				double signedArea = 0.0;
				for (std::size_t current = 0; current < remaining.size(); ++current)
				{
					const Point& from = points_[remaining[current]];
					const Point& to = points_[remaining[(current + 1) % remaining.size()]];
					signedArea += static_cast<double>(from.x) * to.z
						- static_cast<double>(to.x) * from.z;
				}

				if (std::abs(signedArea) <= epsilon)
					return false;

				const double orientation = signedArea > 0.0 ? 1.0 : -1.0;

				while (remaining.size() > 3)
				{
					bool foundEar = false;

					for (std::size_t current = 0; current < remaining.size(); ++current)
					{
						const std::size_t previous = (current + remaining.size() - 1) % remaining.size();
						const std::size_t next = (current + 1) % remaining.size();
						const std::size_t a = remaining[previous];
						const std::size_t b = remaining[current];
						const std::size_t c = remaining[next];

						if (orientation * CrossXZ(points_[a], points_[b], points_[c]) <= epsilon)
							continue;

						bool containsVertex = false;
						for (const std::size_t candidate : remaining)
						{
							if (candidate == a || candidate == b || candidate == c)
								continue;

							if (IsPointInTriangleXZ(
								points_[candidate], points_[a], points_[b], points_[c],
								orientation, epsilon))
							{
								containsVertex = true;
								break;
							}
						}

						if (containsVertex)
							continue;

						ceilingTriangles_.push_back({ a, b, c });
						remaining.erase(remaining.begin() + current);
						foundEar = true;
						break;
					}

					if (!foundEar)
					{
						ceilingTriangles_.clear();
						return false;
					}
				}

				ceilingTriangles_.push_back({ remaining[0], remaining[1], remaining[2] });
				ceilingTrianglesValid_ = true;
				return true;
			}

			bool GetCeilingHeight(float x, float z, float& ceilingY) const
			{
				if (!EnsureCeilingTriangles())
					return false;

				constexpr double WeightEpsilon = 1.0e-5;
				const double geometryEpsilon = GeometryEpsilon();

				for (const Triangle& triangle : ceilingTriangles_)
				{
					const Point& a = points_[triangle.a];
					const Point& b = points_[triangle.b];
					const Point& c = points_[triangle.c];
					const double denominator = (static_cast<double>(b.z) - c.z) * (a.x - c.x)
						+ (static_cast<double>(c.x) - b.x) * (a.z - c.z);

					if (std::abs(denominator) <= geometryEpsilon)
						continue;

					const double weightA = ((static_cast<double>(b.z) - c.z) * (x - c.x)
						+ (static_cast<double>(c.x) - b.x) * (z - c.z)) / denominator;
					const double weightB = ((static_cast<double>(c.z) - a.z) * (x - c.x)
						+ (static_cast<double>(a.x) - c.x) * (z - c.z)) / denominator;
					const double weightC = 1.0 - weightA - weightB;

					if (weightA < -WeightEpsilon || weightB < -WeightEpsilon
						|| weightC < -WeightEpsilon)
					{
						continue;
					}

					ceilingY = static_cast<float>(weightA * a.ceilingY
						+ weightB * b.ceilingY + weightC * c.ceilingY);
					return true;
				}

				return false;
			}

			std::vector<Point> points_;
			mutable std::vector<Triangle> ceilingTriangles_;
			std::unordered_set<std::string> portalRooms_;
			std::string world_;
			bool hasPointCeilings_ = false;
			mutable bool ceilingTrianglesDirty_ = false;
			mutable bool ceilingTrianglesValid_ = false;

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

				return area->AddPoint(x, z);
			}

			bool AddPoint3D(int handle, float x, float y, float z)
			{
				auto* area = FindArea(handle);
				if (area == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
					return false;

				return area->AddPoint3D(x, y, z);
			}

			bool AddPortalRoom(int handle, const zSTRING& portalRoom)
			{
				auto* area = FindArea(handle);
				return area != nullptr
					&& area->AddPortalRoom(NormalizePortalRoomName(portalRoom.ToChar()));
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
				return area->ContainsHero(
					position[0],
					position[1],
					position[2],
					CurrentPlayerPortalRoom());
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

			bool SetDebug(bool enabled)
			{
				debugEnabled_ = enabled;
				return true;
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
				debugEnabled_ = false;
			}

			void RenderDebug() const
			{
				if (!debugEnabled_ || player == nullptr || ogame == nullptr
					|| ogame->GetWorld() == nullptr || zlineCache == nullptr)
				{
					return;
				}

				const float height = player->GetPositionWorld()[1] + DebugLineHeightOffset;

				for (const auto& entry : areas_)
				{
					const Area& area = entry.second;
					if (area.CanRenderDebug() && IsCurrentWorld(area))
						area.RenderDebug(height);
				}
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

			std::string CurrentPlayerPortalRoom() const
			{
				if (player == nullptr)
					return {};

				const zSTRING* portalRoom = player->GetSectorNameVobIsIn();
				return NormalizePortalRoomName(portalRoom != nullptr ? portalRoom->ToChar() : nullptr);
			}

			void Process()
			{
				if (player == nullptr || ogame == nullptr || ogame->GetWorld() == nullptr)
					return;

				const zVEC3 position = player->GetPositionWorld();
				const std::string portalRoom = CurrentPlayerPortalRoom();
				std::vector<Transition> transitions;
				transitions.reserve(registrations_.size());

				for (auto& [handle, registration] : registrations_)
				{
					const auto* area = FindArea(handle);
					if (area == nullptr || !IsCurrentWorld(*area))
						continue;

					const bool isIn = area->ContainsHero(
						position[0], position[1], position[2], portalRoom);
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
			bool debugEnabled_ = false;
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

		int __cdecl External_AddPoint3D()
		{
			float z;
			float y;
			float x;
			int handle;
			parser->GetParameter(z);
			parser->GetParameter(y);
			parser->GetParameter(x);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().AddPoint3D(handle, x, y, z)));
			return 0;
		}

		int __cdecl External_AddPortalRoom()
		{
			zSTRING portalRoom;
			int handle;
			parser->GetParameter(portalRoom);
			parser->GetParameter(handle);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().AddPortalRoom(handle, portalRoom)));
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

		int __cdecl External_SetDebug()
		{
			int enabled;
			parser->GetParameter(enabled);
			parser->SetReturn(static_cast<int>(AreaManager::Instance().SetDebug(enabled != 0)));
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
			scriptParser->DefineExternal("AM_ADDPOINT3D", External_AddPoint3D,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_FLOAT, zPAR_TYPE_FLOAT,
				zPAR_TYPE_FLOAT, zPAR_TYPE_VOID);
			scriptParser->DefineExternal("AM_ADDPORTALROOM", External_AddPortalRoom,
				zPAR_TYPE_INT, zPAR_TYPE_INT, zPAR_TYPE_STRING, zPAR_TYPE_VOID);
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
			scriptParser->DefineExternal("AM_SETDEBUG", External_SetDebug,
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
		AreaManagerPlugin::AreaManager::Instance().RenderDebug();
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
