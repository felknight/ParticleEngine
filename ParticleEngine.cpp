#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.hpp>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include <thread>
#include <iostream>
#include <random>
#include <mutex>
#include <condition_variable>

using namespace vk;

#include "ParticleEngine_DebugFunc.hpp"
#include "ParticleEngine_Configs.hpp"
#include "ParticleEngine.h"

static Instance vinstance;
static PhysicalDevice main_device;
static uint32_t qfamilyIndex;
static CommandPool pool;
static RenderPass render;
static Pipeline pipeline;
static Framebuffer framebuffer;
static Queue queue;
static Buffer circleGeometryDevice;
static Buffer particleBuffer;
static char* ParticleMap;
static DescriptorSet storageBufferSet;
static PipelineLayout mainLayout;
static Format surfaceFormat;
static SwapchainKHR mainChain;
static std::vector<CommandBuffer> drawCommandBuffers;
static std::vector<Image> swapImages;
static Image renderImage;

static xcb_connection_t *connection;
static xcb_window_t window;

static Device D;

static uint32_t memDeviceLocal;
static uint32_t memHostVisible;

static uint32_t fbWidth = 512;
static uint32_t fbHeight = 512;
static uint32_t N = 10;

static Particle* mainMemory;


static void CreateInstance()
{
    InstanceCreateInfo cf;
    cf.ppEnabledExtensionNames = extensions;
    cf.ppEnabledLayerNames = layers;
    cf.enabledExtensionCount = extno;
    cf.enabledLayerCount = layno;

    vinstance = createInstance(cf);

    DebugReportCallbackCreateInfoEXT cfd;
    cfd.setFlags(DebugReportFlagBitsEXT::eWarning 
        | DebugReportFlagBitsEXT::eError 
        | DebugReportFlagBitsEXT::eInformation);
    cfd.pfnCallback = debugCallback;

    auto vkCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)
        vkGetInstanceProcAddr(vinstance, "vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackEXT debug;
    vkCreateDebugReportCallback(vinstance, 
        reinterpret_cast<VkDebugReportCallbackCreateInfoEXT*>(&cfd), nullptr, &debug);
}

static bool CreateDevice()
{
    float prio = 1.0f;
    auto phy = vinstance.enumeratePhysicalDevices();
    main_device = phy[0];

    auto mem = main_device.getMemoryProperties();
    for (int i = 0; i < mem.memoryTypeCount; i++)
    {
        if (mem.memoryTypes[i].propertyFlags & MemoryPropertyFlagBits::eDeviceLocal)
        {
            memDeviceLocal = i;
            break;
        }
    }

    for (int i = 0; i < mem.memoryTypeCount; i++)
    {
        if (mem.memoryTypes[i].propertyFlags & MemoryPropertyFlagBits::eHostVisible)
        {
            memHostVisible = i;
            break;
        }
    }

    if (false)
    {
        printf("\nFormatos: \n");
        for (int i = VK_FORMAT_BEGIN_RANGE; i < VK_FORMAT_END_RANGE; i++)
        {
            auto forp = main_device.getFormatProperties((Format)i);
            if (forp.linearTilingFeatures & FormatFeatureFlagBits::eTransferSrcKHR
                && forp.linearTilingFeatures & FormatFeatureFlagBits::eColorAttachment)
            {
                std::cout << to_string((Format)i) << "\n";
            }
                
        }
        printf("\n\n");
    }
    
    auto fp = main_device.getQueueFamilyProperties();
    qfamilyIndex = -1;

    for (int i = 0; i < fp.size(); i++)
    {
        auto &f = fp[i];
        if (f.queueFlags & QueueFlagBits::eGraphics 
            && f.queueFlags & QueueFlagBits::eCompute)
        qfamilyIndex = i;
    }

    if (qfamilyIndex == -1)
    {
        fprintf(stderr, "No subitable queue family\n");
        return false;
    }

    DeviceQueueCreateInfo cfq;
    cfq.queueFamilyIndex = qfamilyIndex;
    cfq.queueCount = 1;
    cfq.pQueuePriorities = &prio;

    DeviceCreateInfo cf;
    cf.ppEnabledLayerNames = layers;
    cf.enabledLayerCount = layno;
    cf.ppEnabledExtensionNames = dev_extensions;
    cf.enabledExtensionCount = devextno;
    cf.pQueueCreateInfos = &cfq;
    cf.queueCreateInfoCount = 1;

    D = main_device.createDevice(cf);
    return true;
}

static void CreateCommandQueue()
{
    queue = D.getQueue(qfamilyIndex, 0);
    CommandPoolCreateInfo cf;
    cf.queueFamilyIndex = qfamilyIndex;
    pool = D.createCommandPool(cf);
}

static void CreateRenderPass()
{
    AttachmentReference colorref;
    colorref.layout = ImageLayout::eColorAttachmentOptimal;
    colorref.attachment = 0;

    SubpassDescription subpass;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorref;

    AttachmentDescription color;
    color.format = surfaceFormat;
    color.loadOp = AttachmentLoadOp::eClear;
    color.storeOp = AttachmentStoreOp::eStore;
    color.initialLayout = ImageLayout::eUndefined;
    color.finalLayout = ImageLayout::eTransferSrcOptimal;
    color.samples = SampleCountFlagBits::e4;

    RenderPassCreateInfo ren;
    ren.subpassCount = 1;
    ren.pSubpasses = &subpass;
    ren.attachmentCount = 1;
    ren.pAttachments = &color;

    render = D.createRenderPass(ren);
}

static void CreatePipeline()
{
    GraphicsPipelineCreateInfo cf;

    PipelineShaderStageCreateInfo stages[Modules()];
    for (int i = 0; i <  Modules(); i++)
    {
        ShaderModuleCreateInfo mcf;
        mcf.pCode = ModulesData()[i];
        mcf.codeSize = ModuleSizes()[i];
        ShaderModule mod = D.createShaderModule(mcf);

        stages[i].module = mod;
        stages[i].stage = ModulesStage()[i];
        stages[i].pName = "main";
    }

    cf.stageCount = Modules();
    cf.pStages = stages;

    VertexInputBindingDescription bind;
    bind.stride = sizeof(float) * 2;
    bind.inputRate = VertexInputRate::eVertex;
    bind.binding = 0;

    VertexInputAttributeDescription att;
    att.offset = 0;
    att.format = Format::eR32G32Sfloat;
    att.location = 0;
    att.binding = 0;    

    PipelineVertexInputStateCreateInfo vert;
    vert.setPVertexBindingDescriptions(&bind);
    vert.vertexBindingDescriptionCount = 1;
    vert.setPVertexAttributeDescriptions(&att);
    vert.vertexAttributeDescriptionCount = 1;
    cf.pVertexInputState = &vert;

    PipelineInputAssemblyStateCreateInfo iass;
    iass.primitiveRestartEnable = false;
    iass.topology = PrimitiveTopology::eTriangleFan;
    cf.pInputAssemblyState = &iass;
    PipelineRasterizationStateCreateInfo ras;
    ras.lineWidth = 1.0f;
    cf.pRasterizationState = &ras;
    PipelineMultisampleStateCreateInfo mul;
    mul.sampleShadingEnable = true;
    mul.rasterizationSamples = SampleCountFlagBits::e4;
    mul.minSampleShading = 0.11f;
    mul.alphaToOneEnable = false;
    mul.alphaToCoverageEnable = false;
    cf.pMultisampleState = &mul;

    PipelineColorBlendAttachmentState blendatach;
    blendatach.blendEnable = true;
    blendatach.colorWriteMask = ColorComponentFlagBits::eR
        | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB
        | ColorComponentFlagBits::eA;
    blendatach.srcAlphaBlendFactor = BlendFactor::eOne;
    blendatach.dstAlphaBlendFactor = BlendFactor::eOneMinusSrcAlpha;
    blendatach.srcColorBlendFactor = BlendFactor::eOne;
    blendatach.dstColorBlendFactor = BlendFactor::eOneMinusSrcAlpha;
    blendatach.colorBlendOp = BlendOp::eAdd;
    blendatach.alphaBlendOp = BlendOp::eAdd;


    PipelineColorBlendStateCreateInfo blend;
    blend.logicOpEnable = false;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendatach;
    cf.pColorBlendState = &blend;

    Viewport viewport = Viewport(0, 0, fbWidth, fbHeight, 0, 1);
    Rect2D scissor = Rect2D( Offset2D(0, 0), Extent2D(fbWidth, fbHeight) );
    PipelineViewportStateCreateInfo view;
    view.viewportCount = 1;
    view.pViewports = &viewport;
    view.scissorCount = 1;
    view.pScissors = &scissor;
    cf.pViewportState = &view;

    DescriptorSetLayoutBinding buffdesc;
    buffdesc.binding = 0;
    buffdesc.descriptorType = DescriptorType::eStorageBuffer;
    buffdesc.stageFlags = ShaderStageFlagBits::eVertex;
    buffdesc.descriptorCount = 1;

    DescriptorSetLayoutCreateInfo deslay;
    deslay.bindingCount = 1;
    deslay.pBindings = &buffdesc;

    auto actualayout = D.createDescriptorSetLayout(deslay);

    PipelineLayoutCreateInfo lay;
    lay.setLayoutCount = 1;
    lay.pSetLayouts = &actualayout;
    mainLayout = D.createPipelineLayout(lay);
    cf.layout = mainLayout;

    cf.renderPass = render;
    pipeline = D.createGraphicsPipeline(nullptr, cf);

    DescriptorPoolSize psize;
    psize.type = DescriptorType::eStorageBuffer;
    psize.descriptorCount = 3;

    DescriptorPoolCreateInfo pool;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &psize;
    pool.maxSets = 1;

    auto dp = D.createDescriptorPool(pool);

    DescriptorSetAllocateInfo ds;
    ds.pSetLayouts = &actualayout;
    ds.descriptorSetCount = 1;
    ds.descriptorPool = dp;
    storageBufferSet = D.allocateDescriptorSets(ds)[0];
}

static void CreateFramebuffer()
{
    ImageCreateInfo im;
    im.mipLevels = 1;
    im.format = surfaceFormat;
    im.imageType = ImageType::e2D;
    im.extent.width = fbWidth;
    im.extent.height = fbHeight;
    im.extent.depth = 1;
    im.samples = SampleCountFlagBits::e4;
    im.usage = ImageUsageFlagBits::eColorAttachment | ImageUsageFlagBits::eTransferSrc;
    im.arrayLayers = 1;

    try
    {
        renderImage = D.createImage(im);
    }
    catch(std::exception& e)
    {
        exit(120);
    }
    auto req = D.getImageMemoryRequirements(renderImage);

    MemoryAllocateInfo mem;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = memDeviceLocal;
    auto allomem = D.allocateMemory(mem);

    D.bindImageMemory(renderImage, allomem, 0);

    ImageViewCreateInfo iv;
    iv.viewType = ImageViewType::e2D;
    iv.format = surfaceFormat;
    iv.subresourceRange.aspectMask = ImageAspectFlagBits::eColor;
    iv.subresourceRange.levelCount = 1;
    iv.subresourceRange.layerCount = 1;
    iv.image = renderImage;

    auto view = D.createImageView(iv);

    FramebufferCreateInfo cf;
    cf.renderPass = render;
    cf.layers = 1;
    cf.width = fbWidth;
    cf.height = fbHeight;
    cf.pAttachments = &view;
    cf.attachmentCount = 1;

    framebuffer = D.createFramebuffer(cf);
}

static void CreateGeometry(int sides)
{
    struct Vertex
    {
        float x, y;
    }* vertices;

    BufferCreateInfo cf;
    cf.queueFamilyIndexCount = 1;
    cf.pQueueFamilyIndices = &qfamilyIndex;
    cf.size = sizeof(Vertex) * (sides + 2);
    cf.usage = BufferUsageFlagBits::eVertexBuffer | BufferUsageFlagBits::eTransferSrc;
    auto circleGeometryHost = D.createBuffer(cf);
    cf.usage = BufferUsageFlagBits::eVertexBuffer | BufferUsageFlagBits::eTransferDst;
    circleGeometryDevice = D.createBuffer(cf);

    auto req = D.getBufferMemoryRequirements(circleGeometryHost);

    MemoryAllocateInfo mf;
    mf.allocationSize = req.size;
    mf.memoryTypeIndex = memHostVisible;
    auto mem = D.allocateMemory(mf);

    D.bindBufferMemory(circleGeometryHost, mem, 0);

    vertices = (Vertex*)D.mapMemory(mem, 0, sizeof(Vertex) * (sides + 2));

    vertices[0].x = 0.0f;
    vertices[0].y = 0.0f;

    for (int i = 1; i < sides + 1; i++)
    {
        constexpr float pi = 3.14159265359f;
        float sidr = 2.0f * pi / (float)sides;
        sidr *= (float)(i - 1);

        vertices[i].x = cos(sidr);
        vertices[i].y = sin(sidr);
    }

    vertices[sides + 1] = vertices[1];

    D.unmapMemory(mem);

    req = D.getBufferMemoryRequirements(circleGeometryDevice);

    mf.allocationSize = req.size;
    mf.memoryTypeIndex = memDeviceLocal;
    mem = D.allocateMemory(mf);

    D.bindBufferMemory(circleGeometryDevice, mem, 0);

    CommandBufferAllocateInfo aif;
    aif.commandPool = pool;
    aif.commandBufferCount = 1;
    auto c = D.allocateCommandBuffers(aif)[0];

    c.begin(CommandBufferBeginInfo());

    BufferCopy reg;
    reg.srcOffset = 0;
    reg.dstOffset = 0;
    reg.size =  sizeof(Vertex) * (sides + 2);
    c.copyBuffer(circleGeometryHost, circleGeometryDevice, 1, &reg);

    c.end();

    SubmitInfo sif;
    sif.commandBufferCount = 1;
    sif.pCommandBuffers = &c;

    Fence f = D.createFence(FenceCreateInfo());
    queue.submit(1, &sif, f);
    D.waitForFences(1, &f, true, 0xFFFFFFFFFFFFFFFF);

}

static void CreateParticlesBuffer()
{
    BufferCreateInfo cf;
    cf.queueFamilyIndexCount = 1;
    cf.pQueueFamilyIndices = &qfamilyIndex;
    cf.size = sizeof(Particle) * N + 16;
    cf.usage = BufferUsageFlagBits::eStorageBuffer;
    particleBuffer = D.createBuffer(cf);

    auto req = D.getBufferMemoryRequirements(particleBuffer);

    MemoryAllocateInfo mf;
    mf.allocationSize = req.size;
    mf.memoryTypeIndex = memHostVisible;
    auto mem = D.allocateMemory(mf);

    D.bindBufferMemory(particleBuffer, mem, 0);

    ParticleMap = (char*)D.mapMemory(mem, 0, req.size);
    memset(ParticleMap, 0, sizeof(Particle) * N);

    struct Parameters
    {
        float w, h;
        int reserved1;
        int reserved2;
    } *params = (Parameters*)ParticleMap;
    params->w = (float)fbWidth;
    params->h = (float)fbHeight;

    mainMemory = (Particle*)&ParticleMap[16];

    DescriptorBufferInfo buf;
    buf.buffer = particleBuffer;
    buf.range = sizeof(Particle) * N + 16;

    WriteDescriptorSet ws;
    ws.dstSet = storageBufferSet;
    ws.dstBinding = 0;
    ws.descriptorCount = 1;
    ws.descriptorType = DescriptorType::eStorageBuffer;
    ws.pBufferInfo = &buf;

    D.updateDescriptorSets(1, &ws, 0, nullptr);

}

static xcb_atom_t GetAtom(const char* name)
{
    auto cookie = xcb_intern_atom(connection, 1, strlen(name), name);
    auto rep = xcb_intern_atom_reply(connection, cookie, NULL);
    if (rep->atom == 0)
    {
        printf("Warning: atom %s not found\n", name);
    }
    auto res = rep->atom;
    free(rep);
    return res;
}

static void CreateWindow(int w, int h)
{
    connection = xcb_connect (NULL, NULL);

    const xcb_setup_t *setup  = xcb_get_setup (connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
    xcb_screen_t *screen = iter.data;
    window = xcb_generate_id (connection);

    uint32_t eventsmasks[] = { XCB_EVENT_MASK_EXPOSURE 
        | XCB_EVENT_MASK_POINTER_MOTION 
        | XCB_EVENT_MASK_BUTTON_MOTION
        | XCB_EVENT_MASK_BUTTON_PRESS
        | XCB_EVENT_MASK_BUTTON_RELEASE
         };

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root
        , 0, 0, w, h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual
        , XCB_CW_EVENT_MASK, eventsmasks);

    const char* title = "ParticleEngine";
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window
        , XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8
        , strlen(title), title);

    auto protocols = GetAtom("WM_PROTOCOLS");
    auto adelete = GetAtom("WM_DELETE_WINDOW");
    

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window
        , protocols, XCB_ATOM_ATOM, 32
        , 1, &adelete);


    xcb_size_hints_t hints;
    hints.flags = 0;
    xcb_icccm_size_hints_set_min_size(&hints, w, h);
    xcb_icccm_size_hints_set_max_size(&hints, w, h);

    xcb_icccm_set_wm_size_hints(connection, window, XCB_ATOM_WM_NORMAL_HINTS, &hints);


    xcb_map_window (connection, window);

    xcb_flush (connection);

    auto geomck = xcb_get_geometry(connection, window);
    auto geom = xcb_get_geometry_reply(connection, geomck, nullptr);

    fbWidth = geom->width;
    fbHeight = geom->height;

}

static bool CreateSurface()
{
    XcbSurfaceCreateInfoKHR cf;
    cf.connection = connection;
    cf.window = window;
    SurfaceKHR surface;

    surface = vinstance.createXcbSurfaceKHR(cf);

    auto caps = main_device.getSurfaceFormatsKHR(surface);
    if (!main_device.getSurfaceSupportKHR(qfamilyIndex, surface))
    {
        fprintf(stderr, "No device support for vulkan presentation\n");
        return false;
    }
    auto pm = main_device.getSurfacePresentModesKHR(surface);
    PresentModeKHR findMode = (PresentModeKHR)0xFFFFFFFF;
    for (int i = 0; i < pm.size(); i++)
    {
        if (pm[i] == PresentModeKHR::eFifo || pm[i] == PresentModeKHR::eMailbox)
        {
            findMode = pm[i];
            break;
        }
    }

    if (findMode == (PresentModeKHR)0xFFFFFFFF)
    {
        fprintf(stderr, "No valid present mode\n");
        return false;
    }

    surfaceFormat = Format::eUndefined;

    ColorSpaceKHR csp;

    for (int i = 0; i < caps.size(); i++)
    {
        if (caps[i].format == Format::eB8G8R8A8Unorm)
        {
            surfaceFormat = Format::eB8G8R8A8Unorm;
            csp = caps[i].colorSpace;
            break;
        }
    }

    if (surfaceFormat == Format::eUndefined)
    {
        if (caps.size() == 1 
            && caps[0].format == Format::eUndefined)
        {
            surfaceFormat = Format::eB8G8R8A8Unorm;
            csp = ColorSpaceKHR::eSrgbNonlinear;
        }
        else
        {
            surfaceFormat = caps[0].format;
            csp = caps[0].colorSpace;
        }
    }

    auto caps2 = main_device.getSurfaceCapabilitiesKHR(surface);


    SwapchainCreateInfoKHR sc;
    sc.surface = surface;
    sc.minImageCount = caps2.minImageCount;
    sc.imageFormat = surfaceFormat;
    sc.imageColorSpace = csp;
    sc.imageExtent = Extent2D(fbWidth, fbHeight);
    sc.imageArrayLayers = 1;
    sc.imageUsage = ImageUsageFlagBits::eColorAttachment;
    sc.preTransform = SurfaceTransformFlagBitsKHR::eIdentity;
    sc.compositeAlpha = CompositeAlphaFlagBitsKHR::eOpaque;
    sc.presentMode = findMode;
    sc.clipped = 0;
    sc.oldSwapchain = VK_NULL_HANDLE;
    sc.imageSharingMode = SharingMode::eExclusive;
    sc.queueFamilyIndexCount = 0;

    mainChain = D.createSwapchainKHR(sc);
    

    swapImages = D.getSwapchainImagesKHR(mainChain);

    return true;
}

static void CreateDrawCommands(float r, float g, float b, int sides)
{
    int n = swapImages.size();

    CommandBufferAllocateInfo all;
    all.commandPool = pool;
    all.commandBufferCount = n;
    all.level = CommandBufferLevel::ePrimary;

    drawCommandBuffers = D.allocateCommandBuffers(all);

    std::array<float, 4> color = {r, g, b, 1.0};

    for (int i = 0; i < n; i++)
    {
        auto& c = drawCommandBuffers[i];

        c.begin(CommandBufferBeginInfo(CommandBufferUsageFlagBits::eSimultaneousUse));

        ClearValue value;
        value.color = color;

        RenderPassBeginInfo pbeg;
        pbeg.renderArea = Rect2D( Offset2D(0, 0), Extent2D(fbWidth, fbHeight) );
        pbeg.renderPass = render;
        pbeg.framebuffer = framebuffer;
        pbeg.clearValueCount = 1;
        pbeg.pClearValues = &value;


        c.beginRenderPass(pbeg, SubpassContents::eInline);
        c.bindPipeline(PipelineBindPoint::eGraphics, pipeline);
        DeviceSize offset = 0;
        c.bindVertexBuffers(0, 1, &circleGeometryDevice, &offset);
        c.bindDescriptorSets(PipelineBindPoint::eGraphics
            ,mainLayout , 0, 1, &storageBufferSet, 0, nullptr);
        c.draw(sides, N, 0, 0);

        c.endRenderPass();


        ImageMemoryBarrier layBarrier[2];
        layBarrier[0].image = swapImages[i];
        layBarrier[0].subresourceRange.aspectMask = ImageAspectFlagBits::eColor;
        layBarrier[0].subresourceRange.layerCount = 1;
        layBarrier[0].subresourceRange.levelCount = 1;
        layBarrier[0].oldLayout = ImageLayout::eUndefined;
        layBarrier[0].newLayout = ImageLayout::eTransferDstOptimal;
        layBarrier[0].dstAccessMask = AccessFlagBits::eTransferWrite;

        c.pipelineBarrier(PipelineStageFlagBits::eTopOfPipe
            , PipelineStageFlagBits::eTopOfPipe
            , DependencyFlags(), 0, nullptr, 0, nullptr
            , 1, layBarrier);

        ImageResolve region;
        region.srcSubresource.aspectMask = ImageAspectFlagBits::eColor;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = ImageAspectFlagBits::eColor;
        region.dstSubresource.layerCount = 1;
        region.extent.depth = 1;
        region.extent.width = fbWidth;
        region.extent.height = fbHeight;

        c.resolveImage(renderImage, ImageLayout::eTransferSrcOptimal, 
            swapImages[i], ImageLayout::eTransferDstOptimal, 1, &region);

        layBarrier[1].image = swapImages[i];
        layBarrier[1].subresourceRange.aspectMask = ImageAspectFlagBits::eColor;
        layBarrier[1].subresourceRange.layerCount = 1;
        layBarrier[1].subresourceRange.levelCount = 1;
        layBarrier[1].oldLayout = ImageLayout::eTransferDstOptimal;
        layBarrier[1].newLayout = ImageLayout::ePresentSrcKHR;
        layBarrier[1].srcAccessMask = AccessFlagBits::eTransferWrite;
        layBarrier[1].dstAccessMask = AccessFlagBits::eMemoryRead;

        c.pipelineBarrier(PipelineStageFlagBits::eBottomOfPipe
            , PipelineStageFlagBits::eBottomOfPipe
            , DependencyFlags(), 0, nullptr, 0, nullptr
            , 1, &layBarrier[1]);

        c.end();
    }
}

static int Visible;

static bool PollEvents()
{
    auto st = GetAtom("WM_STATE");
    auto hid = GetAtom("_NET_WM_STATE_HIDDEN");

    auto hsck = xcb_get_property(connection, 0, window, st, st, 0, 1);

    xcb_generic_event_t* ev = xcb_poll_for_event(connection);
    while (ev != NULL)
    {
        switch (ev->response_type & ~0x80)
        {
            case XCB_EXPOSE:
            
            break;
            case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t* msg;
                msg = (xcb_client_message_event_t*)ev;

                auto adelete = GetAtom("WM_DELETE_WINDOW");

                if (*msg->data.data32 == adelete)
                    return false;

            }
            break;
        }

        ev = xcb_poll_for_event(connection);
    }

    xcb_get_property_reply_t* prop = xcb_get_property_reply(connection, hsck, nullptr);
    if (xcb_get_property_value_length(prop) != 0)
    {
        uint32_t* value = (uint32_t*)xcb_get_property_value(prop);
        if (*value == 3)
            Visible = 0;
        else
            Visible = 1;
    }

    free(prop);

    return true;
}

static void Draw(Semaphore sem, Semaphore end, uint32_t idx)
{
    PipelineStageFlags waitStage = PipelineStageFlagBits::eTopOfPipe;

    SubmitInfo su;
    su.pCommandBuffers = &drawCommandBuffers[idx];
    su.commandBufferCount = 1;
    su.pWaitSemaphores = &sem;
    su.waitSemaphoreCount = 1;
    su.signalSemaphoreCount = 1;
    su.pSignalSemaphores = &end;
    su.pWaitDstStageMask = &waitStage;

    queue.submit(1, &su, nullptr);
}

static void Present()
{
    static Fence vysnfence = VK_NULL_HANDLE;
    static Semaphore acquireSem = VK_NULL_HANDLE;
    static Semaphore renderEndSem = VK_NULL_HANDLE;
    if (!vysnfence)
    {
        vysnfence = D.createFence(FenceCreateInfo()) ;
    }

    if (!acquireSem)
    {
        acquireSem = D.createSemaphore(SemaphoreCreateInfo());
    }

    if (!renderEndSem)
    {
        renderEndSem = D.createSemaphore(SemaphoreCreateInfo());
    }


    uint32_t image;
    D.acquireNextImageKHR(mainChain, 0xFFFFFFFFFFFFFFFF, acquireSem
        , vysnfence, &image);

    D.waitForFences(1, &vysnfence, true, 0xFFFFFFFFFFFFFFFF);

    D.resetFences(1, &vysnfence);
    Draw(acquireSem, renderEndSem, image);

    PresentInfoKHR pre;
    pre.swapchainCount = 1;
    pre.pSwapchains = &mainChain;
    pre.pImageIndices = &image;
    pre.pWaitSemaphores = &renderEndSem;
    pre.waitSemaphoreCount = 1;
    queue.presentKHR(pre);
}

static int exitCode;
static std::thread engineThread;
static std::condition_variable finishLoadCondition;
static std::mutex finishLoadConditionMutex;
static int closeEngine;

static void ThreadCreateEngine(int n, int w,int h, int s
    , float r, float g, float b
    , CloseFunc close)
{
    N = n;
    CreateInstance();
    if (!CreateDevice())
    {
        exitCode = -1;
    }
    CreateCommandQueue();
    CreateWindow(w, h);
    
    if (!CreateSurface())
    {
        exitCode = -1;
    }
    CreateRenderPass();
    CreatePipeline();
    CreateFramebuffer();
    CreateGeometry(s);
    CreateParticlesBuffer();
    CreateDrawCommands(r, g, b, s + 2);

    exitCode = 1;
    finishLoadCondition.notify_all();

    while(closeEngine == 0)
    {
        
        if (!PollEvents())
            break;

        if (Visible)
            Present();
    }    

    xcb_destroy_window(connection, window);
    xcb_disconnect (connection);


    if (close)
    {
        close();
    }
}

Particle* CreateEngine(int n, int w,int h
    ,CloseFunc close
    , int s, float r, float g, float b)
{
    closeEngine = 0;
    finishLoadCondition.notify_all();
    auto func = std::bind(ThreadCreateEngine, n, w, h, s, r, g, b, close);
    engineThread = std::move(std::thread(func));
    std::unique_lock<std::mutex> lock(finishLoadConditionMutex);
    finishLoadCondition.wait(lock);

    if (exitCode == 1)
        return mainMemory;
    else
        return nullptr;
}

void CloseEngine()
{
    closeEngine = 1;
    engineThread.join();
}

