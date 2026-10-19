
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/device.h>
#include <FCG/render_state.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Assertions remain enabled in Release builds.
void require (bool value, const char *message) {
	if (!value)
		throw std::runtime_error(message);
}

/// Compare matrix elements with a tolerance scaled to the expected magnitude.
template <glm::length_t Columns, glm::length_t Rows>
void matrixNear (
	const glm::mat<Columns, Rows, float> &actual, const glm::mat<Columns, Rows, float> &expected, const char *message
){
	for (glm::length_t column = 0; column < Columns; ++column)
		for (glm::length_t row = 0; row < Rows; ++row)
			require(std::abs(actual[column][row] - expected[column][row])
				<= 2e-5f * std::max(1.f, std::abs(expected[column][row])), message);
}

/// Check the three padded uniform columns against their unpadded matrix.
void matrixNear (const fcg::Std140Mat3 &actual, const glm::mat3 &expected, const char *message)
{
	matrixNear(
		glm::mat3(glm::vec3(actual.columns[0]), glm::vec3(actual.columns[1]), glm::vec3(actual.columns[2])),
		expected, message
	);
	for (const auto &column : actual.columns)
		require(column.w == 0.f, "Normal uniform padding is not zero");
}

/// Check matrix values, normal geometry, and both mutable and const access to the complete uniform block.
void matrices (fcg::RenderState &state, const glm::mat4 &modelview, const glm::mat4 &projection)
{
	const auto linear = glm::mat3(modelview);
	const auto expectedNormal = glm::transpose(glm::inverse(linear));
	const auto expectedInvNormal = glm::transpose(linear);
	const auto normal = state.normalMatrix();
	const auto invNormal = state.invNormalMatrix();
	matrixNear(state.modelviewMatrix(), modelview, "Modelview matrix differs");
	matrixNear(state.projectionMatrix(), projection, "Projection matrix differs");
	matrixNear(normal, expectedNormal, "Normal matrix differs from the linear inverse transpose");
	matrixNear(invNormal, expectedInvNormal, "Inverse normal matrix differs from the linear transpose");
	matrixNear(normal * invNormal, glm::mat3(1.f), "Normal and inverse normal disagree");
	const glm::vec3 tangent{1.f, 2.f, -1.f};
	const glm::vec3 bitangent{-2.f, 1.f, 3.f};
	const auto transformedNormal = glm::normalize(normal * glm::cross(tangent, bitangent));
	for (const auto &direction : {tangent, bitangent})
		require(std::abs(glm::dot(transformedNormal, glm::normalize(linear * direction))) <= 2e-5f,
			"Transformed normal is not perpendicular to a transformed tangent");
	const auto &uniforms = state.viewingUniforms();
	matrixNear(uniforms.modelview, modelview, "Modelview uniform is stale");
	matrixNear(uniforms.invModelview, glm::inverse(modelview), "Inverse modelview uniform is stale");
	matrixNear(uniforms.projection, projection, "Projection uniform is stale");
	matrixNear(uniforms.invProjection, glm::inverse(projection), "Inverse projection uniform is stale");
	matrixNear(uniforms.modelviewProjection, projection * modelview, "Combined uniform is stale");
	matrixNear(uniforms.invModelviewProjection, glm::inverse(projection * modelview), "Inverse combined uniform is stale");
	matrixNear(uniforms.normal, expectedNormal, "Normal uniform is stale");
	matrixNear(uniforms.invNormal, expectedInvNormal, "Inverse normal uniform is stale");
	const auto &cached = std::as_const(state);
	matrixNear(cached.normalMatrix(), expectedNormal, "Const normal matrix differs");
	matrixNear(cached.invNormalMatrix(), expectedInvNormal, "Const inverse normal matrix differs");
	matrixNear(cached.invModelviewMatrix(), glm::inverse(modelview), "Const inverse modelview differs");
	require(&cached.viewingUniforms() == &uniforms, "Const and mutable access do not share the uniform block");
}

/// Derived render-state matrices are computed on demand, independent of query order.
void renderState (fcg::Device &device)
{
	fcg::RenderState state(device);
	glm::mat4 modelview(1.f);
	modelview[0][0] = 2.f;
	modelview[1][1] = 3.f;
	modelview[2][2] = 4.f;
	modelview[3] = {1.f, 2.f, 3.f, 1.f};
	state.loadModelviewMatrix(modelview);
	// Query the inverse normal matrix first, without having queried the normal matrix before.
	const glm::mat3 invNormal = state.invNormalMatrix();
	const glm::mat3 expected(glm::vec3{2.f, 0.f, 0.f}, glm::vec3{0.f, 3.f, 0.f}, glm::vec3{0.f, 0.f, 4.f});
	for (int col = 0; col < 3; ++col)
		for (int row = 0; row < 3; ++row)
			require(std::abs(invNormal[col][row] - expected[col][row]) < 1e-5f,
			        "Inverse normal matrix is incorrect");
	require(state.normalMatrix()[0][0] == 0.5f, "Normal matrix is incorrect");
}

/// Exercise affine normal transforms and lazy inverse availability in each accessor order.
void affineQueries (fcg::Device &device)
{
	const glm::mat4 identity(1.f);
	const auto translation = glm::translate(identity, {3.f, -4.f, 5.f});
	const auto rotation = glm::rotate(identity, glm::radians(37.f), glm::normalize(glm::vec3{1.f, 2.f, -3.f}));
	const auto scale = glm::scale(identity, {2.f, 3.f, .5f});
	const auto reflection = glm::scale(identity, {-1.f, 1.f, 1.f});
	glm::mat4 shear(1.f);
	shear[1][0] = .4f;
	shear[2][1] = -.25f;
	const std::array transforms{identity, translation, rotation, scale, shear, reflection,
		translation * rotation * shear * scale * reflection};
	const auto projection = glm::perspective(glm::radians(55.f), 1.5f, .25f, 40.f);
	for (const auto &modelview : transforms)
		for (int order = 0; order < 4; ++order)
		{
			fcg::RenderState state(device);
			state.loadModelviewMatrix(modelview);
			state.loadProjectionMatrix(projection);
			switch (order) {
			case 0: (void)state.normalMatrix(); break;
			case 1: (void)state.invModelviewMatrix(); (void)state.normalMatrix(); break;
			case 2: (void)state.invNormalMatrix(); break;
			case 3: (void)state.viewingUniforms(); break;
			}
			matrixNear(
				std::as_const(state).invModelviewMatrix(), glm::inverse(modelview),
				"Normal queries did not populate the inverse modelview cache"
			);
			matrices(state, modelview, projection);
		}
}

/// Recompute changed entries, restore saved matrices, and retain normals across projection changes.
void matrixStacks (fcg::Device &device)
{
	fcg::RenderState state(device);
	const glm::mat4 identity(1.f);
	matrices(state, identity, identity);
	const auto modelview = glm::translate(identity, {1.f, 2.f, -3.f}) * glm::scale(identity, {2.f, 3.f, 4.f});
	state.loadModelviewMatrix(modelview);
	matrices(state, modelview, identity);
	state.pushModelviewMatrix();
	const auto rotation = glm::rotate(identity, glm::radians(25.f), glm::vec3{0.f, 1.f, 0.f});
	state.mulModelviewMatrix(rotation);
	matrices(state, modelview * rotation, identity);
	state.loadModelviewMatrix(rotation);
	matrices(state, rotation, identity);
	state.popModelviewMatrix();
	matrices(state, modelview, identity);
	const auto savedNormal = state.normalMatrix();
	const auto savedInvNormal = state.invNormalMatrix();
	const auto projection = glm::perspective(glm::radians(70.f), 1.75f, .125f, 100.f);
	state.loadProjectionMatrix(projection);
	matrixNear(std::as_const(state).normalMatrix(), savedNormal, "Projection load invalidated the normal matrix");
	matrixNear(std::as_const(state).invNormalMatrix(), savedInvNormal, "Projection load invalidated the inverse normal");
	matrices(state, modelview, projection);
	state.pushProjectionMatrix();
	const auto offset = glm::translate(identity, {.1f, -.2f, 0.f});
	state.mulProjectionMatrix(offset);
	matrixNear(std::as_const(state).normalMatrix(), savedNormal, "Projection multiplication invalidated the normal matrix");
	matrices(state, modelview, projection * offset);
	state.popProjectionMatrix();
	matrixNear(std::as_const(state).normalMatrix(), savedNormal, "Projection pop invalidated the normal matrix");
	matrices(state, modelview, projection);
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Check render-state matrices with a real device and report validation diagnostics through CTest.
auto main () -> int
{
	try {
		require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
		/* perform tests with GPU device */ {
			auto device = fcg::Device::create();
			require(device.has_value(), SDL_GetError());
			renderState(*device);
			affineQueries(*device);
			matrixStacks(*device);
		}
		SDL_Quit();
		std::cout << "Render-state matrix checks passed\n";
		return 0;
	}
	catch (const std::exception &failure) {
		std::cerr << failure.what() << '\n';
		SDL_Quit();
		return 1;
	}
}
