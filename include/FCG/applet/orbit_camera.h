
#ifndef __FCG_ORBIT_CAMERA_H__
#define __FCG_ORBIT_CAMERA_H__


//////
//
// Includes
//

// C++ STL
#include <memory>
#include <optional>

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

		/// The aspect ratio of the camera's image plane (width/height).
		float aspect;

		/// The focal length of the camera in world units. For now, this only decides the "focal point" around which the
		/// camera orbits. It could also be used to render camera-related post-processing effects like depth-of-field.
		float f;

		/// The near clipping plane distance.
		float zNear;

		/// The far clipping plane distance.
		float zFar;
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

	void gui (Device &device, Player &player) override;

	void update (Device &device, Player &player) override;

	void render (Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass, Player &player) override;


private:

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


	////
	// Fields

	/// The current camera parameters.
	OrbitCameraParams params;

	/// The current view matrix resulting from the camera parameters.
	std::optional<glm::mat4> viewMatrix;

	/// The current projection matrix resulting from the camera parameters.
	std::optional<glm::mat4> projMatrix;
};



//////
//
// Namespaces close
//

// Our module namespace
} // namespace applet::fcg


#endif  // ifndef __FCG_ORBIT_CAMERA_H__
