#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "types.hpp"

#define VKT(x) VK_STRUCTURE_TYPE_##x

#define check_vk(x) if(x != VK_SUCCESS) assert(false);

struct GPU
{
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR format;
    VkQueue device_queue;
    u32 queue_index;
    VkSurfaceCapabilitiesKHR capabilities;
    VkPhysicalDeviceProperties properties;
    Array<VkSurfaceFormatKHR> surface_formats;
    Array<VkQueueFamilyProperties> queue_families;

    Array<VkImage> swapchain_images;
    Array<VkImageView> swapchain_image_views;
};

struct Synchronization
{
    VkSemaphore fetch;
    VkSemaphore draw;
    VkFence fence;
};

struct Pipeline
{
    VkPipelineLayout layout;
    VkPipeline pipeline;
};


struct Context
{
    int width;
    int height;

    SDL_Window* window;

    Synchronization syncs;
    VkInstance instance;
    VkSurfaceKHR surface;

    VkViewport viewport;
    VkRect2D scissor;

    VkExtent2D extent;

    VkRenderPass render_pass;

    Array<const char*> extensions;
    Array<VkLayerProperties> layers;
    Array<const char*> layer_names;
    Array<GPU> gpus;

    Array<VkFramebuffer> framebuffers;

    VkShaderModule vertex_shader;
    VkShaderModule i_vertex_shader;
    VkShaderModule frag_shader;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    
    VkPipelineVertexInputStateCreateInfo vertex_input_stage    {};
    VkPipelineInputAssemblyStateCreateInfo input_assembly      {};
    VkPipelineTessellationStateCreateInfo tessellation_info    {};
    VkPipelineViewportStateCreateInfo viewport_info            {};
    VkPipelineRasterizationStateCreateInfo rasterization_info  {};
    VkPipelineMultisampleStateCreateInfo multisample_info      {};
    VkPipelineColorBlendAttachmentState color_blend_attachment {};
    VkPipelineColorBlendStateCreateInfo color_blend_info       {};

    GPU* gpu {nullptr};

    float queue_priority {1.f};

    Array<Pipeline> pipelines;
    u32 swapchain_image;

    VkShaderModule generic_fragment_shader {};

    void init(const char* name, const int w, const int h)
    {
        // TODO do proper error handling noob

        width = w;
        height = h;
        window = SDL_CreateWindow("vulkan test",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height,
                                  SDL_WINDOW_VULKAN);

        u32 ctr;
        VkResult err;

        {
            SDL_Vulkan_GetInstanceExtensions(window, &ctr, nullptr);
            extensions.resize(ctr);
            auto res {SDL_Vulkan_GetInstanceExtensions(window, &ctr, extensions.data())};
            if(res != SDL_TRUE){
                assert(false);
            }
        }

        vkEnumerateInstanceLayerProperties(&ctr, nullptr);
        layers.resize(ctr);
        err = vkEnumerateInstanceLayerProperties(&ctr, layers.data());
        check_vk(err);

        const char* unwanted_layers[] {"VK_LAYER_LUNARG_api_dump"};

        for(auto& l : layers)
        {
            auto found {false};
            for(auto j : unwanted_layers)
            {
                if(String{j} == l.layerName)
                {
                    found = true;
                    break;
                }
            }
            if(!found){
                layer_names.push_back(l.layerName);
            }
        }

        {

            VkApplicationInfo app_info
            {
                .sType = VKT(APPLICATION_INFO),
                .apiVersion = VK_API_VERSION_1_3
            };

            VkInstanceCreateInfo info
            {
                .sType = VKT(INSTANCE_CREATE_INFO),
                .pApplicationInfo = &app_info,
                .enabledLayerCount = (u32)layer_names.size(),
                .ppEnabledLayerNames = layer_names.data(),
                .enabledExtensionCount = (u32)extensions.size(),
                .ppEnabledExtensionNames = extensions.data()
            };

            err = vkCreateInstance(&info, nullptr, &instance);
            check_vk(err);
            auto res {SDL_Vulkan_CreateSurface(window, instance, &surface)};
            if(res != SDL_TRUE){
                assert(false);
            }
        }

        {
            Array<VkPhysicalDevice> devices;

            vkEnumeratePhysicalDevices(instance, &ctr, nullptr);
            devices.resize(ctr);
            err = vkEnumeratePhysicalDevices(instance, &ctr, devices.data());
            check_vk(err);

            for(auto& p : devices)
            {
                Array<VkQueueFamilyProperties> families;
                vkGetPhysicalDeviceQueueFamilyProperties(p, &ctr, nullptr);
                families.resize(ctr);
                vkGetPhysicalDeviceQueueFamilyProperties(p, &ctr, families.data());

                Array<VkDeviceQueueCreateInfo> queue_infos;
                queue_infos.reserve(families.size());

                {
                    u32 ctr {0};
                    for(auto& f : families)
                    {
                        VkDeviceQueueCreateInfo queue_info
                        {
                            .sType = VKT(DEVICE_QUEUE_CREATE_INFO),
                            .queueFamilyIndex = ctr,
                            .queueCount = 1,
                            .pQueuePriorities = &queue_priority,
                        };
                        queue_infos.push_back(queue_info);
                        ctr++;
                    }
                }

                const char* required_extensions[] {"VK_KHR_portability_subset", VK_KHR_SWAPCHAIN_EXTENSION_NAME};

                Array<const char*> device_extensions {required_extensions[0], required_extensions[1]};

                // NOTE this causes an access violation error during second call to vkEnumerateDeviceExtensionProperties
                // just manually added required extentions for now in the device extensions cause can't check if extension is available for the device!
                //Array<VkExtensionProperties> extension_properties;
                //vkEnumerateDeviceExtensionProperties(p, nullptr, &ctr, nullptr);
                //extension_properties.resize(ctr);
                //err = vkEnumerateDeviceExtensionProperties(p, nullptr, &ctr, extension_properties.data());
                //check_vk(err);

                //Array<const char*> device_extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

                //for(auto& i : extension_properties)
                //{
                //    for(auto j : required_extensions)
                //    {
                //        if(String{i.extensionName} == j)
                //        {
                //            device_extensions.push_back(j);
                //            break;
                //        }
                //    }
                //}

                VkPhysicalDeviceFeatures features;

                vkGetPhysicalDeviceFeatures(p, &features);

                VkDeviceCreateInfo device_info
                {
                    .sType = VKT(DEVICE_CREATE_INFO),
                    .queueCreateInfoCount = (u32)queue_infos.size(),
                    .pQueueCreateInfos = queue_infos.data(),
                    .enabledExtensionCount = (u32)device_extensions.size(),
                    .ppEnabledExtensionNames = device_extensions.data(),
                    .pEnabledFeatures = &features
                };

                VkDevice device;
                err = vkCreateDevice(p, &device_info, nullptr, &device);
                check_vk(err);

                gpus.push_back({});
                auto& g {gpus.back()};
                g.gpu = p;
                g.device = device;
                g.queue_families = families;
            }
        }
        assert(!gpus.empty());
        gpu = &gpus[0];

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->gpu, surface, &gpu->capabilities);
        extent = {(u32)width, (u32)height};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = extent.width; 
        viewport.height = extent.height; 
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        scissor.offset = {0, 0};
        scissor.extent = extent;

        vkGetPhysicalDeviceProperties(gpu->gpu, &gpu->properties);

        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->gpu, surface, &ctr, nullptr);
        gpu->surface_formats.resize(ctr);
        err = vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->gpu, surface, &ctr, gpu->surface_formats.data());
        check_vk(err);

        gpu->format = gpu->surface_formats[0];

        for(auto& f : gpu->surface_formats)
        {
            if(f.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                gpu->format = f;
                break;
            }
        }

        {
            u32 ctr {0};
            for(auto& f : gpu->queue_families)
            {
                if(f.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    gpu->queue_index = ctr;
                    break;
                }
            }
            vkGetDeviceQueue(gpu->device, gpu->queue_index, 0, &gpu->device_queue);
        }

        {
            u32 family_indices[] {gpu->queue_index};
            VkSwapchainCreateInfoKHR info
            {
                .sType = VKT(SWAPCHAIN_CREATE_INFO_KHR),
                .surface = surface,
                .minImageCount = gpu->capabilities.minImageCount + 1,
                .imageFormat = gpu->format.format,
                .imageColorSpace = gpu->format.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = family_indices,
                .preTransform = gpu->capabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                .clipped = VK_TRUE,
                .oldSwapchain = VK_NULL_HANDLE,
            };

            err = vkCreateSwapchainKHR(gpu->device, &info, nullptr, &gpu->swapchain);
            check_vk(err);

            vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &ctr, nullptr);
            gpu->swapchain_images.resize(ctr);
            err = vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &ctr, gpu->swapchain_images.data());
            check_vk(err);

        }
        {
            VkCommandPoolCreateInfo info
            {
                .sType = VKT(COMMAND_POOL_CREATE_INFO),
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gpu->queue_index
            };
            err = vkCreateCommandPool(gpu->device, &info, nullptr, &command_pool);
            check_vk(err);

            VkCommandBufferAllocateInfo buffer_info
            {
                .sType = VKT(COMMAND_BUFFER_ALLOCATE_INFO),
                .commandPool = command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            err = vkAllocateCommandBuffers(gpu->device, &buffer_info, &command_buffer);
            check_vk(err);
        }

        {
            VkAttachmentDescription ad
            {
                .format = gpu->format.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };

            VkAttachmentReference color_attachment
            {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            VkSubpassDescription sp
            {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
            };

            VkRenderPassCreateInfo info
            {
                .sType = VKT(RENDER_PASS_CREATE_INFO),
                .attachmentCount = 1,
                .pAttachments = &ad,
                .subpassCount = 1,
                .pSubpasses = &sp,
            };

            err = vkCreateRenderPass(gpu->device, &info, nullptr, &render_pass);
            check_vk(err);
        }

        {
            for(auto i : gpu->swapchain_images)
            {
                VkImageViewCreateInfo info
                {
                    .sType = VKT(IMAGE_VIEW_CREATE_INFO),
                    .image = i,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = gpu->format.format,
                    .subresourceRange = {
                                          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                          .baseMipLevel = 0,
                                          .levelCount = 1,
                                          .baseArrayLayer = 0,
                                          .layerCount = 1}
                };

                VkImageView image_view;
                err = vkCreateImageView(gpu->device, &info, nullptr, &image_view);
                check_vk(err);
                gpu->swapchain_image_views.push_back(image_view);
            }
            for(auto& i : gpu->swapchain_image_views)
            {
                VkFramebufferCreateInfo info
                {
                    .sType = VKT(FRAMEBUFFER_CREATE_INFO),
                    .renderPass = render_pass,
                    .attachmentCount = 1,
                    .pAttachments = &i,
                    .width = extent.width,
                    .height = extent.height,
                    .layers = 1,
                };
                VkFramebuffer framebuffer;
                err = vkCreateFramebuffer(gpu->device, &info, nullptr, &framebuffer);
                check_vk(err);
                framebuffers.push_back(framebuffer);
            }
        }

        generic_fragment_shader = load_shader("shader.frag.spv");
    }

    void build_synchronization()
    {
        VkResult err;
        {
            VkSemaphoreCreateInfo info
            {
                .sType = VKT(SEMAPHORE_CREATE_INFO)
            };
            err = vkCreateSemaphore(gpu->device, &info, nullptr, &syncs.fetch);
            check_vk(err);

            err = vkCreateSemaphore(gpu->device, &info, nullptr, &syncs.draw);
            check_vk(err);
        }
        
        {
            VkFenceCreateInfo info
            {
                .sType = VKT(FENCE_CREATE_INFO)
            };
            err = vkCreateFence(gpu->device, &info, nullptr, &syncs.fence);
            check_vk(err);
        }
    }

    void build_pipeline_stages()
    {
        vertex_input_stage.sType = VKT(PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);

        input_assembly.sType = VKT(PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        tessellation_info.sType = VKT(PIPELINE_TESSELLATION_STATE_CREATE_INFO);
        tessellation_info.patchControlPoints = gpu->properties.limits.maxTessellationPatchSize;

        viewport_info.sType = VKT(PIPELINE_VIEWPORT_STATE_CREATE_INFO);
        viewport_info.viewportCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.scissorCount = 1;
        viewport_info.pScissors = &scissor;

        rasterization_info.sType = VKT(PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        rasterization_info.depthClampEnable = VK_FALSE;
        rasterization_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.lineWidth = 1;

        multisample_info.sType = VKT(PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.sampleShadingEnable = VK_FALSE;

        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        color_blend_info.sType = VKT(PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        color_blend_info.logicOpEnable = VK_FALSE;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attachment;
    }

    VkShaderModule load_shader(const String& d)
    {
        VkResult err;
        String code;
        std::ifstream f {d, f.binary | f.ate};

        u32 size;
        size = f.tellg();
        code.resize(size);
        f.seekg(0);
        f.read(code.data(), size);

        VkShaderModuleCreateInfo info
        {
            .sType = VKT(SHADER_MODULE_CREATE_INFO),
            .codeSize = size,
            .pCode = (u32*)code.data(),
        };

        VkShaderModule module;
        err = vkCreateShaderModule(gpu->device, &info, nullptr, &module);
        check_vk(err);

        return module;
    }

    VkGraphicsPipelineCreateInfo new_pipeline_create_info()
    {
        VkGraphicsPipelineCreateInfo info
        {
            .sType = VKT(GRAPHICS_PIPELINE_CREATE_INFO),
            .pVertexInputState = &vertex_input_stage,
            .pInputAssemblyState = &input_assembly,
            .pTessellationState = &tessellation_info,
            .pViewportState = &viewport_info,
            .pRasterizationState = &rasterization_info,
            .pMultisampleState = &multisample_info,
            .pColorBlendState = &color_blend_info, 
            .renderPass = render_pass,
            .subpass = 0,
        };
        return info;
    }

    template<typename S>
    VkPipelineShaderStageCreateInfo new_shader_stage(S s, VkShaderModule module)
    {
        VkPipelineShaderStageCreateInfo info
        {
            .sType = VKT(PIPELINE_SHADER_STAGE_CREATE_INFO),
            .stage = s,
            .module = module,
            .pName = "main"
        };
        return info;
    }

    void render_reset(const RGBA& color)
    {
        // TODO error handling
        VkClearValue clear {{{color.r, color.g, color.b, color.a}}};

        vkWaitForFences(gpu->device, 1, &syncs.fence, VK_TRUE, 50000000);
        vkResetFences(gpu->device, 1, &syncs.fence);

        vkResetCommandBuffer(command_buffer, 0);

        VkResult err;
        vkAcquireNextImageKHR(gpu->device, gpu->swapchain, 50000000, syncs.fetch, VK_NULL_HANDLE, &swapchain_image);

        VkRenderPassBeginInfo render_pass_begin
        {
            .sType = VKT(RENDER_PASS_BEGIN_INFO),
            .renderPass = render_pass,
            .framebuffer = framebuffers[swapchain_image],
            .renderArea {.offset = {0, 0}, .extent = extent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        VkCommandBufferBeginInfo buffer_begin_info
        {
            .sType = VKT(COMMAND_BUFFER_BEGIN_INFO),
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        };

        vkBeginCommandBuffer(command_buffer, &buffer_begin_info);
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    void present()
    {
        VkResult err;
        vkCmdEndRenderPass(command_buffer);

        vkEndCommandBuffer(command_buffer);

        VkPipelineStageFlags stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submit
        {
            .sType = VKT(SUBMIT_INFO),
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &syncs.fetch,
            .pWaitDstStageMask = stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &syncs.draw,
        };

        err = vkQueueSubmit(gpu->device_queue, 1, &submit, syncs.fence);
        check_vk(err);

        VkPresentInfoKHR present
        {
            .sType = VKT(PRESENT_INFO_KHR),
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &syncs.draw,
            .swapchainCount = 1,
            .pSwapchains = &gpu->swapchain, 
            .pImageIndices = &swapchain_image
        };

        err = vkQueuePresentKHR(gpu->device_queue, &present);
        check_vk(err);

    }

    float aspect_ratio()
    {
        return (float)width / (float)height;
    }

    V2 norm(const float x, const float y)
    {
        V2 r;
        r.x = ((float)x / (float)width) * 2.f - 1.f;
        r.y = ((float)y / (float)height) * 2.f - 1.f;
        return r;
    }

    template<typename F>
    u32 add_new_pipeline(F f)
    {
        pipelines.push_back(f());
        return pipelines.size() - 1;
    }

    Pipeline get_pipeline(const int id)
    {
        return pipelines[id];
    }

};

