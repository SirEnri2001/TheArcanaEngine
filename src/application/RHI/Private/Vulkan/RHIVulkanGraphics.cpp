#include <array>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define RHI_IMPLEMENT
#include "RHIVulkan.h"
#include "RHIVulkanImpl.h"
#include "GLFW/glfw3.h"

void RHIVulkanPipelineFactory::SetUniformBinding(uint32_t Binding)
{
	VkDescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding = Binding;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	if(DescSetLayoutBindings.size()<=Binding)
	{
		DescSetLayoutBindings.resize(Binding + 1);
	}
	DescSetLayoutBindings[Binding] = uboLayoutBinding;
}

void RHIVulkanPipelineFactory::SetImageSamplerBinding(uint32_t Binding)
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = Binding;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	if(DescSetLayoutBindings.size()<=Binding)
	{
		DescSetLayoutBindings.resize(Binding + 1);
	}
	DescSetLayoutBindings[Binding] = samplerLayoutBinding;
}

void RHIVulkanPipelineFactory::RemoveAllGlobalBindings()
{
	DescSetLayoutBindings.clear();
}


void RHIVulkanPipelineFactory::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
	VertShaderBytecode = VertShader;
	FragShaderBytecode = FragShader;
}


void RHIVulkanPipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanRenderPassResource = static_cast<RHIVulkanRenderPass*>(RenderPassResource->GetImpl());
	auto* VulkanPipelineObject = static_cast<RHIVulkanPipelineObject*>(OutPipelineObject->GetImpl());
	if (!DescSetLayoutBindings.empty())
	{
		CreateDescriptorSetLayout(VulkanPipelineObject->DescriptorSetLayout, DescSetLayoutBindings, VulkanContext->Device);
	}
	else
	{
		VulkanPipelineObject->DescriptorSetLayout = nullptr;
	}

	//for (auto& BindingDesc : BindingDescMap)
	//{
	//	BindingDescriptions.push_back(BindingDesc.second);
	//}

	//for (auto& AttributeDesc : AttributeDescMap)
	//{
	//	AttributeDescriptions.push_back(AttributeDesc.second);
	//}

	CreateGraphicsPipeline(VulkanPipelineObject->Pipeline, VulkanPipelineObject->PipelineLayout, 
		VK_SAMPLE_COUNT_1_BIT, VulkanContext->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, VulkanPipelineObject->DescriptorSetLayout, VulkanRenderPassResource->RenderPass);
}


void RHIVulkanPipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanPresentPassResource = static_cast<RHIVulkanPresentPass*>(PresentPass->GetImpl());
	auto* VulkanPipelineObject = static_cast<RHIVulkanPipelineObject*>(OutPipelineObject->GetImpl());
	if (!DescSetLayoutBindings.empty())
	{
		DescSetLayoutBindings.erase(std::remove_if(DescSetLayoutBindings.begin(), DescSetLayoutBindings.end(), [](VkDescriptorSetLayoutBinding Desc){return Desc.descriptorCount==0;}), DescSetLayoutBindings.end());
		CreateDescriptorSetLayout(VulkanPipelineObject->DescriptorSetLayout, DescSetLayoutBindings, VulkanContext->Device);
		//CreateDescriptorPool(DescriptorPool, VulkanContext->Device);
		//CreateDescriptorSet(DescriptorSet, WriteDescriptorSets, VulkanContext->Device, DescriptorPool, DescriptorSetLayout);
	} 
	else
	{
		VulkanPipelineObject->DescriptorSetLayout = nullptr;
	}

	//for (auto& BindingDesc : BindingDescMap)
	//{
	//	BindingDescriptions.push_back(BindingDesc.second);
	//}

	//for (auto& AttributeDesc : AttributeDescMap)
	//{
	//	AttributeDescriptions.push_back(AttributeDesc.second);
	//}

	CreateGraphicsPipeline(VulkanPipelineObject->Pipeline, VulkanPipelineObject->PipelineLayout, 
		VulkanPresentPassResource->msaaSamples, VulkanContext->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, VulkanPipelineObject->DescriptorSetLayout, VulkanPresentPassResource->RenderPass);
}


void RHIVulkanPipelineFactory::Cleanup(RHIContext* Context)
{
}

void RHIVulkanPipelineFactory::AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
	//AttributeDescMap[{BindingIndex, Location}] = { Location, BindingIndex, RHIVulkanPlatformSupport::GetVkFormat(Format), Offset };
	AttributeDescriptions.push_back({ Location, BindingIndex, RHIVulkanPlatformSupport::GetVkFormat(Format), Offset });
}

void RHIVulkanPipelineFactory::AddBufferBinding(uint32_t BindingIndex, uint32_t Stride)
{
	//BindingDescMap[BindingIndex] = { BindingIndex, Stride, VK_VERTEX_INPUT_RATE_VERTEX };
	BindingDescriptions.push_back({ BindingIndex, Stride, VK_VERTEX_INPUT_RATE_VERTEX });
}

void RHIVulkanPipelineFactory::RemoveAllBufferBindings()
{
	BindingDescriptions.clear();
	AttributeDescriptions.clear();
}

void RHIVulkanRenderPass::Initialize(RHIContext* Context, uint32_t Width, uint32_t Height)
{
	// Initialize: std::vector<VkAttachmentDescription> & VkRenderPass & VkFramebuffer
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Extent.height = Height;
	Extent.width = Width;
	std::vector<VkImageView> ImageViews;
	for(size_t i = 0; i < ColorRenderTargets.size(); i++)
	{
		auto* VkImageResource = ColorRenderTargets[i];
		VkAttachmentDescription AttachmentDesc{};
		AttachmentDesc.format = VkImageResource->InnerFormat;
		AttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		AttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		AttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		AttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		AttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Attachments.emplace_back(
			AttachmentDesc
		);
		ImageViews.push_back(VkImageResource->ImageView);
	}
	if(DepthRenderTargets!=nullptr)
	{
		VkAttachmentDescription AttachmentDesc{};
		AttachmentDesc.format = DepthRenderTargets->InnerFormat;
		AttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		AttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		AttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		AttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		Attachments.emplace_back(
			AttachmentDesc
		);
		DepthAttachmentIndex = ColorRenderTargets.size();
		ImageViews.push_back(DepthRenderTargets->ImageView);
	}
	CreateRenderPassSingleSubpass(RenderPass, VulkanContext->Device, Attachments, DepthAttachmentIndex);
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = RenderPass;
	framebufferInfo.attachmentCount = static_cast<uint32_t>(ImageViews.size());
	framebufferInfo.pAttachments = ImageViews.data();
	framebufferInfo.width = Width;
	framebufferInfo.height = Height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &FrameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	}
}

void RHIVulkanRenderPass::Cleanup(RHIContext* Context)
{

}

void RHIVulkanRenderPass::AddColorRenderTarget(RHIImageResource* ColorRT)
{
	ColorRenderTargets.push_back(static_cast<RHIVulkanImageResource*>(ColorRT->GetImpl()));
}

void RHIVulkanRenderPass::SetDepthRenderTarget(RHIImageResource* DepthRT)
{
	DepthRenderTargets = static_cast<RHIVulkanImageResource*>(DepthRT->GetImpl());
}


// RHIVulkanPresentPass implementation
RHIVulkanPresentPass::RHIVulkanPresentPass()
{
    // Initialize any necessary Vulkan-specific resources here
}

RHIVulkanPresentPass::~RHIVulkanPresentPass()
{
    // Cleanup any Vulkan-specific resources here
}

void RHIVulkanPresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* InColorRT, RHIImageResource* InDepthRT)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	ColorRT = static_cast<RHIVulkanImageResource*>(InColorRT->GetImpl());
	DepthRT = static_cast<RHIVulkanImageResource*>(InDepthRT->GetImpl());
	msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	CreatePresentableRenderPass(RenderPass, VulkanContext->Device, DepthRT!=nullptr, RHIVulkanPlatformSupport::Get()->GetDepthFormat(), VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat, msaaSamples);
}

void RHIVulkanPresentPass::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyRenderPass(VulkanContext->Device, RenderPass, nullptr);
}

void RHIVulkanPresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	WindowManager->RecreateSwapchain(Context);
	ColorRT->Cleanup(Context);
	DepthRT->Cleanup(Context);
	ImageExtent3D RenderTargetExtent;
	RenderTargetExtent.Height = WindowManager->GetWindowHeight();
	RenderTargetExtent.Width = WindowManager->GetWindowWidth();
	RenderTargetExtent.Depth = 1;
	ColorRT->InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_COLOR_RT, msaaSamples);
	DepthRT->InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_DEPTH_RT, msaaSamples);
}

void RHIVulkanGraphicDispatcher::Initialize(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
	if (vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &RenderFinishSemaphore) != VK_SUCCESS ||
		vkCreateFence(VulkanContext->Device, &fenceInfo, nullptr, &InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
}

void RHIVulkanGraphicDispatcher::Cleanup(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	for (int i = 0; i < VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size(); i++)
	{
		vkDestroySemaphore(VulkanContext->Device, ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(VulkanContext->Device, RenderFinishSemaphore, nullptr);
		vkDestroyFence(VulkanContext->Device, InFlightFence, nullptr);
	}
}

void RHIVulkanGraphicDispatcher::BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex)
{
	auto* VulkanBufferResource = static_cast<RHIVulkanBufferResource*>(BufferResource->GetImpl());
	BindingInfo Info;
	Info.BindingIndex = BindingIndex;
	Info.BufferResource = VulkanBufferResource;
	Info.Offset = Offset;
	BindingInfos.push_back(Info);
}

void RHIVulkanGraphicDispatcher::BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset)
{
	auto* VulkanBufferResource = static_cast<RHIVulkanBufferResource*>(BufferResource->GetImpl());
	IndexBindingInfo.BufferResource = VulkanBufferResource;
	IndexBindingInfo.Offset = Offset;
}

void RHIVulkanGraphicDispatcher::Dispatch(RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	auto* VulkanPipeline = static_cast<RHIVulkanPipelineObject*>(PipelineObject->GetImpl());
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->Pipeline);

	for (auto& Info : BindingInfos)
	{
		vkCmdBindVertexBuffers(CommandBuffer, Info.BindingIndex, 1, &Info.BufferResource->Buffer, &Info.Offset);
	}
	BindingInfos.clear();
	vkCmdBindIndexBuffer(CommandBuffer, IndexBindingInfo.BufferResource->Buffer, IndexBindingInfo.Offset, VK_INDEX_TYPE_UINT32);
	if (VulkanPipeline->WriteDescriptorSets.size()>0)
	{
		vkCmdPushDescriptorSetKHR(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout,
			0, VulkanPipeline->WriteDescriptorSets.size(), VulkanPipeline->WriteDescriptorSets.data());
	}
	
	//if (VulkanPipeline->DescriptorSet)
	//{
	//	
	//	//vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout, 0, 1,
	//	//	&VulkanPipeline->DescriptorSet, 0, nullptr);
	//}
	
	vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, IndexOffset, 0, 0);
}

void RHIVulkanGraphicDispatcher::BeginRenderPass(RHIRenderPass* RenderPass)
{
	auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VulkanRenderPass->RenderPass;
	renderPassInfo.framebuffer = VulkanRenderPass->FrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VulkanRenderPass->Extent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {1.0f, 0.0f, 1.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)VulkanRenderPass->Extent.width;
	viewport.height = (float)VulkanRenderPass->Extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = VulkanRenderPass->Extent;
	vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);
}

void RHIVulkanGraphicDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
	auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());
	vkCmdEndRenderPass(CommandBuffer);
}

void RHIVulkanGraphicDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkWaitForFences(VulkanContext->Device, 1, &InFlightFence, VK_TRUE, UINT64_MAX);
}


void RHIVulkanGraphicDispatcher::EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &RenderFinishSemaphore;

	if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

	if (vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &RenderFinishSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &VulkanWindowManager->CurrentSwapchain.Swapchain;
	presentInfo.pImageIndices = &CurrentImageIndex;

	auto result = vkQueuePresentKHR(VulkanContext->PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		bWindowResizeLastframe = true;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}


void RHIVulkanGraphicDispatcher::BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto* VulkanPresentPass = static_cast<RHIVulkanRenderPass*>(PresentRenderPass->GetImpl());

	glfwPollEvents();
	WaitForGPUIdle(Context);
	if (bWindowResizeLastframe)
	{
		//PresentPassResource->OnWindowResize(Context, WindowManager);
		bWindowResizeLastframe = false;
	}
	VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain,
		UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);

	while (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {

		vkResetFences(VulkanContext->Device, 1, &InFlightFence);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		if(result==VK_SUBOPTIMAL_KHR)
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
		}
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;

		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		if (vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, InFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VulkanWindowManager->RecreateSwapchain(Context);
		bWindowResizeLastframe = false;

		result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain,
			UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);
	}

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	VulkanPresentPass->FrameBuffer = VulkanWindowManager->CurrentSwapchain.SwapchainFramebuffers[CurrentImageIndex];
	VulkanPresentPass->Extent.height = VulkanWindowManager->GetWindowHeight();
	VulkanPresentPass->Extent.width = VulkanWindowManager->GetWindowWidth();

	vkResetFences(VulkanContext->Device, 1, &InFlightFence);

	vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}


void RHIVulkanImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
	auto VulkanPlatform = static_cast<RHIVulkanPlatformSupport*>(RHIPlatformSupport::Get()->GetImpl());
	auto vkWindow = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto vkContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto vkPresentPass = static_cast<RHIVulkanRenderPass*>(PresentRenderPass->GetImpl());
	
    ImGlobals.Context = ImGui::CreateContext();
	ImGui::GetAllocatorFunctions(&ImGlobals.MemAllocFunc, &ImGlobals.MemFreeFunc, &ImGlobals.UserData);
	ImGui_ImplVulkan_InitInfo ImGuiInitInfo{};
	ImGuiInitInfo.Instance = VulkanPlatform->Instance;
	ImGuiInitInfo.PhysicalDevice = VulkanPlatform->CurrentPhysicalDevice.PhysicalDevice;
	ImGuiInitInfo.Device = vkContext->Device;
	ImGuiInitInfo.QueueFamily = VulkanPlatform->CurrentPhysicalDevice.GraphicsQueueFamilyIndex;
	ImGuiInitInfo.Queue = vkContext->GraphicsQueue;
	ImGuiInitInfo.DescriptorPool = nullptr;
	ImGuiInitInfo.DescriptorPoolSize = 32;
	ImGuiInitInfo.RenderPass = vkPresentPass->RenderPass;
	ImGuiInitInfo.MinImageCount = 2;
	ImGuiInitInfo.ImageCount = vkWindow->CurrentSwapchain.SwapchainImageViews.size();
	ImGuiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&ImGuiInitInfo);
	ImGui_ImplGlfw_InitForVulkan(vkWindow->pGLFWwindow, true);
}

void RHIVulkanImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context))
{
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
	pFuncDrawUI(&ImGlobals);
    // Rendering
    ImGui::Render();
}


void RHIVulkanImGUI::Cleanup()
{
	
}

void RHIVulkanImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
	auto* VkDispatcher = reinterpret_cast<RHIVulkanGraphicDispatcher*>(Dispatcher->GetImpl());
	ImGui_ImplVulkan_RenderDrawData(draw_data, VkDispatcher->CommandBuffer, nullptr);
}

void RHIVulkanPipelineObject::SetUniform(RHIUniform* Uniform, uint32_t Binding)
{
	auto* VulkanUniform = static_cast<RHIVulkanUniform*>(Uniform->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pBufferInfo = &VulkanUniform->DescriptorBufferInfo;

	if(WriteDescriptorSets.size()<=Binding)
	{
		WriteDescriptorSets.resize(Binding+1);
	}
	WriteDescriptorSets[Binding] = WriteDescSet;
}

void RHIVulkanPipelineObject::SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding)
{
	auto* VulkanImageResource = static_cast<RHIVulkanImageResource*>(ImageResource->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pImageInfo = &VulkanImageResource->DescriptorInfo;

	if(WriteDescriptorSets.size()<=Binding)
	{
		WriteDescriptorSets.resize(Binding+1);
	}
	WriteDescriptorSets[Binding] = WriteDescSet;
}

void RHIVulkanPipelineObject::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyPipeline(VulkanContext->Device, Pipeline, nullptr);
	vkDestroyPipelineLayout(VulkanContext->Device, PipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(VulkanContext->Device, DescriptorSetLayout, nullptr);
}

