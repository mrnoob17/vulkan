#define SDL_MAIN_HANDLED

#include <cstdint>
#include <cassert>
#include <fstream>
#include <chrono>
#include <cmath>

using Time = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<float>;

#include "context.hpp"
#include "types.hpp"

/* TODO
 
    projection matrix 
    render targets
    VkBuffer 
    shader attributes 
 
 */

Array<Array<float>> matrix_product(const Array<Array<float>>& a, const Array<Array<float>>& b)
{
    assert(a[0].size() == b.size());

    Array<Array<float>> result(a.size());
    for(auto& v : result){
        v.resize(b[0].size());
    }

    for(int i = 0; i < a.size(); i++)
    {
        for(int j = 0; j < a[i].size(); j++)
        {
            for(int k = 0; k < a[i].size(); k++){
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

int main()
{
    Context context;
    context.init("vulkan test", 1280, 720);
    context.build_synchronization();
    context.build_pipeline_stages();

    auto immediate_pipeline {context.add_new_pipeline([&]() -> Pipeline
    {
        struct Data
        {
            V4 a[3];
            RGBA b[3];
        };

        Pipeline result;
        VkResult err;
        auto& c {context};
        auto info {c.new_pipeline_create_info()};

        auto vertex {c.load_shader("ishader.vert.spv")};
        auto frag {c.load_shader("shader.frag.spv")};

        const auto shader_count {2};

        info.stageCount = shader_count;

        VkPipelineShaderStageCreateInfo shader_stages[shader_count] {};

        shader_stages[0] = c.new_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vertex);
        shader_stages[1] = c.new_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, frag);
        info.pStages = shader_stages;

        VkPushConstantRange constant
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Data),
        };

        VkPipelineLayoutCreateInfo layout_info
        {
            .sType = VKT(PIPELINE_LAYOUT_CREATE_INFO),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &constant,
        };

        err = vkCreatePipelineLayout(c.gpu->device, &layout_info, nullptr, &result.layout);
        check_vk(err);

        info.layout = result.layout;
        err = vkCreateGraphicsPipelines(c.gpu->device, VK_NULL_HANDLE, 1, &info, nullptr, &result.pipeline);
        check_vk(err);
        vkDestroyShaderModule(c.gpu->device, vertex, nullptr);
        vkDestroyShaderModule(c.gpu->device, frag, nullptr);
        return result;
    })};

    auto additive_pipeline {context.add_new_pipeline([&]() -> Pipeline
    {
        struct Data
        {
            V4 a[3];
            RGBA b[3];
        };

        Pipeline result;
        VkResult err;
        auto& c {context};
        auto info {c.new_pipeline_create_info()};

        VkPipelineColorBlendAttachmentState color_blend_attachment {};
        VkPipelineColorBlendStateCreateInfo color_blend_info       {};

        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        color_blend_info.sType = VKT(PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        color_blend_info.logicOpEnable = VK_FALSE;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attachment;
        info.pColorBlendState = &color_blend_info;

        auto vertex {c.load_shader("ishader.vert.spv")};
        auto frag {c.load_shader("shader.frag.spv")};

        const auto shader_count {2};

        info.stageCount = shader_count;

        VkPipelineShaderStageCreateInfo shader_stages[shader_count] {};

        shader_stages[0] = c.new_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vertex);
        shader_stages[1] = c.new_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, frag);
        info.pStages = shader_stages;

        VkPushConstantRange constant
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Data),
        };

        VkPipelineLayoutCreateInfo layout_info
        {
            .sType = VKT(PIPELINE_LAYOUT_CREATE_INFO),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &constant,
        };

        err = vkCreatePipelineLayout(c.gpu->device, &layout_info, nullptr, &result.layout);
        check_vk(err);

        info.layout = result.layout;
        err = vkCreateGraphicsPipelines(c.gpu->device, VK_NULL_HANDLE, 1, &info, nullptr, &result.pipeline);
        check_vk(err);
        vkDestroyShaderModule(c.gpu->device, vertex, nullptr);
        vkDestroyShaderModule(c.gpu->device, frag, nullptr);
        return result;
    })};

    auto running {true};

    VkClearValue clear {{{1.f, 0.8f, 0.8f, 1.f}}};

    V2 mouse;

    auto clamp {[](auto a, auto b, auto c)
    {
        if(a < b){
            return b;
        }
        if(a > c){
            return c;
        }
        return a;
    }};

    auto render_triangle {[&](const int p, V2 a, V2 b, V2 c, const RGBA& ca, const RGBA& cb, const RGBA& cc, const float rotation = 0.f, V2 mid = {FLT_MAX, FLT_MAX})
    {
        const auto& pl {context.get_pipeline(p)};

        struct Data
        {
            V4 a[3];
            RGBA b[3];
        };

        constexpr auto size {sizeof(Data)};

        const auto sin {sinf(rotation)};
        const auto cos {cosf(rotation)};

        const Array<Array<float>> r_matrix {{cos, -sin},
                                            {sin, cos}};

        if(mid.x == FLT_MAX)
        {
            mid.x = a.x + b.x + c.x;
            mid.y = a.y + b.y + c.y;
            mid.x /= 3.f;
            mid.y /= 3.f;
        }

        a.x -= mid.x;
        b.x -= mid.x;
        c.x -= mid.x;
        a.y -= mid.y;
        b.y -= mid.y;
        c.y -= mid.y;

        auto coords {matrix_product({{a.x, a.y}}, r_matrix)};

        a.x = coords[0][0] + mid.x;
        a.y = coords[0][1] + mid.y;

        coords = matrix_product({{b.x, b.y}}, r_matrix);
        b.x = coords[0][0] + mid.x;
        b.y = coords[0][1] + mid.y;

        coords = matrix_product({{c.x, c.y}}, r_matrix);
        c.x = coords[0][0] + mid.x;
        c.y = coords[0][1] + mid.y;

        a = context.norm(a.x, a.y);
        b = context.norm(b.x, b.y);
        c = context.norm(c.x, c.y);

        float data[24]{
                       a.x,  a.y,  0.f,  1.f,
                       b.x,  b.y,  0.f,  1.f,
                       c.x,  c.y,  0.f,  1.f,
                       ca.r, ca.g, ca.b, ca.a,
                       cb.r, cb.g, cb.b, ca.a,
                       cc.r, cc.g, cc.b, cc.a 
        };


        vkCmdBindPipeline(context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pl.pipeline);

        vkCmdPushConstants(context.command_buffer, pl.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, size, (void*)(data));

        vkCmdDraw(context.command_buffer, 3, 1, 0, 0);
    }};

    auto i_render_triangle {[&](const int p, V2 a, V2 b, V2 c, RGBA color, const float rotation = 0.f, V2 mid = {FLT_MAX, FLT_MAX})
    {
        render_triangle(p, a, b, c, color, color, color, rotation, mid);
    }};

    auto render_rectangle{[&](const int p, V2 a, V2 b, const RGBA& c, const float rotation = 0)
    {
        V2 mid;
        mid.x = a.x + b.x * 0.5f;
        mid.y = a.y + b.y * 0.5f;
        i_render_triangle(p, a, {a.x + b.x, a.y}, {a.x, a.y + b.y}, c, rotation, mid);
        i_render_triangle(p, {a.x + b.x, a.y}, {a.x, a.y + b.y}, {a.x + b.x, a.y + b.y}, c, rotation, mid);
    }};


    auto start {Time::now()};
    auto end {Time::now()};

    float delta {};
    u64 frame {0};
    float angle {0.f};

    constexpr auto dt {1.f / 60.f};

    while(running)
    {
        start = Time::now();
        delta = Duration{end - start}.count();
        SDL_Event e;
        while(SDL_PollEvent(&e))
        {
            if(e.type == SDL_QUIT){
                running = false;
            }
        }

        {
            int x;
            int y;
            SDL_GetMouseState(&x, &y);

            mouse.x = x;
            mouse.y = y;

            auto& c {clear.color.float32};
            c[0] = 0;
            c[1] = 0;
            c[2] = 0;
        }

        angle += (M_PI * dt) * 0.25f;
        if(angle >= M_PI * 2.f){
            angle = 0.0f;
        }

        vkWaitForFences(context.gpu->device, 1, &context.syncs.fence, VK_TRUE, 50000000);
        vkResetFences(context.gpu->device, 1, &context.syncs.fence);

        vkResetCommandBuffer(context.command_buffer, 0);

        VkResult err;
        u32 image;
        vkAcquireNextImageKHR(context.gpu->device, context.gpu->swapchain, 50000000, context.syncs.fetch, VK_NULL_HANDLE, &image);

        VkRenderPassBeginInfo render_pass_begin
        {
            .sType = VKT(RENDER_PASS_BEGIN_INFO),
            .renderPass = context.render_pass,
            .framebuffer = context.framebuffers[image],
            .renderArea {.offset = {0, 0}, .extent = context.extent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        VkCommandBufferBeginInfo buffer_begin_info
        {
            .sType = VKT(COMMAND_BUFFER_BEGIN_INFO),
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        };

        vkBeginCommandBuffer(context.command_buffer, &buffer_begin_info);
        vkCmdBeginRenderPass(context.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

        i_render_triangle(immediate_pipeline, {500, 0}, {10, 100}, { 510, 80}, {1.f, 1.f, 1.f, 1.f}, angle);

        render_triangle(immediate_pipeline, {500,  300}, {400, 450}, {575, 400}, {1.f, 0.f, 1.f, 1.f}, {0, 1.f, 1.f, 1.f}, {1.f, 1.f, 0, 1.f}, angle);

        i_render_triangle(immediate_pipeline, {510,  300}, {600, 600}, {550, 400}, {0, 0.f, 1.f, 1.f}, angle);

        i_render_triangle(additive_pipeline, {550,  300}, {650, 600}, {580, 350}, {1, 0.f, 0.f, 1.f}, angle);

        V2 size {720, 720};
        render_rectangle(additive_pipeline, {mouse.x - size.x * 0.5f, mouse.y - size.y * 0.5f}, size, {0, 1, 0, 1.f});

        vkCmdEndRenderPass(context.command_buffer);

        vkEndCommandBuffer(context.command_buffer);

        VkPipelineStageFlags stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submit
        {
            .sType = VKT(SUBMIT_INFO),
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.syncs.fetch,
            .pWaitDstStageMask = stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &context.command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &context.syncs.draw,
        };

        err = vkQueueSubmit(context.gpu->device_queue, 1, &submit, context.syncs.fence);
        check_vk(err);

        VkPresentInfoKHR present
        {
            .sType = VKT(PRESENT_INFO_KHR),
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.syncs.draw,
            .swapchainCount = 1,
            .pSwapchains = &context.gpu->swapchain, 
            .pImageIndices = &image
        };

        err = vkQueuePresentKHR(context.gpu->device_queue, &present);
        check_vk(err);

        end = Time::now();
        frame++;
    }
}
