#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <set>

namespace Files = std::filesystem;

#include "types.hpp"

template<typename T>
using Array = std::vector<T>;

using String = std::string;

template<typename T>
T clamp(T a, T b, T c)
{
    if(a < b){
        return b;
    }
    if(a > c){
        return c;
    }
    return a;
}

template<typename A, typename B>
struct Pair
{
    A first;
    B second;
};

template<typename T, size_t S>
constexpr size_t array_size(const T(&)[S])
{
    return S;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback( VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData )
{
    printf("%s", pCallbackData->pMessage);
    return false;
}

int main(int args, const char** argc)
{

    const auto WIDTH  {1280};
    const auto HEIGHT {720};

	SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);

	auto window {SDL_CreateWindow("test",
		                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		                          WIDTH, HEIGHT,
		                          SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_RESIZABLE)};

    Array<VkLayerProperties> layers;

    Array<char*> enabled_layers;

    VkResult vkr;
    u32 count;

    {
        vkr = vkEnumerateInstanceLayerProperties(&count, nullptr);
        layers.resize(count);
        vkr = vkEnumerateInstanceLayerProperties(&count, layers.data());
        if(vkr != VK_SUCCESS){
            printf("bik problem %i", __LINE__);
        }
    
    }

    for(auto& l : layers){
        enabled_layers.push_back(l.layerName);
    }

    SDL_bool res;

    Array<const char*> extensions;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    extensions.resize(count);

    res = SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());

    if(!res){
        printf("%s\n", SDL_GetError());
    }

    const auto app_name    {"vulkan test"};
    const auto engine_name {"deez nuts"};

    VkApplicationInfo app_info {};

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = 0;
    app_info.pEngineName = engine_name;
    app_info.engineVersion = 0;
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo info {};

    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //info.pNext = &debug_info;
    info.pApplicationInfo = &app_info;
    info.enabledLayerCount = enabled_layers.size();
    info.ppEnabledLayerNames = enabled_layers.data();
    info.enabledExtensionCount = extensions.size();
    info.ppEnabledExtensionNames = extensions.data();

    VkInstance context;
    vkr = vkCreateInstance(&info, nullptr, &context);

    VkDebugUtilsMessengerEXT messenger;

    {
        VkDebugUtilsMessengerCreateInfoEXT debug_info
        {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = vk_debug_callback,
        };

        auto func {(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context, "vkCreateDebugUtilsMessengerEXT")};
        if(func != nullptr){
            func(context, &debug_info, nullptr, &messenger);
        }
        else{
            printf("bik problem %i\n", __LINE__);
        }
    }

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    if(!res){
        printf("%s\n", SDL_GetError());
    }

    VkSurfaceKHR vk_surface;

    res  = SDL_Vulkan_CreateSurface(window, context, &vk_surface);

    if(!res){
        printf("%s\n", SDL_GetError());
    }

    struct GPU
    {
        VkPhysicalDevice gpu;
        VkPhysicalDeviceFeatures features;
        Array<VkQueueFamilyProperties> queue;
        Array<VkExtensionProperties> extensions;

        struct Swapchain_Details
        {
            VkSurfaceCapabilitiesKHR capabilities {};
            Array<VkSurfaceFormatKHR> formats;
            Array<VkPresentModeKHR> present_modes;
        };

        Swapchain_Details swapchain;
    };

    Array<GPU> gpus;
    {
        Array<VkPhysicalDevice> _gpus;

        vkEnumeratePhysicalDevices(context, &count, nullptr);
        _gpus.resize(count); 
        vkr = vkEnumeratePhysicalDevices(context, &count, _gpus.data());
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }
        gpus.resize(count);

        for(int i = 0; i < _gpus.size(); i++){
            gpus[i].gpu = _gpus[i];
        }
    }

    for(auto& g : gpus)
    {
        vkGetPhysicalDeviceFeatures(g.gpu, &g.features);

        vkGetPhysicalDeviceQueueFamilyProperties(g.gpu, &count, nullptr);
        g.queue.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(g.gpu, &count, g.queue.data());
        vkEnumerateDeviceExtensionProperties(g.gpu, nullptr, &count, nullptr);
        g.extensions.resize(count);
        vkEnumerateDeviceExtensionProperties(g.gpu, nullptr, &count, g.extensions.data());
    }

    // TODO add a way to get the best gpu

    const char* device_required_extensions[] {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    Array<Pair<GPU*, Pair<u32, u32>>> possible_gpus;
    for(auto& g : gpus)
    {
        
        auto skip {false};
        for(auto e : device_required_extensions)
        {
            auto found {false};
            for(auto& ge : g.extensions)
            {
                if(String{e} == String{ge.extensionName})
                {
                    found = true;
                    break;
                }
            }
            if(!found)
            {
                skip = true;
                break;
            }
        }
        if(skip){
            continue;
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g.gpu, vk_surface, &g.swapchain.capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(g.gpu, vk_surface, &count, nullptr);
        assert(count != 0);
        g.swapchain.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g.gpu, vk_surface, &count, g.swapchain.formats.data());

        vkGetPhysicalDeviceSurfacePresentModesKHR(g.gpu, vk_surface, &count, nullptr);
        assert(count != 0);
        g.swapchain.present_modes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(g.gpu, vk_surface, &count, g.swapchain.present_modes.data());


        u32 queue_index {0};
        auto& family {g.queue};
        for(int i = 0; i < family.size(); i++)
        {
            auto& q {family[i]};
            if(q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                possible_gpus.push_back({&g, {queue_index}});
                break;
            }
            queue_index++;
        }
    }

    GPU* gpu {nullptr};
    u32 queue_index {0};
    u32 present_index {0};
    for(auto& p : possible_gpus)
    {
        auto i {0};
        for(auto& f : p.first->queue)
        {
            VkBool32 res;
            vkr = vkGetPhysicalDeviceSurfaceSupportKHR(p.first->gpu, i, vk_surface, &res);

            if(res == VK_TRUE)
            {
                present_index = i;
                p.second.second = i;
                queue_index = p.second.first;
                gpu = p.first;
                break;
            }
            i++;
        }
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }
        if(gpu){
            break;
        }
    }

    assert(gpu);

    float queue_priority {1.f};

    Array<VkDeviceQueueCreateInfo> queue_infos {};

    {
        std::set queues {queue_index, present_index};

        auto ctr {0};
        for(auto i : queues)
        {
            VkDeviceQueueCreateInfo queue_info {};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = i;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_infos.push_back(queue_info);
            ctr++;
        }

    }
    VkDeviceCreateInfo device_info {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.queueCreateInfoCount = queue_infos.size();
    device_info.enabledExtensionCount = array_size(device_required_extensions);
    device_info.ppEnabledExtensionNames = device_required_extensions;
    device_info.pEnabledFeatures = &gpu->features;

    VkDevice device;

    vkr = vkCreateDevice(gpu->gpu, &device_info, nullptr, &device);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    VkQueue device_queue;
    VkQueue present_queue;

    vkGetDeviceQueue(device, queue_index, 0, &device_queue);
    vkGetDeviceQueue(device, present_index, 0, &present_queue);

    auto surface_format {gpu->swapchain.formats[0]};

    for(auto f : gpu->swapchain.formats)
    {
        if(f.format == VK_FORMAT_R8G8B8A8_SRGB)
        {
            surface_format = f;
            break;
        }
    }

    auto present_mode {VK_PRESENT_MODE_FIFO_KHR};

    for(auto i : gpu->swapchain.present_modes)
    {
        if(i == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            present_mode = i;
            break;
        }
    }

    VkExtent2D vk_extent {gpu->swapchain.capabilities.currentExtent};
    if(vk_extent.width == UINT32_MAX)
    {
        vk_extent = {WIDTH, HEIGHT};
        vk_extent.width = clamp(vk_extent.width, gpu->swapchain.capabilities.minImageExtent.width, gpu->swapchain.capabilities.maxImageExtent.width);

        vk_extent.height = clamp(vk_extent.height, gpu->swapchain.capabilities.minImageExtent.height, gpu->swapchain.capabilities.maxImageExtent.height);
    }

    VkSwapchainCreateInfoKHR swapchain_info {};

    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = vk_surface;

    swapchain_info.minImageCount = gpu->swapchain.capabilities.minImageCount + 1;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = vk_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const u32 swapchain_family_indices[] {queue_index, present_index};

    if(queue_index != present_index)
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = array_size(swapchain_family_indices);
        swapchain_info.pQueueFamilyIndices = swapchain_family_indices;
    }
    else{
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    swapchain_info.preTransform = gpu->swapchain.capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    vkr = vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    Array<VkImage> imgs;
    auto create_images {[&]()
    {
        count = gpu->swapchain.capabilities.minImageCount + 1;
        vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
        imgs.resize(count);
        vkGetSwapchainImagesKHR(device, swapchain, &count, imgs.data());

        Array<VkImageView> result(imgs.size());
        auto ctr {0};
        for(auto i : imgs)
        {
            VkImageViewCreateInfo image_view_create_info {};
            image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.image = i;
            image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_view_create_info.format = surface_format.format;
            image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;

            vkr = vkCreateImageView(device, &image_view_create_info, nullptr, &result[ctr]);

            if(vkr != VK_SUCCESS){
                printf("bik problem %i\n", __LINE__);
            }
            ctr++;
        }
        return result;
    }};

    auto image_views {create_images()};

    String vertex_shader;
    String frag_shader;

    {
        vertex_shader.resize(Files::file_size("shader.vert.spv"));
        std::ifstream file {"shader.vert.spv", file.binary};
        file.seekg(0);
        file.read(vertex_shader.data(), vertex_shader.length());

        frag_shader.resize(Files::file_size("shader.frag.spv"));
        file = std::ifstream {"shader.frag.spv", file.binary};
        file.seekg(0);
        file.read(frag_shader.data(), frag_shader.length());
    }

    VkPipelineShaderStageCreateInfo shader_stage_infos[2] {};

    {

        VkShaderModule vertex_module;
        VkShaderModule frag_module;

        VkShaderModuleCreateInfo shader_info {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.codeSize = vertex_shader.size();
        shader_info.pCode = (const u32*)(vertex_shader.data());
        vkr = vkCreateShaderModule(device, &shader_info, nullptr, &vertex_module);
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        shader_info.codeSize = frag_shader.size();
        shader_info.pCode = (const u32*)(frag_shader.data());
        vkr = vkCreateShaderModule(device, &shader_info, nullptr, &frag_module);
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        shader_stage_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

        shader_stage_infos[0].module = vertex_module;
        shader_stage_infos[0].pName = "main";

        shader_stage_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        shader_stage_infos[1].module = frag_module;
        shader_stage_infos[1].pName = "main";
    }

    VkPipelineMultisampleStateCreateInfo multisample
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = vk_extent.width;
    viewport.height = vk_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = vk_extent;

    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineColorBlendAttachmentState color_blend {};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend;

    VkPipelineLayout pipeline_layout;

    VkPipelineLayoutCreateInfo pipeline_layout_info {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    vkr = vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    VkAttachmentDescription color_attachment {};
    color_attachment.format = surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPass render_pass;

    VkRenderPassCreateInfo render_pass_info {};

    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    vkr = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    VkDynamicState dynamic_states[] {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = array_size(dynamic_states),
        .pDynamicStates = dynamic_states
    };

    VkPipelineViewportStateCreateInfo viewport_state
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stage_infos,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline pipeline;

    vkr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    auto create_frame_buffers {[&]()
    {
        Array<VkFramebuffer> result(image_views.size());
        for(int i = 0; i < result.size(); i++)
        {
            VkFramebufferCreateInfo frame_buffer_info {};
            frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frame_buffer_info.renderPass = render_pass;
            frame_buffer_info.attachmentCount = 1;
            frame_buffer_info.pAttachments = &image_views[i];
            frame_buffer_info.width = vk_extent.width;
            frame_buffer_info.height = vk_extent.height;
            frame_buffer_info.layers = 1;

            vkr = vkCreateFramebuffer(device, &frame_buffer_info, nullptr, &result[i]);

            if(vkr != VK_SUCCESS){
                printf("bik problem %i\n", __LINE__);
            }
        }
        return result;
    }};

    auto frame_buffers {create_frame_buffers()};

    VkCommandPool command_pool;
    {
        VkCommandPoolCreateInfo command_pool_create_info {};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_create_info.queueFamilyIndex = queue_index;

        vkr = vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool);

        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }
    }

    VkCommandBuffer command_buffer;
    {
        VkCommandBufferAllocateInfo alloc_info
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        vkr = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }
    }

    VkClearValue clear_red {{{1.f, 0.f, 0.f, 1.f}}};

    VkSemaphore get_image_semaphore;
    VkSemaphore render_finished_semaphore;

    VkFence fence;

    VkSemaphoreCreateInfo semaphore_create_info
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    vkr = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &get_image_semaphore);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    vkr = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &render_finished_semaphore);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    vkr = vkCreateFence(device, &fence_create_info, nullptr, &fence);

    if(vkr != VK_SUCCESS){
        printf("bik problem %i\n", __LINE__);
    }

    auto running {true};
	for(;running;)
    {
        SDL_Event e;
		while(SDL_PollEvent(&e))
        {
			if(e.type == SDL_QUIT){
                running = false;
            }
            else if(e.type == SDL_KEYDOWN)
            {
                if(e.key.keysym.sym == SDLK_ESCAPE){
                    running = false;
                }
            }
		}

        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

        u32 ii;
        vkr = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, get_image_semaphore, VK_NULL_HANDLE, &ii); 
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        vkResetFences(device, 1, &fence);

        vkResetCommandBuffer(command_buffer, 0);

        VkCommandBufferBeginInfo begin_info {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

        vkr = vkBeginCommandBuffer(command_buffer, &begin_info);
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        VkRenderPassBeginInfo render_pass_begin_info
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = frame_buffers[ii],
            .renderArea = {.offset = {0, 0}, .extent = vk_extent},
            .clearValueCount = 1,
            .pClearValues = &clear_red,
        };

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        //vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
        vkr = vkEndCommandBuffer(command_buffer);
        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        VkPipelineStageFlags wait_stage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submit_info
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &get_image_semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished_semaphore,
        };

        vkr = vkQueueSubmit(device_queue, 1, &submit_info, fence);

        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &render_finished_semaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &ii;

        vkr = vkQueuePresentKHR(present_queue, &presentInfo);

        if(vkr != VK_SUCCESS){
            printf("bik problem %i\n", __LINE__);
        }
	}

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(context, vk_surface, nullptr);
    vkDestroyInstance(context, nullptr);
	SDL_DestroyWindow(window);
	SDL_Quit();
    return 0;
}
