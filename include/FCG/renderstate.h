
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

	/// Read-only reference the top of the modelview matrix stack.
	[[nodiscard]] auto modelviewMatrix() const -> const glm::mat4& {
		return modelview.top();
	}

	/// Read-only reference the top of the projection matrix stack.
	[[nodiscard]] auto projectionMatrix() const -> const glm::mat4& {
		return projection.top();
	}

	/// Reference the current inverse modelview matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto invModelviewMatrix() -> const glm::mat4& {
		if (!invModelview.top().has_value()) {
			invModelview.top() = glm::inverse(modelview.top());
		}
		return invModelview.top().value();
	}

	/// Reference the top of the inverse modelview matrix stack from a \c const context. It is a logic error to call
	/// this accessor when the \em current inverse modelview matrix has never been queried before, since the \c const
	/// context does not allow updating it if it is currently invalid.
	[[nodiscard]] auto invModelviewMatrix() const -> const glm::mat4& {
		return invModelview.top().value();
	}

	/// Reference the current normal matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto normalMatrix() -> const glm::mat3& {
		if (!normal.top().has_value()) {
			normal.top() = glm::mat3(glm::transpose(glm::inverse(modelview.top())));
		}
		return normal.top().value();
	}

	/// Reference the top of the normal matrix stack from a \c const context. It is a logic error to call this accessor
	/// when the \em current normal matrix has never been queried before, since the \c const context does not allow
	/// updating it if it is currently invalid.
	[[nodiscard]] auto normalMatrix() const -> const glm::mat3& {
		return normal.top().value();
	}

	/// Reference the current inverse normal matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto invNormalMatrix() -> const glm::mat3& {
		if (!invNormal.top().has_value()) {
			invNormal.top() = glm::inverse(normal.top().value());
		}
		return invNormal.top().value();
	}

	/// Reference the top of the inverse normal matrix stack from a \c const context. It is a logic error to call this
	/// accessor when the \em current inverse normal matrix has never been queried before, since the \c const context
	/// does not allow updating it if it is currently invalid.
	[[nodiscard]] auto invNormalMatrix() const -> const glm::mat3& {
		return invNormal.top().value();
	}

	/// Reference the current inverse projection matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto invProjectionMatrix() -> const glm::mat4& {
		if (!invProjection.top().has_value()) {
			invProjection.top() = glm::inverse(projection.top());
		}
		return invProjection.top().value();
	}

	/// Reference the top of the inverse projection matrix stack from a \c const context. It is a logic error to call
	/// this accessor when the \em current inverse projection matrix has never been queried before, since the \c const
	/// context does not allow updating it if it is currently invalid.
	[[nodiscard]] auto invProjectionMatrix() const -> const glm::mat4& {
		return invProjection.top().value();
	}

	/// Reference the current modelview-projection matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto modelviewProjectionMatrix() -> const glm::mat4& {
		if (!modelviewProjection.has_value()) {
			modelviewProjection = projection.top() * modelview.top();
		}
		return modelviewProjection.value();
	}

	/// Reference the current modelview-projection matrix from a \c const context. It is a logic error to call this
	/// accessor when the \em current modelview-projection matrix has never been queried before, since the \c const
	/// context does not allow updating it if it is currently invalid.
	[[nodiscard]] auto modelviewProjectionMatrix() const -> const glm::mat4& {
		return modelviewProjection.value();
	}

	/// Reference the current inverse modelview-projection matrix. Will be lazily computed if it is currently invalid.
	[[nodiscard]] auto invModelviewProjectionMatrix() -> const glm::mat4& {
		if (!invModelviewProjection.has_value()) {
			invModelviewProjection = glm::inverse(modelviewProjectionMatrix());
		}
		return invModelviewProjection.value();
	}

	/// Reference the current inverse modelview-projection matrix from a \c const context. It is a logic error to call
	/// this accessor when the \em current inverse modelview-projection matrix has never been queried before, since the
	/// \c const context does not allow updating it if it is currently invalid.
	[[nodiscard]] auto invModelviewProjectionMatrix() const -> const glm::mat4& {
		return invModelviewProjection.value();
	}


	////
	// Methods

	/// Push the modelview matrix stack.
	void pushModelviewMatrix () {
		modelview.push(modelview.top());
		invModelview.push(invModelview.top());
		normal.push(normal.top());
		invNormal.push(invNormal.top());
	}

	/// Multiply the top of the modelview matrix stack by a new matrix.
	void mulModelviewMatrix (const glm::mat4& matrix) {
		modelview.top() *= matrix;
		onModelviewChange();
	}

	/// Load a new modelview matrix into the top of the stack.
	void loadModelviewMatrix (const glm::mat4& matrix) {
		modelview.top() = matrix;
		onModelviewChange();
	}

	/// Pop the modelview matrix stack.
	void popModelviewMatrix () {
		modelview.pop();
		invModelview.pop();
		normal.pop();
		invNormal.pop();
		invalidateDependentMatrices();
	}

	/// Push the projection matrix stack.
	void pushProjectionMatrix () {
		projection.push(projection.top());
		invProjection.push(invProjection.top());
	}

	/// Multiply the top of the projection matrix stack by a new matrix.
	void mulProjectionMatrix (const glm::mat4& matrix) {
		projection.top() *= matrix;
		onProjectionChange();
	}

	/// Load a new projection matrix into the top of the stack.
	void loadProjectionMatrix (const glm::mat4& matrix) {
		projection.top() = matrix;
		onProjectionChange();
	}

	/// Pop the projection matrix stack.
	void popProjectionMatrix () {
		projection.pop();
		invProjection.pop();
		invalidateDependentMatrices();
	}


private:

	////
	// Methods

	/// Invalidate unsynchronized dependent matrices
	inline void invalidateDependentMatrices () {
		modelviewProjection.reset();
		invModelviewProjection.reset();
	}

	/// Invalidate after a modelview matrix stack manipulation.
	inline void onModelviewChange () {
		invModelview.top().reset();
		normal.top().reset();
		invNormal.top().reset();
		invalidateDependentMatrices();
	}

	/// Invalidate after a projection matrix stack manipulation.
	inline void onProjectionChange () {
		invProjection.top().reset();
		invalidateDependentMatrices();
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
