
#ifndef __FCG_RENDER_STATE_H__
#define __FCG_RENDER_STATE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_render_state Render state
/// \ingroup fcg_modules
/// \brief Modelview and projection matrix stacks, their derived matrices, and pushing them to shaders.
///
/// <tt>\ref fcg::RenderState "RenderState"</tt> holds the transformation state of the current frame: a modelview and a
/// projection matrix stack, each starting at identity. The derived matrices that shaders typically need — inverses,
/// the normal matrix, the combined modelview-projection matrix — are computed on demand from the stack tops and cached
/// until the stacks change. <tt>\ref fcg::ViewingUniforms "ViewingUniforms"</tt> is the uniform block that carries
/// these matrices into shaders.
///
/// In applications run by <tt>\ref fcg::run "run"</tt>, one render state is created per scene refresh and passed to
/// the active camera's <tt>\ref fcg::Applet::render "Applet::render"</tt> callback first, followed by ordinary applets
/// in registration order. Without an active camera, the scene starts with identity viewing matrices.
///
/// \section fcg_render_state_usage Getting started
/// Load the matrices for the frame, compose transforms by right-multiplication, and push the uniform block to the
/// shader stage that needs it:
/// \snippet render_state_examples.cpp render_state_usage
///
/// \section fcg_render_state_workflows Common tasks
/// \subsection fcg_render_state_workflows_stacks Working with the matrix stacks
/// <tt>\ref fcg::RenderState::loadModelviewMatrix "loadModelviewMatrix"</tt> and
/// <tt>\ref fcg::RenderState::mulModelviewMatrix "mulModelviewMatrix"</tt> replace and right-multiply the top of a
/// stack. <tt>\ref fcg::RenderState::pushModelviewMatrix "pushModelviewMatrix"</tt> saves the current matrices and
/// <tt>\ref fcg::RenderState::popModelviewMatrix "popModelviewMatrix"</tt> restores them; don't pop the entry that was
/// pushed first. This is how scene hierarchies are walked: push, draw the subtree, pop.
///
/// \subsection fcg_render_state_workflows_derived Derived matrices
/// The derived accessors (<tt>\ref fcg::RenderState::invModelviewMatrix "invModelviewMatrix"</tt>,
/// <tt>\ref fcg::RenderState::normalMatrix "normalMatrix"</tt>,
/// <tt>\ref fcg::RenderState::modelviewProjectionMatrix "modelviewProjectionMatrix"</tt>, …) compute and cache on first
/// use. Accessing a derived matrix from a \c const render state requires that the non-const overload has been called
/// since the last change to the stacks.
///
/// The managed normal matrices are only valid when an invertible affine \ref fcg::RenderState::modelviewMatrix
/// "modelview" matrix is used, i.e., its bottom row is <tt>(0, 0, 0, 1)</tt> and its upper-left 3x3 part is invertible.
/// Rotations, translations, nonzero scales, and shears all fulfill this requirement and can be combined in such a
/// matrix. In general, as long as the camera projection is kept in the projection stack, this is typically not an
/// issue. Also, normals transformed by the normal matrix usually need to be normalized afterwards (unless the modelview
/// matrix is known to contain only rotation and translation).
///
/// \subsection fcg_render_state_workflows_uniforms Matching shader declaration
/// <tt>\ref fcg::ViewingUniforms "ViewingUniforms"</tt> is a \c std140 uniform block; its field order must match the
/// shader's declaration. See its class documentation for the exact layout.
///
/// \see \ref fcg_applets, \ref fcg_viewing, \ref fcg_devices



//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <stack>
#include <optional>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/device.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUCommandBuffer;

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

/// \addtogroup fcg_render_state
/// @{



//////
//
// Structs and enums
//

/// \brief A 3x3 matrix laid out as required by the \c std140 rules, where each column occupies a full \c vec4 slot.
///
/// Use it wherever a \c mat3 has to be stored in a \c std140 uniform block, e.g. in
/// <tt>\ref fcg::ViewingUniforms "ViewingUniforms"</tt>.
struct alignas(16) Std140Mat3
{
	/// The three matrix columns. The fourth component of each column is padding and always \c 0.
	glm::vec4 columns[3];

	/// Convert a GLM matrix into the padded \c std140 layout.
	///
	/// \param matrix The matrix to convert.
	Std140Mat3(const glm::mat3 &matrix)
		: columns{{matrix[0], 0.f}, {matrix[1], 0.f}, {matrix[2], 0.f}}
	{}
};
static_assert(sizeof(Std140Mat3) == 3 * sizeof(glm::vec4));

/// \brief The viewing matrices of a <tt>\ref fcg::RenderState "RenderState"</tt>, laid out as a \c std140 uniform block.
///
/// <tt>\ref fcg::RenderState::pushViewingUniforms "RenderState::pushViewingUniforms"</tt> uploads this block to a
/// shader. The uniform block declared in the shader must contain the same members in the same order: six \c mat4
/// followed by two \c mat3, for a total of 480 bytes.
struct ViewingUniforms
{
	/// The modelview matrix, transforming object coordinates into eye coordinates.
	glm::mat4 modelview;

	/// The inverse of the modelview matrix.
	glm::mat4 invModelview;

	/// The projection matrix, transforming eye coordinates into clip coordinates.
	glm::mat4 projection;

	/// The inverse of the projection matrix.
	glm::mat4 invProjection;

	/// The combined matrix <tt>projection * modelview</tt>.
	glm::mat4 modelviewProjection;

	/// The inverse of the combined modelview-projection matrix.
	glm::mat4 invModelviewProjection;

	/// \brief
	/// 	The object-to-eye normal matrix, i.e. the inverse transpose of the upper-left 3x3 part of the modelview
	/// 	matrix.
	///
	/// The modelview matrix must be affine and invertible.
	Std140Mat3 normal;

	/// The inverse of \c normal for the same affine, invertible modelview matrix.
	Std140Mat3 invNormal;
};
static_assert(offsetof(ViewingUniforms, normal) == 6 * sizeof(glm::mat4));
static_assert(offsetof(ViewingUniforms, invNormal) == 6 * sizeof(glm::mat4) + sizeof(Std140Mat3));
static_assert(sizeof(ViewingUniforms) == 6 * sizeof(glm::mat4) + 2 * sizeof(Std140Mat3));



//////
//
// Classes
//

/// \brief Holds the viewing transformations used while rendering a frame.
///
/// A render state maintains two matrix stacks, one for the modelview matrix and one for the projection matrix, in the
/// style of classic OpenGL. Both stacks start out with a single identity matrix. The top entry of each stack is the
/// current matrix; it can be replaced (\c load…), right-multiplied (\c mul…), saved (\c push…) and restored (\c pop…).
///
/// The derived matrices (inverses, the normal matrix and the combined modelview-projection matrix) as well as the
/// complete <tt>\ref fcg::ViewingUniforms "ViewingUniforms"</tt> block are computed on demand by the non-const
/// accessors and then reused until the matrix stacks change. The \c const overloads of these accessors can only return
/// values that have already been computed. Their precondition is that the non-const overload has been called since the
/// last change to the matrix stacks. A change is any load, multiplication or pop.
///
/// The framework creates one render state per scene refresh and passes it to \c Applet::render of every applet in turn.
/// Camera applets typically load their view and projection matrices into it, and subsequent applets read the matrices
/// or upload them to their shaders with <tt>\ref pushViewingUniforms</tt>. The runtime also captures the scene viewport
/// dimensions at the beginning of the render pass. Custom passes can record their viewport dimensions with
/// <tt>\ref setViewportSize</tt>.
///
/// Normal matrices and complete viewing uniforms require an invertible affine modelview matrix.
///
/// References returned by the accessors are valid only until the next change to the matrix stacks. Copy the values if
/// they are needed beyond that.
class FCG_FRAMEWORK_EXPORT RenderState
{
public:

	////
	// Object construction/destruction

	/// Construct a render state with identity modelview and projection matrices.
	///
	/// \param device The device used for rendering.
	RenderState(Device &device);

	/// \brief Construct a render state with identity matrices and a viewport size.
	///
	/// \param device The device used for rendering.
	/// \param viewportSize The current viewport dimensions in physical framebuffer pixels; zero means unspecified.
	RenderState(Device &device, glm::uvec2 viewportSize);

	/// The destructor.
	~RenderState();

	/// Not copyable.
	RenderState(const RenderState&) = delete;

	/// Not copy-assignable.
	auto operator= (const RenderState&) -> RenderState& = delete;


	////
	// Accessors

	/// The current viewport dimensions in physical framebuffer pixels, or zero when unspecified.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2 {
		return m_viewportSize;
	}

	/// \brief Record the dimensions of the viewport used by subsequent draws.
	///
	/// Update this value when changing the GPU viewport. The GPU viewport itself is set separately.
	///
	/// \param size The viewport dimensions in physical framebuffer pixels; zero means unspecified.
	void setViewportSize (glm::uvec2 size) {
		m_viewportSize = size;
	}

	/// \brief The current modelview matrix, i.e. the top of the modelview matrix stack.
	[[nodiscard]] auto modelviewMatrix () const -> const glm::mat4& {
		return modelview.top();
	}

	/// \brief The current projection matrix, i.e. the top of the projection matrix stack.
	[[nodiscard]] auto projectionMatrix () const -> const glm::mat4& {
		return projection.top();
	}

	/// \brief The inverse of the current modelview matrix, computing it if necessary.
	///
	/// \return A reference to the inverse modelview matrix.
	[[nodiscard]] auto invModelviewMatrix () -> const glm::mat4& {
		if (!invModelview.top().has_value())
			invModelview.top() = glm::inverse(modelview.top());
		return invModelview.top().value();
	}

	/// \brief The inverse of the current modelview matrix, as last computed by the non-const overload.
	///
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the inverse modelview matrix.
	///
	/// \throws std::bad_optional_access if the precondition is violated.
	[[nodiscard]] auto invModelviewMatrix () const -> const glm::mat4& {
		return invModelview.top().value();
	}

	/// \brief The normal matrix for the current modelview matrix, computing it if necessary.
	///
	/// \pre The current modelview matrix is affine and invertible.
	///
	/// \return A reference to the inverse transpose of the upper-left 3x3 part of the modelview matrix.
	[[nodiscard]] auto normalMatrix () -> const glm::mat3& {
		if (!normal.top().has_value())
			normal.top() = glm::transpose(glm::mat3(invModelviewMatrix()));
		return normal.top().value();
	}

	/// \brief The normal matrix for the current modelview matrix, as last computed by the non-const overload.
	///
	/// \pre The current modelview matrix is affine and invertible.
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the inverse transpose of the upper-left 3x3 part of the modelview matrix.
	///
	/// \throws std::bad_optional_access if the normal matrix has not been computed.
	[[nodiscard]] auto normalMatrix () const -> const glm::mat3& {
		return normal.top().value();
	}

	/// \brief The inverse of the current normal matrix, computing it if necessary.
	///
	/// \pre The current modelview matrix is affine and invertible.
	///
	/// \return A reference to the inverse normal matrix.
	[[nodiscard]] auto invNormalMatrix () -> const glm::mat3& {
		if (!invNormal.top().has_value())
			invNormal.top() = glm::inverse(normalMatrix());
		return invNormal.top().value();
	}

	/// \brief The inverse of the current normal matrix, as last computed by the non-const overload.
	///
	/// \pre The current modelview matrix is affine and invertible.
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the inverse normal matrix.
	///
	/// \throws std::bad_optional_access if the inverse normal matrix has not been computed.
	[[nodiscard]] auto invNormalMatrix () const -> const glm::mat3& {
		return invNormal.top().value();
	}

	/// \brief The inverse of the current projection matrix, computing it if necessary.
	///
	/// \return A reference to the inverse projection matrix.
	[[nodiscard]] auto invProjectionMatrix () -> const glm::mat4& {
		if (!invProjection.top().has_value())
			invProjection.top() = glm::inverse(projection.top());
		return invProjection.top().value();
	}

	/// \brief The inverse of the current projection matrix, as last computed by the non-const overload.
	///
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the inverse projection matrix.
	///
	/// \throws std::bad_optional_access if the precondition is violated.
	[[nodiscard]] auto invProjectionMatrix () const -> const glm::mat4& {
		return invProjection.top().value();
	}

	/// \brief The combined modelview-projection matrix, computing it if necessary.
	///
	/// \return A reference to the matrix <tt>projectionMatrix() * modelviewMatrix()</tt>.
	[[nodiscard]] auto modelviewProjectionMatrix () -> const glm::mat4& {
		if (!modelviewProjection.has_value())
			modelviewProjection = projection.top() * modelview.top();
		return modelviewProjection.value();
	}

	/// \brief The combined modelview-projection matrix, as last computed by the non-const overload.
	///
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the matrix <tt>projectionMatrix() * modelviewMatrix()</tt>.
	///
	/// \throws std::bad_optional_access if the precondition is violated.
	[[nodiscard]] auto modelviewProjectionMatrix () const -> const glm::mat4& {
		return modelviewProjection.value();
	}

	/// \brief The inverse of the combined modelview-projection matrix, computing it if necessary.
	///
	/// \return A reference to the inverse modelview-projection matrix.
	[[nodiscard]] auto invModelviewProjectionMatrix () -> const glm::mat4& {
		if (!invModelviewProjection.has_value())
			invModelviewProjection = glm::inverse(modelviewProjectionMatrix());
		return invModelviewProjection.value();
	}

	/// \brief The inverse of the combined modelview-projection matrix, as last computed by the non-const overload.
	///
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the inverse modelview-projection matrix.
	///
	/// \throws std::bad_optional_access if the precondition is violated.
	[[nodiscard]] auto invModelviewProjectionMatrix () const -> const glm::mat4& {
		return invModelviewProjection.value();
	}

	/// \brief All current viewing matrices as a uniform block, computing them if necessary.
	///
	/// \pre The current modelview matrix is affine and invertible.
	///
	/// \return A reference to the viewing uniforms reflecting the current matrix stacks.
	[[nodiscard]] auto viewingUniforms () -> const ViewingUniforms&
	{
		if (!m_viewingUniforms.has_value()) {
			m_viewingUniforms = {
				modelviewMatrix(), invModelviewMatrix(), projectionMatrix(),
				invProjectionMatrix(), modelviewProjectionMatrix(),
				invModelviewProjectionMatrix(), normalMatrix(), invNormalMatrix()
			};
		}
		return m_viewingUniforms.value();
	}

	/// \brief All current viewing matrices as a uniform block, as last computed by the non-const overload.
	///
	/// \pre The current modelview matrix is affine and invertible.
	/// \pre The non-const overload has been called since the last change to the matrix stacks.
	///
	/// \return A reference to the viewing uniforms reflecting the current matrix stacks.
	///
	/// \throws std::bad_optional_access if the uniform block has not been computed.
	[[nodiscard]] auto viewingUniforms () const -> const ViewingUniforms& {
		return m_viewingUniforms.value();
	}


	////
	// Methods

	/// \brief Save the current modelview matrix by pushing a copy of it onto the modelview matrix stack.
	void pushModelviewMatrix () {
		modelview.push(modelview.top());
		invModelview.push(invModelview.top());
		normal.push(normal.top());
		invNormal.push(invNormal.top());
	}

	/// \brief Right-multiply the current modelview matrix by the given matrix, i.e. <tt>top = top * matrix</tt>.
	///
	/// \param matrix The matrix to multiply with.
	void mulModelviewMatrix (const glm::mat4& matrix) {
		modelview.top() *= matrix;
		onModelviewChange();
	}

	/// \brief Replace the current modelview matrix with the given matrix.
	///
	/// \param matrix The new modelview matrix.
	void loadModelviewMatrix (const glm::mat4& matrix) {
		modelview.top() = matrix;
		onModelviewChange();
	}

	/// \brief Restore the modelview matrix saved by the matching <tt>\ref pushModelviewMatrix</tt>.
	///
	/// \pre The modelview matrix stack holds more than one entry, i.e. every pop matches a preceding push.
	void popModelviewMatrix () {
		modelview.pop();
		invModelview.pop();
		normal.pop();
		invNormal.pop();
		invalidateDependentData();
	}

	/// \brief Save the current projection matrix by pushing a copy of it onto the projection matrix stack.
	void pushProjectionMatrix () {
		projection.push(projection.top());
		invProjection.push(invProjection.top());
	}

	/// \brief Right-multiply the current projection matrix by the given matrix, i.e. <tt>top = top * matrix</tt>.
	///
	/// \param matrix The matrix to multiply with.
	void mulProjectionMatrix (const glm::mat4& matrix) {
		projection.top() *= matrix;
		onProjectionChange();
	}

	/// \brief Replace the current projection matrix with the given matrix.
	///
	/// \param matrix The new projection matrix.
	void loadProjectionMatrix (const glm::mat4& matrix) {
		projection.top() = matrix;
		onProjectionChange();
	}

	/// \brief Restore the projection matrix saved by the matching <tt>\ref pushProjectionMatrix</tt>.
	///
	/// \pre The projection matrix stack holds more than one entry, i.e. every pop matches a preceding push.
	void popProjectionMatrix () {
		projection.pop();
		invProjection.pop();
		invalidateDependentData();
	}

	/// \brief
	/// 	Record pushing the current <tt>\ref viewingUniforms</tt> block to the given uniform slot of a shader stage.
	///
	/// Call this from \c Applet::render before issuing draw calls that use the viewing matrices. Computes any viewing
	/// matrices that are not yet available. The data is copied into the command buffer, so later changes to the matrix
	/// stacks do not affect draws already recorded. Failures, such as a null command buffer or an invalid slot, are
	/// reported to the SDL error log and nothing is pushed.
	///
	/// \pre The current modelview matrix is affine and invertible.
	///
	/// \param commandBuffer The command buffer to record the push into.
	/// \param stage The shader stage that receives the uniform block.
	/// \param slot The uniform slot of that stage, in the range 0–3.
	void pushViewingUniforms (SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, uint32_t slot);

	/// \brief
	/// 	Record pushing the <tt>\ref viewingUniforms</tt> block, as last computed by the non-const overloads, to the
	/// 	given uniform slot of a shader stage.
	///
	/// Failures, such as a null command buffer or an invalid slot, are reported to the SDL error log and nothing is
	/// pushed.
	///
	/// \pre The current modelview matrix is affine and invertible.
	/// \pre
	/// 	The non-const overload of <tt>\ref viewingUniforms</tt> or of this method has been called since the last
	/// 	change to the matrix stacks.
	///
	/// \param commandBuffer The command buffer to record the push into.
	/// \param stage The shader stage that receives the uniform block.
	/// \param slot The uniform slot of that stage, in the range 0–3.
	///
	/// \throws std::bad_optional_access if the uniform block has not been computed.
	void pushViewingUniforms (SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, uint32_t slot) const;


private:

	////
	// Methods

	/// \brief Reset the cached matrices that depend on both stacks, as well as the cached uniform block.
	inline void invalidateDependentData () {
		modelviewProjection.reset();
		invModelviewProjection.reset();
		m_viewingUniforms.reset();
	}

	/// \brief Reset all cached matrices derived from the top of the modelview stack after it changed.
	inline void onModelviewChange () {
		invModelview.top().reset();
		normal.top().reset();
		invNormal.top().reset();
		invalidateDependentData();
	}

	/// \brief Reset all cached matrices derived from the top of the projection stack after it changed.
	inline void onProjectionChange () {
		invProjection.top().reset();
		invalidateDependentData();
	}


	////
	// Fields

	/// Dimensions of the current viewport, independent of the matrix stacks.
	glm::uvec2 m_viewportSize{0};

	/// The modelview matrix stack.
	std::stack<glm::mat4> modelview{{glm::mat4(1.f)}};

	/// The projection matrix stack.
	std::stack<glm::mat4> projection{{glm::mat4(1.f)}};

	/// Cached inverse modelview matrices, one per modelview stack entry; empty when not yet computed.
	std::stack<std::optional<glm::mat4>> invModelview{{glm::mat4(1.f)}};

	/// Cached inverse projection matrices, one per projection stack entry; empty when not yet computed.
	std::stack<std::optional<glm::mat4>> invProjection{{glm::mat4(1.f)}};

	/// Cached modelview-projection matrix for the current stack tops; empty when not yet computed.
	std::optional<glm::mat4> modelviewProjection;

	/// Cached inverse modelview-projection matrix for the current stack tops; empty when not yet computed.
	std::optional<glm::mat4> invModelviewProjection;

	/// Cached normal matrices, one per modelview stack entry; empty when not yet computed.
	std::stack<std::optional<glm::mat3>> normal{{glm::mat3(1.f)}};

	/// Cached inverse normal matrices, one per modelview stack entry; empty when not yet computed.
	std::stack<std::optional<glm::mat3>> invNormal{{glm::mat3(1.f)}};

	/// Cached viewing uniform block for the current stack tops; empty when not yet computed.
	std::optional<ViewingUniforms> m_viewingUniforms;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif  // ifndef __FCG_RENDER_STATE_H__
