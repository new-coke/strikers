// Compile the real factory translation unit so these are the types used by sizeof.
#ifndef CAMERA_LAYOUT_MANAGER_SOURCE
#error "Set CAMERA_LAYOUT_MANAGER_SOURCE to the CameraMan.cpp being tested."
#endif
#include CAMERA_LAYOUT_MANAGER_SOURCE
#include <type_traits>

template <class T>
constexpr bool valid_camera_layout = std::is_base_of_v<cBaseCamera, T> && sizeof(T) >= sizeof(cBaseCamera);

static_assert(valid_camera_layout<cDebugCamera>);
static_assert(valid_camera_layout<ReplayCamera>);
static_assert(valid_camera_layout<TopDownCamera>);
static_assert(valid_camera_layout<cFollowCamera>);
static_assert(valid_camera_layout<cKickOffCamera>);
static_assert(valid_camera_layout<GoalCamera>);
static_assert(valid_camera_layout<cShootToScoreCamera>);
static_assert(valid_camera_layout<cAnimViewerCamera>);
static_assert(valid_camera_layout<FaceCam>);
static_assert(valid_camera_layout<cAnimCamera>);
static_assert(valid_camera_layout<GameplayCamera>);
