#include "DrawExample.h"

DrawExample::DrawExample()
	: ExampleBase()
{
}

DrawExample::~DrawExample()
{
}

int DrawExample::init(int argc, char** argv)
{
	// set up defaults and read command line arguments to override them
	vsg::CommandLine arguments(&argc, argv);
	auto debugLayer = arguments.value(false, { "--debug","-d" });
	auto apiDumpLayer = arguments.value(false, { "--api","-a" });
	_numFrames = arguments.value(-1, "-f");
	_printFrameRate = arguments.value(false, "--fr");
	auto numWindows = arguments.value(1, "--num-windows");
	auto[width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), { "--window", "-w" });
	if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

	// read shaders
	vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

	vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert.spv", searchPaths));
	vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag.spv", searchPaths));
	if (!vertexShader || !fragmentShader)
	{
		std::cout << "Could not create shaders." << std::endl;
		return 1;
	}

	// create the viewer and assign window(s) to it
	_viewer = vsg::Viewer::create();

	vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
	if (!window)
	{
		std::cout << "Could not create windows." << std::endl;
		return 1;
	}

	_viewer->addWindow(window);

	for (int i = 1; i<numWindows; ++i)
	{
		vsg::ref_ptr<vsg::Window> new_window(vsg::Window::create(width, height, debugLayer, apiDumpLayer, window));
		_viewer->addWindow(new_window);
	}

	// create high level Vulkan objects associated the main window
	vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
	vsg::ref_ptr<vsg::Device> device(window->device());
	vsg::ref_ptr<vsg::Surface> surface(window->surface());
	vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());


	VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
	VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
	if (!graphicsQueue || !presentQueue)
	{
		std::cout << "Required graphics/present queue not available!" << std::endl;
		return 1;
	}

	vsg::ref_ptr<vsg::CommandPool> commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());

	// set up vertex and index arrays
	vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array
		{
			{ -0.5f, -0.5f, 0.0f },
		{ 0.5f,  -0.5f, 0.05f },
		{ 0.5f , 0.5f, 0.0f },
		{ -0.5f, 0.5f, 0.0f },
		{ -0.5f, -0.5f, -0.5f },
		{ 0.5f,  -0.5f, -0.5f },
		{ 0.5f , 0.5f, -0.5 },
		{ -0.5f, 0.5f, -0.5 }
		});

	vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
		{
			{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f },
		});

	vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array
		{
			{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
		});

	vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
		{
			0, 1, 2,
			2, 3, 0,
			4, 5, 6,
			6, 7, 4
		});

	auto vertexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, vsg::DataList{ vertices, colors, texcoords }, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
	auto indexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, { indices }, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

	// set up uniforms
	_projMatrix = new vsg::mat4Value;
	_viewMatrix = new vsg::mat4Value;
	_modelMatrix = new vsg::mat4Value;

	_uniformBufferData = vsg::createHostVisibleBuffer(device, { _projMatrix, _viewMatrix, _modelMatrix }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

	//
	// set up texture image
	//
	vsg::ImageData imageData = osg2vsg::readImageFile(device, commandPool, graphicsQueue, vsg::findFile("textures/lz.rgb", searchPaths));
	if (!imageData.valid())
	{
		std::cout << "Texture not created" << std::endl;
		return 1;
	}

	//
	// set up descriptor layout and descriptor set and pipeline layout for uniforms
	//
	vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, 1,
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
		});

	vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
		{
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
		});

	vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
		{
			vsg::DescriptorBuffer::create(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _uniformBufferData),
			vsg::DescriptorImage::create(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{ imageData })
		});

	vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, { descriptorSetLayout }, {});

	// setup binding of descriptors
	vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vsg::DescriptorSets{ descriptorSet }); // device dependent


																																													   // set up graphics pipeline
	vsg::VertexInputState::Bindings vertexBindingsDescriptions
	{
		VkVertexInputBindingDescription{ 0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
		VkVertexInputBindingDescription{ 1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
		VkVertexInputBindingDescription{ 2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX }
	};

	vsg::VertexInputState::Attributes vertexAttributeDescriptions
	{
		VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		VkVertexInputAttributeDescription{ 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		VkVertexInputAttributeDescription{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 },
	};

	vsg::ref_ptr<vsg::ShaderStages> shaderStages = vsg::ShaderStages::create(vsg::ShaderModules
		{
			vsg::ShaderModule::create(device, vertexShader),
			vsg::ShaderModule::create(device, fragmentShader)
		});

	vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, // device dependent
		{
			shaderStages,  // device dependent
			vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
			vsg::InputAssemblyState::create(), // device independent
			vsg::ViewportState::create(VkExtent2D{ width, height }), // device independent
			vsg::RasterizationState::create(),// device independent
			vsg::MultisampleState::create(),// device independent
			vsg::ColorBlendState::create(),// device independent
			vsg::DepthStencilState::create()// device independent
		});

	vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

	// set up vertex buffer binding
	vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent

																												   // set up index buffer binding
	vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent

																																	  // set up drawing of the triangles
	vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent

																						   // set up what we want to render in a command graph
																						   // create command graph to contain all the Vulkan calls for specifically rendering the model
	vsg::ref_ptr<vsg::Group> commandGraph = vsg::Group::create();

	vsg::ref_ptr<vsg::StateGroup> stateGroup = vsg::StateGroup::create();
	commandGraph->addChild(stateGroup);

	// set up the state configuration
	stateGroup->add(bindPipeline);  // device dependent
	stateGroup->add(bindDescriptorSets);  // device dependent

										  // add subgraph that represents the model to render
	vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();
	stateGroup->addChild(model);

	// add the vertex and index buffer data
	model->add(bindVertexBuffers); // device dependent
	model->add(bindIndexBuffer); // device dependent

								 // add the draw primitive command
	model->addChild(drawIndexed); // device independent

								  //
								  // end of initialize vulkan
								  //
								  /////////////////////////////////////////////////////////////////////

	_startTime = std::chrono::steady_clock::now();

	for (auto& win : _viewer->windows())
	{
		// add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
		win->addStage(vsg::GraphicsStage::create(commandGraph));
		win->populateCommandBuffers();
	}
	return 0;
}

int DrawExample::frame()
{
	if (_viewer->done() || (_numFrames--) == 0) return 1;

	_viewer->pollEvents();

	float previousTime = _time;
	_time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now() - _startTime).count();
	if (_printFrameRate) std::cout << "time = " << _time << " fps=" << 1.0 / (_time - previousTime) << std::endl;

	// update
	(*_projMatrix) = vsg::perspective(vsg::radians(45.0f), float(_width) / float(_height), 0.1f, 10.f);
	(*_viewMatrix) = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));
	(*_modelMatrix) = vsg::rotate(_time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

	vsg::copyDataListToBuffers(_uniformBufferData);

	_viewer->submitFrame();

	return 0;
}
