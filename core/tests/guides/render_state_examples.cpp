
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
// [includes]
#include <FCG/render_state.h>
// [includes]



//////
//
// Function implementations
//

/// \brief Load modelview and projection matrices and upload the viewing uniforms for a draw.
///
/// \pre The model matrix is affine and invertible.
///
/// \param device The device used for rendering.
/// \param commandBuffer The command buffer that receives the viewing uniforms.
/// \param model The affine, invertible model matrix that transforms object coordinates into world coordinates.
// [render_state_usage]
void example (fcg::Device &device, SDL_GPUCommandBuffer *commandBuffer, const glm::mat4 &model)
{
	fcg::RenderState renderState(device);
	renderState.loadProjectionMatrix(glm::perspective(glm::radians(60.f), 16.f / 9.f, 0.125f, 100.f));

	// Compose transforms by right-multiplication; model must be affine and invertible:
	renderState.loadModelviewMatrix(model);
	renderState.mulModelviewMatrix(glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f)));

	// Push the matrices for the vertex stage:
	renderState.pushViewingUniforms(commandBuffer, fcg::ShaderStage::VERTEX, 0);
}
// [render_state_usage]
