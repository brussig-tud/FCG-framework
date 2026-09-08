
#ifndef __FCG_ORBIT_CAMERA_H__
#define __FCG_ORBIT_CAMERA_H__


//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <optional>
#include <variant>
#include <chrono>

// FCG Framework
#include <FCG/applet.h>




//////
//
// Namespaces open
//

/// Our module namespace.
namespace fcg::applet {



//////
//
// Classes
//

/// Stuct describing camera parameters.
struct OrbitCameraParams
{
	/// The intrinsic camera parameters (describing the camera's internal properties, i.e. its optics).
	struct Intrinsics
	{
		/// The vertical field of view in degrees.
		float fovY;

		/// The focal length of the camera in world units. For now, this only decides the "focal point" around which the
		/// camera orbits. It could also be used to render camera-related post-processing effects like depth-of-field.
		float f;

		/// The near clipping plane distance.
		float zNear;

		/// The far clipping plane distance.
		float zFar;

		/// Set \ref fovY such that the frustum has the given diameter at the focal point.
		void setFovYForFrustumDiameterAtFocus (float diameter) {
			const auto theta = glm::atan(.5f*diameter / f);
			fovY = theta+theta;
		}

		/// Set \ref f to the distance where the frustum has the given diameter under the current \ref fovY.
		void setFocusDistForFrustumDiameter (float diameter) {
			f = .5f*diameter/glm::tan(.5f*fovY);
		}

		/// Compute the frustum diameter at the current \ref f.
		[[nodiscard]] auto frustumDiameterAtFocus () const -> float {
			const auto h05 = f*glm::tan(.5f*fovY);
			return h05+h05;
		}
	} intrinsics;

	/// The extrinsic camera parameters (describing the camera's position and orientation in world space).
	struct Extrinsics
	{
		/// The camera's position in world space.
		glm::vec3 eye;

		/// The camera's viewing direction world space.
		glm::vec3 dir;

		/// The camera's up direction in world space.
		glm::vec3 up;
	} extrinsics;
};

/// An applet implementing an orbit camera.
class FCG_FRAMEWORK_EXPORT OrbitCamera : public Applet
{
	////
	// Types

	struct PendingReadbackInfo {
		/// The token associated with the readback operation.
		uint64_t token;

		/// The mouse click coordinates.
		glm::uvec2 clickPos;
	};

	struct FocusAnimation {
		/// The focal point when the animation began.
		glm::vec3 start;

		/// The resolved world-space focus point to transition to.
		glm::vec3 target;

		/// Accumulated seconds since the animation began, in [0, duration].
		float elapsed;
	};


public:

	////
	// Object construction/destruction

	/// Default constructor.
	OrbitCamera();

	/// The destructor.
	~OrbitCamera() override;


	////
	// Interface: fcg::Applet

	auto name () -> std::string& override;

	void init (Device &device, Player &player) override;

	void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) override;

	void onEvent (const Event &event, EventContext &context, Player &player) override;

	void gui (Device &device, Player &player) override;

	void update (Device &device, Player &player, float dt) override;

	void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, Player &player
		) override;


	////
	// Accessors

	/// Compute the focal point around which the camera orbits from a \c const context.
	///
	/// It is a logic error to call this accessor when the \em current focal point has never been queried before, since
	/// the \c const context does not allow updating it if it is currently invalid.
	[[nodiscard]] inline auto focalPoint () const -> const glm::vec3& {
		return m_focalPoint.value();
	}

	/// Compute the focal point around which the camera orbits. Will be lazily computed if it is currently invalid.
	[[nodiscard]] inline auto focalPoint () -> const glm::vec3& {
		if (!m_focalPoint.has_value()) {
			m_focalPoint = m_params.extrinsics.eye + m_params.intrinsics.f*m_params.extrinsics.dir;
		}
		return m_focalPoint.value();
	}

	/// Reference the current camera parameters.
	[[nodiscard]] inline auto params () const -> const OrbitCameraParams& {
		return m_params;
	}


	////
	// Methods

	/// Set a new vertical FoV, in degrees.
	void setFovY (float FovY);

	/// Set a new focal length.
	void setFocalLength (float f);

	/// Set a new near clipping plane distance.
	void setNearPlane (float zNear);

	/// Set a new far clipping plane distance.
	void setFarPlane (float zFar);

	/// Set a new eye point.
	void setEye (const glm::vec3 &eye);

	/// Set a new viewing direction.
	void setDir (const glm::vec3 &dir);

	/// Set a new up direction.
	void setUp (const glm::vec3 &up);

	/// Set a new focal point, updating the \link camera parameters params \endlink accordingly.
	void setFocalPoint (const glm::vec3 &focalPoint);

	/// Translates the camera such that the given point becomes the new focal point, updating the
	/// \link camera parameters params \endlink accordingly.
	void translateToFocalPoint (const glm::vec3 &focalPoint);


private:

	////
	// Fields

	/// The current camera parameters.
	OrbitCameraParams m_params;

	/// The current focal point of the orbit.
	std::optional<glm::vec3> m_focalPoint;

	/// The current view matrix resulting from the camera parameters.
	std::optional<glm::mat4> viewMatrix;

	/// The current projection matrix resulting from the camera parameters.
	std::optional<glm::mat4> projMatrix;

	/// Multiplier applied to all translation/zoom speeds.
	float speedFactor = 1.0f;

	/// The mouse button currently dragging the camera, or Unknown.
	MouseButton activeDragButton = MouseButton::Unknown;

	/// The time of the last left mouse click, if any, for double-click detection.
	std::optional<std::chrono::steady_clock::time_point> lastLeftClickTime;

	/// Whether Shift was held when the active drag started.
	bool dragStartedWithShift = false;

	/// For handling double-click-to-focus actions.
	std::variant<std::monostate, PendingReadbackInfo, glm::vec3, FocusAnimation> focusChange;
};



//////
//
// Namespaces close
//

// Our module namespace
} // namespace fcg::applet


#endif  // ifndef __FCG_ORBIT_CAMERA_H__
