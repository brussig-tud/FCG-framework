
//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>

// Dear ImGui
#include <imgui.h>

// GLM library
#include <glm/gtx/transform.hpp>

// SDL3 library
#include <SDL3/SDL.h>

// CMRC library for embedded app resources (shaders)
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(hello_res);

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet.h>
#include <FCG/applet/orbit_camera.h>

// Local includes
#include <shapes.h>



//////
//
// Classes
//

/// Convert a cmrc::file into a byte span for SDL GPU shader creation.
auto asBytes (const cmrc::file &file) -> std::span<const std::byte> {
	return {
		reinterpret_cast<const std::byte*>(file.begin()),
		file.size()
	};
}

/// Load a shader stage from the app's embedded resource filesystem.
auto loadAppShaderStage (const char *shaderName, const char *stageSuffix)
	-> std::optional<std::span<const std::byte>>
{
	const auto fs = cmrc::hello_res::get_filesystem();
	const std::string path = std::format("{}/{}.{}.spv", shaderName, shaderName, stageSuffix);
	if (!fs.is_file(path)) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "App shader stage not found: %s", path.c_str());
		return std::nullopt;
	}
	return asBytes(fs.open(path));
}


// Our demo applet.
class SimpleShapesApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Default constructor.
	SimpleShapesApplet()
		: shapes{
			std::make_unique<ConvexPolygon>()
		}
	{}

	/// The destructor. Releases the graphics pipeline created during \ref init.
	~SimpleShapesApplet() override {
		if (m_device != nullptr && m_pipeline != nullptr) {
			SDL_ReleaseGPUGraphicsPipeline(m_device->handle(), m_pipeline);
		}
	}


	////
	// Interface: fcg::Applet

	auto name () -> std::string& override {
		static std::string name = "Simple Shapes";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{
		// Cache the device so the destructor can release the pipeline.
		m_device = &device;

		// Load the embedded app shader stages.
		auto vertexSpirv = loadAppShaderStage("simple_shapes", "vert");
		auto fragmentSpirv = loadAppShaderStage("simple_shapes", "frag");
		if (!vertexSpirv || !fragmentSpirv) {
			return;
		}

		auto *vertexShader = device.createShader(fcg::ShaderStage::VERTEX, *vertexSpirv, 1);
		auto *fragmentShader = device.createShader(fcg::ShaderStage::FRAGMENT, *fragmentSpirv, 0);
		if (!vertexShader || !fragmentShader) {
			return;  // createShader already logged the error.
		}

		// Vertex layout: interleaved position + normal, both vec4.
		SDL_GPUVertexBufferDescription vertexBufferDesc {};
		vertexBufferDesc.slot = 0;
		vertexBufferDesc.pitch = sizeof(SimpleShape::Vertex);
		vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

		SDL_GPUVertexAttribute vertexAttributes[2] {};
		vertexAttributes[0].location = 0;
		vertexAttributes[0].buffer_slot = 0;
		vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		vertexAttributes[0].offset = offsetof(SimpleShape::Vertex, position);
		vertexAttributes[1].location = 1;
		vertexAttributes[1].buffer_slot = 0;
		vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		vertexAttributes[1].offset = offsetof(SimpleShape::Vertex, normal);

		SDL_GPUVertexInputState vertexInputState {};
		vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
		vertexInputState.num_vertex_buffers = 1;
		vertexInputState.vertex_attributes = vertexAttributes;
		vertexInputState.num_vertex_attributes = 2;

		// Depth-stencil state: match the framework's D32_FLOAT depth target.
		SDL_GPUDepthStencilState depthStencilState {};
		depthStencilState.enable_depth_test = true;
		depthStencilState.enable_depth_write = true;
		depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS;

		// Color target matching the swapchain format.
		SDL_GPUColorTargetDescription colorTarget {};
		colorTarget.format = player.swapchainFormat();

		SDL_GPUGraphicsPipelineTargetInfo targetInfo {};
		targetInfo.color_target_descriptions = &colorTarget;
		targetInfo.num_color_targets = 1;

		SDL_GPUGraphicsPipelineCreateInfo pipelineInfo {};
		pipelineInfo.vertex_shader = vertexShader;
		pipelineInfo.fragment_shader = fragmentShader;
		pipelineInfo.vertex_input_state = vertexInputState;
		pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.depth_stencil_state = depthStencilState;
		pipelineInfo.target_info = targetInfo;

		m_pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &pipelineInfo);
		if (!m_pipeline) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Creating the pipeline failed: %s", SDL_GetError());
		}

		// The pipeline has its own references to the shader objects now.
		SDL_ReleaseGPUShader(device.handle(), vertexShader);
		SDL_ReleaseGPUShader(device.handle(), fragmentShader);
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Simple Shapes");

		// Shape selection combo box.
		if (ImGui::BeginCombo("Shape", shapes[selectedShape]->name())) {
			for (unsigned i=0; i<(unsigned)SS::NUM; ++i) {
				const bool isSelected = (i == static_cast<std::size_t>(selectedShape));
				if (ImGui::Selectable(shapes[i]->name(), isSelected)) {
					selectedShape = static_cast<int>(i);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		// GUI for the currently selected shape's parameters.
		shapes[selectedShape]->gui();

		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player) override {
		// Make sure our shape is up-to-date and ready to render
		shapes[selectedShape]->update(device);
	}

	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {
		if (!m_pipeline) {
			return;
		}

		const auto &shape = *shapes[selectedShape];

		rs.pushModelviewMatrix();

		// Bind the pipeline and the shape's geometry buffers.
		SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);

		// Push the viewing uniforms so the shader can transform vertices.
		rs.pushViewingUniforms(commandBuffer, fcg::ShaderStage::VERTEX, 0);

		// Bind vertex buffer
		const SDL_GPUBufferBinding vertexBinding {
			.buffer = shape.vertexBuffer(),
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

		// Bind index buffer
		const SDL_GPUBufferBinding indexBinding {
			.buffer = shape.indexBuffer(),
			.offset = 0
		};
		SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		// Draw the indexed triangle list.
		SDL_DrawGPUIndexedPrimitives(renderPass, shape.numIndices(), 1, 0, 0, 0);
	}


protected:

	////
	// Fields

	/// All available simple shapes, instantiated once.
	std::unique_ptr<SimpleShape> shapes[(size_t)SS::NUM];

	/// Index of the currently selected shape in \ref shapes.
	int selectedShape = 0;

	/// Cached GPU device handle, captured during \ref init so the destructor can release the pipeline.
	fcg::Device *m_device = nullptr;

	/// The graphics pipeline used to render the simple shapes.
	SDL_GPUGraphicsPipeline *m_pipeline = nullptr;
};



//////
//
// Functions
//

/// Program entry point.
int main () {
	// Run with our demo applets
	return fcg::run<fcg::applet::OrbitCamera, SimpleShapesApplet>(fcg::PlayerSettings{.mainWindowTitle="Hello FCG!"});
}
