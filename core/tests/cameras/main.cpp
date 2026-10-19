
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <cstdio>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

// FCG Framework
#include <FCG/camera_focus.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/applet/orbit_camera.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

using Intrinsics = fcg::CameraParameters::Intrinsics;

static_assert(!std::is_copy_constructible_v<fcg::CameraFocus>);
static_assert(!std::is_copy_assignable_v<fcg::CameraFocus>);
static_assert(!std::is_move_constructible_v<fcg::CameraFocus>);
static_assert(!std::is_move_assignable_v<fcg::CameraFocus>);

void check (bool condition, const std::source_location &location = std::source_location::current()) {
	if (!condition)
		throw std::runtime_error("Camera contract check failed at line " + std::to_string(location.line()));
}

[[nodiscard]] auto close (float a, float b, float tolerance = 1e-4f) -> bool {
	return std::abs(a - b) <= tolerance;
}

[[nodiscard]] auto close (const glm::vec3 &a, const glm::vec3 &b) -> bool {
	return glm::length(a - b) < 1e-4f;
}

[[nodiscard]] auto close (const glm::mat4 &a, const glm::mat4 &b) -> bool {
	for (unsigned col = 0; col < 4; ++col)
		for (unsigned row = 0; row < 4; ++row)
			if (!close(a[col][row], b[col][row]))
				return false;
	return true;
}

[[nodiscard]] auto same (const fcg::CameraParameters &a, const fcg::CameraParameters &b) -> bool
{
	if (a.intrinsics.fovY.index() != b.intrinsics.fovY.index())
		return false;
	const bool sameFov = a.intrinsics.fovY.index() == 0
		? std::get<Intrinsics::PerspectiveFov>(a.intrinsics.fovY).angle == std::get<Intrinsics::PerspectiveFov>(b.intrinsics.fovY).angle
		: std::get<Intrinsics::OrthoExtend>(a.intrinsics.fovY).size == std::get<Intrinsics::OrthoExtend>(b.intrinsics.fovY).size;
	return sameFov && a.intrinsics.f == b.intrinsics.f && a.intrinsics.zNear == b.intrinsics.zNear
		&& a.intrinsics.zFar == b.intrinsics.zFar && a.extrinsics.eye == b.extrinsics.eye
		&& a.extrinsics.dir == b.extrinsics.dir && a.extrinsics.up == b.extrinsics.up;
}

template <class Error = std::invalid_argument, class Action>
void rejects (
	fcg::Camera &camera, Action action, const std::source_location &location = std::source_location::current()
) {
	const auto before = camera.params();
	bool rejected = false;
	try { action(); }
	catch (const Error&) { rejected = true; }
	check(rejected && same(camera.params(), before), location);
}

void parameters ()
{
	Intrinsics in{.fovY=Intrinsics::PerspectiveFov{60}, .f=3, .zNear=.125f, .zFar=100};
	check(close(in.frustumDiameterAtFocus(), 6.f * std::tan(glm::radians(30.f))));
	in.setFovYForFrustumDiameterAtFocus(5.f);
	check(close(in.frustumDiameterAtFocus(), 5.f));
	check(close(std::get<Intrinsics::PerspectiveFov>(in.fovY).angle, glm::degrees(2.f * std::atan(2.5f / 3.f))));
	in.setFocusDistForFrustumDiameter(7.f);
	check(close(in.frustumDiameterAtFocus(), 7.f));
	for (unsigned choice = 0; choice < 2; ++choice)
	{
		if (choice)
			in.fovY = Intrinsics::OrthoExtend{4.f};
		const auto matrix = in.projectionMatrix({800, 400});
		const auto nearClip = matrix * glm::vec4(0, 0, -in.zNear, 1);
		const auto farClip = matrix * glm::vec4(0, 0, -in.zFar, 1);
		check(close(nearClip.z / nearClip.w, 0.f) && close(farClip.z / farClip.w, 1.f));
		const auto halfHeight = choice ? 2.f : in.zNear * std::tan(.5f * glm::radians(std::get<Intrinsics::PerspectiveFov>(in.fovY).angle));
		const auto edge = matrix * glm::vec4(2.f * halfHeight, halfHeight, -in.zNear, 1);
		check(close(edge.x / edge.w, 1.f) && close(edge.y / edge.w, 1.f));
		const auto square = in.projectionMatrix({400, 400});
		check(close(square[1][1], matrix[1][1]) && close(square[0][0], 2.f * matrix[0][0]));
	}
	const auto ortho = in.projectionMatrix({800, 400});
	in.f = 1000;
	check(close(in.frustumDiameterAtFocus(), 4.f) && close(ortho, in.projectionMatrix({800, 400})));
	in.setFovYForFrustumDiameterAtFocus(12.f);
	check(std::holds_alternative<Intrinsics::OrthoExtend>(in.fovY) && close(in.frustumDiameterAtFocus(), 12.f));
	try {
		in.setFocusDistForFrustumDiameter(8.f);
		check(false);
	}
	catch (const std::logic_error&) {
		check(in.f == 1000 && in.frustumDiameterAtFocus() == 12);
	}
	const fcg::CameraParameters::Extrinsics pose{{2, 3, 4}, {1, 0, 0}, {0, 0, 1}};
	const auto view = pose.modelviewMatrix();
	check(close(glm::vec3(view * glm::vec4(pose.eye, 1)), glm::vec3(0)));
	check(close(glm::vec3(view * glm::vec4(pose.eye + pose.dir, 1)), {0, 0, -1}));
	check(close(glm::vec3(view * glm::vec4(pose.up, 0)), {0, 1, 0}));
}

void common (fcg::Applet &applet, bool planar)
{
	auto *interface = dynamic_cast<fcg::Camera*>(&applet);
	check(interface != nullptr);
	fcg::Camera &camera = *interface;
	const fcg::Camera &readOnly = camera;
	check(close(readOnly.focalPoint(), glm::vec3(0)));
	check(close(camera.modelviewMatrix(), camera.params().extrinsics.modelviewMatrix()));
	check(close(camera.projectionMatrix({640, 480}), camera.params().intrinsics.projectionMatrix({640, 480})));
	camera.setFovY(planar ? Intrinsics::FovY{Intrinsics::OrthoExtend{6}} : Intrinsics::FovY{Intrinsics::PerspectiveFov{70}});
	camera.setFrustumDiameterAtFocus(8.f);
	check(close(camera.frustumDiameterAtFocus(), 8.f));
	if (planar)
		rejects<std::logic_error>(camera, [&] { camera.setFocusDistForFrustumDiameter(10); });
	else {
		camera.setFocusDistForFrustumDiameter(10);
		check(close(camera.frustumDiameterAtFocus(), 10));
	}
	camera.setFocalLength(-1);
	check(camera.params().intrinsics.f == .001f);
	camera.setFocalLength(5);
	camera.setNearPlane(-1);
	check(camera.params().intrinsics.zNear == .001f);
	camera.setNearPlane(200);
	check(camera.params().intrinsics.zFar > 200);
	camera.setFarPlane(100);
	check(camera.params().intrinsics.zFar > 200);
	camera.setNearPlane(.125f);
	camera.setFarPlane(100);
	camera.setEye({2, 3, 5});
	camera.setDir({0, 0, -7});
	camera.setUp({1, 1, 0});
	check(close(camera.params().extrinsics.dir, {0, 0, -1}));
	check(close(glm::length(camera.params().extrinsics.up), 1.f));
	camera.setFocalPoint({3, 4, 0});
	check(close(camera.focalPoint(), {3, 4, 0}));
	const auto direction = camera.params().extrinsics.dir;
	const auto up = camera.params().extrinsics.up;
	const auto f = camera.params().intrinsics.f;
	camera.translateToFocalPoint({7, 8, 0});
	check(close(camera.focalPoint(), {7, 8, 0}) && camera.params().intrinsics.f == f);
	check(camera.params().extrinsics.dir == direction && camera.params().extrinsics.up == up);
	camera.resetRotation();
	check(close(camera.focalPoint(), {7, 8, 0}) && camera.params().intrinsics.f == f);
	check(camera.params().extrinsics.dir == glm::vec3(0, 0, -1) && camera.params().extrinsics.up == glm::vec3(0, 1, 0));
	camera.resetFocus();
	check(close(camera.focalPoint(), glm::vec3(0)));
	const auto wrongFov = planar ? Intrinsics::FovY{Intrinsics::PerspectiveFov{60}} : Intrinsics::FovY{Intrinsics::OrthoExtend{4}};
	rejects(camera, [&] { camera.setFovY(wrongFov); });
	rejects(camera, [&] { camera.setDir({0, 0, 0}); });
	rejects(camera, [&] { camera.setUp({0, 0, 0}); });
	rejects(camera, [&] { camera.setUp(camera.params().extrinsics.dir); });
	rejects(camera, [&] { camera.setNearPlane(std::numeric_limits<float>::max()); });
	for (const float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
	{
		rejects(camera, [&] { camera.setFovY(planar ? Intrinsics::FovY{Intrinsics::OrthoExtend{invalid}} : Intrinsics::FovY{Intrinsics::PerspectiveFov{invalid}}); });
		rejects(camera, [&] { camera.setFocalLength(invalid); });
		rejects(camera, [&] { camera.setNearPlane(invalid); });
		rejects(camera, [&] { camera.setFarPlane(invalid); });
		rejects(camera, [&] { camera.setEye({invalid, 0, 0}); });
		rejects(camera, [&] { camera.setDir({0, invalid, -1}); });
		rejects(camera, [&] { camera.setUp({0, 1, invalid}); });
		rejects(camera, [&] { camera.setFocalPoint({0, invalid, 0}); });
		rejects(camera, [&] { camera.translateToFocalPoint({0, 0, invalid}); });
		rejects(camera, [&] { camera.setFrustumDiameterAtFocus(invalid); });
		rejects(camera, [&] { camera.setFocusDistForFrustumDiameter(invalid); });
	}
}

/// Exercise the shared axis setters, including validation before the unchecked orientation hook mutates state.
void axes (fcg::Camera &camera)
{
	const auto before = camera.params();
	for (const float scale : {std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max()}) {
		camera.setUp({0, scale, scale});
		check(close(camera.params().extrinsics.up, {0, 1, 0}));
		camera.setDir({0, 0, -scale});
		check(close(camera.params().extrinsics.dir, {0, 0, -1}));
	}
	camera.setUp({1, 2, 3});
	check(close(camera.params().extrinsics.up, glm::normalize(glm::vec3(1, 2, 0))));
	check(close(glm::length(camera.params().extrinsics.up), 1.f));
	check(std::abs(glm::dot(camera.params().extrinsics.dir, camera.params().extrinsics.up)) < 1e-6f);
	check(camera.params().extrinsics.eye == before.extrinsics.eye && camera.params().intrinsics.f == before.intrinsics.f);
	rejects(camera, [&] { camera.setUp(-camera.params().extrinsics.dir); });
	rejects(camera, [&] { camera.setDir(-camera.params().extrinsics.up); });
	rejects(camera, [&] { camera.setUp({0, 5e-6f, -1}); });
	rejects(camera, [&] { camera.setDir(camera.params().extrinsics.up + glm::vec3(0, 0, 5e-6f)); });
	camera.setUp({0, 2e-5f, -1});
	check(close(camera.params().extrinsics.up, {0, 1, 0}));
	for (const float scale : {std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max()}) {
		camera.setOrientation({0, 0, -scale}, {scale, scale, scale});
		check(close(camera.params().extrinsics.up, glm::normalize(glm::vec3(1, 1, 0))));
	}
	rejects(camera, [&] { camera.setOrientation({0, 0, 0}, {0, 1, 0}); });
	rejects(camera, [&] { camera.setOrientation({0, 0, -1}, {0, 0, 0}); });
	rejects(camera, [&] { camera.setOrientation({0, 0, -1}, {0, 0, 1}); });
	rejects(camera, [&] { camera.setOrientation({0, 0, -1}, {0, 5e-6f, -1}); });
	for (const float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
		rejects(camera, [&] { camera.setOrientation({invalid, 0, -1}, {0, 1, 0}); });
		rejects(camera, [&] { camera.setOrientation({0, 0, -1}, {0, 1, invalid}); });
	}
	camera.resetRotation();
}

void planar ()
{
	fcg::applet::Camera2D camera;
	camera.setEyeZ(7);
	camera.setFocalLength(2);
	camera.setViewHeight(9);
	const auto projection = camera.projectionMatrix({800, 600});
	camera.setFocalLength(3);
	check(close(camera.projectionMatrix({800, 600}), projection));
	camera.setRotation(450);
	check(close(camera.rotation(), 90) && close(camera.params().extrinsics.up, {-1, 0, 0}));
	camera.setRotation(180);
	check(close(camera.rotation(), -180));
	camera.setRotation(-540);
	check(close(camera.rotation(), -180));
	camera.setUp({1, 1, 1e-6f});
	check(camera.params().extrinsics.up.z == 0.f);
	camera.setDir({1e-6f, 0, -2});
	check(camera.params().extrinsics.dir == glm::vec3(0, 0, -1));
	check(camera.params().extrinsics.up.z == 0.f);
	check(close(camera.params().extrinsics.up, glm::normalize(glm::vec3(1, 1, 0))));
	rejects(camera, [&] { camera.setDir({0, 0, 1}); });
	rejects(camera, [&] { camera.setDir({.01f, 0, -1}); });
	rejects(camera, [&] { camera.setDir({1.1e-5f, 0, -1}); });
	camera.setUp({0, 1, 1});
	check(camera.params().extrinsics.up == glm::vec3(0, 1, 0));
	camera.setUp({1, 2, 3});
	check(camera.params().extrinsics.up.z == 0.f);
	check(close(camera.params().extrinsics.up, glm::normalize(glm::vec3(1, 2, 0))));
	rejects(camera, [&] { camera.setOrientationUnchecked({1, 0, 0}, {0, 1, 0}); });
	rejects(camera, [&] { camera.setOrientation({0, 1, 0}, {0, 0, 1}); });
	fcg::Camera &interface = camera;
	interface.setOrientationUnchecked({0, 0, -1}, {1, 0, 0});
	check(camera.params().extrinsics.up == glm::vec3(1, 0, 0));
	camera.setOrientation({1e-6f, 0, -2}, {1, 1, 1});
	check(camera.params().extrinsics.dir == glm::vec3(0, 0, -1));
	check(camera.params().extrinsics.up.z == 0.f);
	check(close(glm::length(camera.params().extrinsics.up), 1.f));
	interface.setOrientationUnchecked(glm::normalize(glm::vec3(1e-6f, 0, -2)), {0, 1, 0});
	check(camera.params().extrinsics.dir == glm::vec3(0, 0, -1));
	check(camera.params().extrinsics.up == glm::vec3(0, 1, 0));
	rejects(camera, [&] { camera.setFocalPoint(glm::vec3(1, 2, 5)); });
	rejects(camera, [&] { camera.translateToFocalPoint({1, 2, 5}); });
	camera.setFocalPoint(glm::vec3(1, 2, 4.f + 5e-6f));
	check(camera.params().extrinsics.eye.z == 7.f && camera.focalPoint().z == 4.f);
	camera.translateToFocalPoint({3, 4, 4});
	camera.setFocalPoint(glm::vec2(5, 6));
	camera.resetRotation();
	check(camera.focalPoint() == glm::vec3(5, 6, 4) && camera.frustumDiameterAtFocus() == 9);
	camera.setRotation(37);
	camera.resetFocus();
	check(camera.focalPoint() == glm::vec3(0, 0, 4) && close(camera.rotation(), 37));
	check(camera.params().extrinsics.eye.z == 7 && camera.params().intrinsics.f == 3);
	camera.setViewHeight(-1);
	check(camera.frustumDiameterAtFocus() == .001f);
	camera.setViewHeight(1e6f);
	check(camera.frustumDiameterAtFocus() == 100000.f);
	rejects(camera, [&] { camera.setRotation(std::numeric_limits<float>::infinity()); });
	rejects(camera, [&] { camera.setEyeZ(std::numeric_limits<float>::quiet_NaN()); });
}

void orbit ()
{
	fcg::applet::OrbitCamera camera;
	camera.setUp({0, 1, 1});
	check(camera.params().extrinsics.up == glm::vec3(0, 1, 0));
	const auto before = camera.params();
	camera.setDir({0, 1, -1});
	check(close(camera.params().extrinsics.dir, glm::normalize(glm::vec3(0, 1, -1))));
	check(close(camera.params().extrinsics.up, glm::normalize(glm::vec3(0, 1, 1))));
	check(std::abs(glm::dot(camera.params().extrinsics.dir, camera.params().extrinsics.up)) < 1e-6f);
	check(camera.params().extrinsics.eye == before.extrinsics.eye && camera.params().intrinsics.f == before.intrinsics.f);
	fcg::Camera &interface = camera;
	interface.setOrientationUnchecked({1, 0, 0}, {0, 0, 1});
	check(camera.params().extrinsics.dir == glm::vec3(1, 0, 0));
	check(camera.params().extrinsics.up == glm::vec3(0, 0, 1));
	camera.resetRotation();
	camera.setEye({1e20f, 0, 3});
	rejects(camera, [&] { camera.setDir({1, 0, 0}); });
	rejects(camera, [&] { camera.setOrientation({1, 0, 0}, {0, 1, 0}); });
	camera.setEye({0, 0, 3});
	rejects(camera, [&] { camera.setFocalPoint(camera.params().extrinsics.eye); });
	rejects(camera, [&] { camera.setDir({0, 1, 0}); });
	camera.setFocalPoint({0, 4, 3}); // Retargeting parallel to the previous up chooses a valid fallback.
	check(close(camera.focalPoint(), {0, 4, 3}));
	check(close(glm::dot(camera.params().extrinsics.dir, camera.params().extrinsics.up), 0));
	const auto focus = camera.focalPoint();
	const auto f = camera.params().intrinsics.f;
	camera.resetRotation();
	check(close(camera.focalPoint(), focus) && camera.params().intrinsics.f == f);
	camera.setFovY(-10);
	check(std::get<Intrinsics::PerspectiveFov>(camera.params().intrinsics.fovY).angle == 1);
	camera.setFovY(200);
	check(std::get<Intrinsics::PerspectiveFov>(camera.params().intrinsics.fovY).angle == 179);
}

/// Verify complete orientation commits and quarter-turns through concrete and shared camera interfaces.
void orientationCommits ()
{
	/// Records the complete poses visible around the single implementation hook.
	struct ObservedCamera final : fcg::applet::OrbitCamera
	{
		/// The number of completed hook calls.
		unsigned commits = 0;

		/// The complete pose observed on entry to the hook.
		fcg::CameraParameters before{};

		/// The complete pose observed after the hook completes.
		fcg::CameraParameters after{};

		/// Record and apply the prepared orientation in one hook call.
		void setOrientationUnchecked (const glm::vec3 &dir, const glm::vec3 &up) override {
			before = params();
			OrbitCamera::setOrientationUnchecked(dir, up);
			after = params();
			++commits;
		}
	};
	ObservedCamera camera;
	fcg::Camera &interface = camera;
	const auto verifyCommit = [&] (auto action)
	{
		const auto before = camera.params();
		const auto commits = camera.commits;
		action();
		check(camera.commits == commits + 1);
		check(same(camera.before, before) && same(camera.after, camera.params()));
		check(camera.after.extrinsics.eye == before.extrinsics.eye && camera.after.intrinsics.f == before.intrinsics.f);
		for (const auto &pose : {camera.before, camera.after})
		{
			check(std::abs(glm::dot(pose.extrinsics.dir, pose.extrinsics.up)) < 1e-6f);
			const auto view = pose.extrinsics.modelviewMatrix();
			for (unsigned col = 0; col < 4; ++col)
				for (unsigned row = 0; row < 4; ++row)
					check(std::isfinite(view[col][row]));
		}
	};
	const auto initial = camera.params().extrinsics;
	verifyCommit([&] { camera.setOrientation(camera.params().extrinsics.up, -camera.params().extrinsics.dir); });
	check(camera.params().extrinsics.dir == initial.up && camera.params().extrinsics.up == -initial.dir);
	verifyCommit([&] { interface.setOrientation({1, -2, -3}, {2, 3, 1}); });
	const auto rotated = camera.params().extrinsics;
	verifyCommit([&] { interface.setOrientation(camera.params().extrinsics.up, -camera.params().extrinsics.dir); });
	check(close(camera.params().extrinsics.dir, rotated.up) && close(camera.params().extrinsics.up, -rotated.dir));
	const auto aliased = camera.params().extrinsics;
	verifyCommit([&] { interface.setOrientation(camera.params().extrinsics.up, camera.params().extrinsics.dir); });
	check(close(camera.params().extrinsics.dir, aliased.up) && close(camera.params().extrinsics.up, aliased.dir));
	verifyCommit([&] { interface.setOrientationUnchecked(camera.params().extrinsics.up, camera.params().extrinsics.dir); });
	check(close(camera.params().extrinsics.dir, aliased.dir) && close(camera.params().extrinsics.up, aliased.up));
	verifyCommit([&] { interface.setDir(camera.params().extrinsics.dir + camera.params().extrinsics.up); });
	verifyCommit([&] { interface.setUp(camera.params().extrinsics.up + camera.params().extrinsics.dir); });
	const auto commits = camera.commits;
	rejects(camera, [&] { interface.setOrientation({0, 0, -1}, {0, 0, -1}); });
	check(camera.commits == commits);
}

// Anonymous namespace end
}



//////
//
// Functions
//

[[nodiscard]] auto main () -> int {
	try
	{
		parameters();
		fcg::applet::OrbitCamera orbitCamera;
		fcg::applet::Camera2D planarCamera;
		axes(orbitCamera);
		axes(planarCamera);
		common(orbitCamera, false);
		common(planarCamera, true);
		fcg::CameraFocus orbitFocus(orbitCamera);
		fcg::CameraFocus planarFocus(planarCamera, true);
		orbitFocus.cancel();
		planarFocus.resized();
		planar();
		orbit();
		orientationCommits();
	}
	catch (const std::exception &error) {
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
	return 0;
}
