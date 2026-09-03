
#ifndef __FCG_RENDER_STATE_H__
#define __FCG_RENDER_STATE_H__


//////
//
// Includes
//

// C++ STL
#include <stack>
#include <optional>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUTexture;

// Framework types
namespace fcg {
	class Device;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Structs
//

/// Structure of the viewing uniforms buffer.
struct ViewingUniforms
{
	/// The modelview matrix.
	glm::mat4 modelview;

	/// The inverse modelview matrix.
	glm::mat4 invModelview;

	/// The projection matrix.
	glm::mat4 projection;

	/// The inverse projection matrix.
	glm::mat4 invProjection;

	/// The combined modelview-projection matrix.
	glm::mat4 modelviewProjection;

	/// The inverse combined modelview-projection matrix.
	glm::mat4 invModelviewProjection;

	/// The normal matrix.
	glm::mat3 normal;

	/// The inverse normal matrix.
	glm::mat3 invNormal;
};



//////
//
// Classes
//

/// The central render state.
class FCG_FRAMEWORK_EXPORT RenderState
{
public:

	////
	// Object construction/destruction

	/// Construct using the provided device for resource creation. The device must outlive this render state.
	RenderState(Device &device);

	/// The destructor. Releases all GPU resources.
	~RenderState();

	/// RenderStates are not copyable.
	RenderState(const RenderState&) = delete;

	/// RenderStates are not copy-assignable.
	auto operator= (const RenderState&) -> RenderState& = delete;


	////
	// Accessors

	/* nothing here yet */


	////
	// Methods

	/* nothing here yet */


private:

	////
	// Methods

	/// Push a new modelview matrix onto the stack which is the product of the current top and the given matrix.
	inline void mulModelviewMatrix (const glm::mat4 &matrix) {
		pushModelviewMatrix(modelview.top()*matrix);
	}

	/// Push the given new modelview matrix onto the stack.
	void pushModelviewMatrix (const glm::mat4 &matrix)
	{
		modelview.push(matrix);
		invModelview.emplace(); //
		normal.emplace();       // <- pushes std::nullopt
		invNormal.emplace();    //
		modelviewProjection.reset();
		invModelviewProjection.reset();
	}

	/// Pop the current modelview matrix off the stack.
	void popModelviewMatrix ()
	{
		modelview.pop();
		invModelview.pop();
		normal.pop();
		invNormal.pop();
		modelviewProjection.reset();
		invModelviewProjection.reset();
	}

	/// Push a new projection matrix onto the stack which is the product of the current top and the given matrix.
	inline void mulProjectionMatrix (const glm::mat4 &matrix) {
		pushProjectionMatrix(projection.top()*matrix);
	}

	/// Push the given new projection matrix onto the stack.
	void pushProjectionMatrix (const glm::mat4 &matrix) {
		projection.push(matrix);
		invProjection.emplace(); // <- pushes std::nullopt
		modelviewProjection.reset();
		invModelviewProjection.reset();
	}

	/// Pop the current projection matrix off the stack.
	void popProjectionMatrix () {
		projection.pop();
		invProjection.pop();
		modelviewProjection.reset();
		invModelviewProjection.reset();
	}


	////
	// Fields

	/// The \ref ViewingUniforms::modelview matrix stack.
	std::stack<glm::mat4> modelview;

	/// The \ref ViewingUniforms::projection matrix stack.
	std::stack<glm::mat4> projection;

	/// Lazy matrix stack for the \ref ViewingUniforms::invModelview matrix.
	std::stack<std::optional<glm::mat4>> invModelview;

	/// Lazy matrix stack for the \ref ViewingUniforms::invProjection matrix.
	std::stack<std::optional<glm::mat4>> invProjection;

	/// Lazy \ref ViewingUniforms::modelviewProjection matrix.
	std::optional<glm::mat4> modelviewProjection;

	///Lazy \ref ViewingUniforms::invModelviewProjection matrix.
	std::optional<glm::mat4> invModelviewProjection;

	/// Lazy matrix stack for the \ref ViewingUniforms::normal matrix.
	std::stack<std::optional<glm::mat3>> normal;

	/// Lazy matrix stack for the \ref ViewingUniforms::invNormal matrix.
	std::stack<std::optional<glm::mat3>> invNormal;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_RENDER_STATE_H__
