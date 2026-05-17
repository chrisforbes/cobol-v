#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
#include <stdexcept>

// Minimal Vector and Matrix Math Structures for the 3D Perspective Rotation
struct Vec3 { float x, y, z; };
struct Matrix4 {
    float m[16];
    static Matrix4 identity() {
        Matrix4 res = {};
        res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
        return res;
    }
    static Matrix4 rotationX(float angle) {
        Matrix4 res = identity();
        float c = cosf(angle);
        float s = sinf(angle);
        res.m[5] = c;   res.m[6] = s;
        res.m[9] = -s;  res.m[10] = c;
        return res;
    }
    static Matrix4 rotationY(float angle) {
        Matrix4 res = identity();
        float c = cosf(angle);
        float s = sinf(angle);
        res.m[0] = c;   res.m[2] = -s;
        res.m[8] = s;   res.m[10] = c;
        return res;
    }
    static Matrix4 translation(float x, float y, float z) {
        Matrix4 res = identity();
        res.m[12] = x; res.m[13] = y; res.m[14] = z;
        return res;
    }
    static Matrix4 perspective(float fovDeg, float aspect, float n, float f) {
        Matrix4 res = {};
        float fovRad = fovDeg * 3.14159265f / 180.0f;
        float g = 1.0f / tanf(fovRad * 0.5f);
        res.m[0] = g / aspect;
        res.m[5] = -g; // Vulkan Y is downwards
        res.m[10] = f / (n - f);
        res.m[11] = -1.0f;
        res.m[14] = (n * f) / (n - f);
        return res;
    }
    Matrix4 operator*(const Matrix4& o) const {
        Matrix4 res = {};
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[c * 4 + r] =
                    m[0 * 4 + r] * o.m[c * 4 + 0] +
                    m[1 * 4 + r] * o.m[c * 4 + 1] +
                    m[2 * 4 + r] * o.m[c * 4 + 2] +
                    m[3 * 4 + r] * o.m[c * 4 + 3];
            }
        }
        return res;
    }
};

// Vertex Structure matching COBOL-V Vertex Layout
struct Vertex {
    float pos[3];
    float color[3];
};

const Vertex vertices[] = {
    // Front face
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},
    // Back face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0, // front
    1, 5, 6, 6, 2, 1, // right
    5, 4, 7, 7, 6, 5, // back
    4, 0, 3, 3, 7, 4, // left
    3, 2, 6, 6, 7, 3, // top
    4, 5, 1, 1, 0, 4  // bottom
};

bool shouldQuit = false;
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        shouldQuit = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

std::string getExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

// Robust File Loader looking next to the executable first, then current directories
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        file.open(getExecutableDir() + "\\" + filename, std::ios::ate | std::ios::binary);
    }
    if (!file.is_open()) {
        file.open(getExecutableDir() + "/" + filename, std::ios::ate | std::ios::binary);
    }
    if (!file.is_open()) {
        file.open("demo/" + filename, std::ios::ate | std::ios::binary);
    }
    if (!file.is_open()) {
        file.open("../demo/" + filename, std::ios::ate | std::ios::binary);
    }
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename + " (Checked CWD, Executable Dir, and demo/)");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

int main() {
    try {
        std::cout << "Initializing native Win32 + Vulkan rotating cube demo..." << std::endl;

        // 1. Create native Win32 Window
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = "COBOL_V_DemoClass";
        RegisterClassExA(&wc);

        HWND hwnd = CreateWindowExA(
            0, wc.lpszClassName, "COBOL-V 3D Vulkan Cube Demo",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            100, 100, 800, 600,
            NULL, NULL, wc.hInstance, NULL
        );

        if (!hwnd) throw std::runtime_error("failed to create Win32 window!");

        // 2. Initialize Vulkan Instance
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "COBOL-V Cube Demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_1;

        std::vector<const char*> extensions = { "VK_KHR_surface", "VK_KHR_win32_surface" };
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkInstance instance;
        if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create Vulkan instance!");
        }

        // 3. Create Win32 Surface
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.hwnd = hwnd;
        surfaceCreateInfo.hinstance = wc.hInstance;

        VkSurfaceKHR surface;
        if (vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create Win32 Vulkan surface!");
        }

        // 4. Select Physical Device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
        if (deviceCount == 0) throw std::runtime_error("failed to find GPUs with Vulkan support!");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        VkPhysicalDevice physicalDevice = devices[0];

        // 5. Get Queue Family Index
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamilyIndex = 9999;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                graphicsQueueFamilyIndex = i;
                break;
            }
        }
        if (graphicsQueueFamilyIndex == 9999) throw std::runtime_error("failed to find a graphics + present queue family!");

        // 6. Create Logical Device
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> devExts = { "VK_KHR_swapchain", "VK_KHR_shader_non_semantic_info" };
        VkDeviceCreateInfo devCreateInfo = {};
        devCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCreateInfo.queueCreateInfoCount = 1;
        devCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        devCreateInfo.enabledExtensionCount = (uint32_t)devExts.size();
        devCreateInfo.ppEnabledExtensionNames = devExts.data();

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &devCreateInfo, NULL, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

        // 7. Create Swapchain
        VkSwapchainCreateInfoKHR swapInfo = {};
        swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapInfo.surface = surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapInfo.imageExtent = { 800, 600 };
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        if (vkCreateSwapchainKHR(device, &swapInfo, NULL, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swapchain!");
        }

        uint32_t swapImageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, NULL);
        std::vector<VkImage> swapImages(swapImageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, swapImages.data());

        std::vector<VkImageView> swapImageViews(swapImageCount);
        for (uint32_t i = 0; i < swapImageCount; i++) {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            vkCreateImageView(device, &viewInfo, NULL, &swapImageViews[i]);
        }

        // 8. Create Depth Image for 3D Perspective Depth Testing
        VkImageCreateInfo depthImgInfo = {};
        depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImgInfo.format = VK_FORMAT_D32_SFLOAT;
        depthImgInfo.extent = { 800, 600, 1 };
        depthImgInfo.mipLevels = 1;
        depthImgInfo.arrayLayers = 1;
        depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImage depthImage;
        vkCreateImage(device, &depthImgInfo, NULL, &depthImage);

        VkMemoryRequirements depthMemReq;
        vkGetImageMemoryRequirements(device, depthImage, &depthMemReq);
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        VkMemoryAllocateInfo depthAlloc = {};
        depthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAlloc.allocationSize = depthMemReq.size;
        depthAlloc.memoryTypeIndex = findMemoryType(memProperties, depthMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory depthImageMemory;
        vkAllocateMemory(device, &depthAlloc, NULL, &depthImageMemory);
        vkBindImageMemory(device, depthImage, depthImageMemory, 0);

        VkImageViewCreateInfo depthViewInfo = {};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VkImageView depthImageView;
        vkCreateImageView(device, &depthViewInfo, NULL, &depthImageView);

        // 9. Create Render Pass
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthAttachmentRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = (uint32_t)attachments.size();
        rpInfo.pAttachments = attachments.data();
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;

        VkRenderPass renderPass;
        vkCreateRenderPass(device, &rpInfo, NULL, &renderPass);

        // 10. Create Swapchain Framebuffers
        std::vector<VkFramebuffer> framebuffers(swapImageCount);
        for (uint32_t i = 0; i < swapImageCount; i++) {
            std::vector<VkImageView> fbAttachments = { swapImageViews[i], depthImageView };
            VkFramebufferCreateInfo fbInfo = {};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = renderPass;
            fbInfo.attachmentCount = (uint32_t)fbAttachments.size();
            fbInfo.pAttachments = fbAttachments.data();
            fbInfo.width = 800;
            fbInfo.height = 600;
            fbInfo.layers = 1;
            vkCreateFramebuffer(device, &fbInfo, NULL, &framebuffers[i]);
        }

        // 11. Create Vertex & Index Buffers containing 3D Cube geometry
        VkBufferCreateInfo vBufferInfo = {};
        vBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vBufferInfo.size = sizeof(vertices);
        vBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer vertexBuffer;
        vkCreateBuffer(device, &vBufferInfo, NULL, &vertexBuffer);

        VkMemoryRequirements vMemReq;
        vkGetBufferMemoryRequirements(device, vertexBuffer, &vMemReq);
        VkMemoryAllocateInfo vAlloc = {};
        vAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vAlloc.allocationSize = vMemReq.size;
        vAlloc.memoryTypeIndex = findMemoryType(memProperties, vMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory vertexBufferMemory;
        vkAllocateMemory(device, &vAlloc, NULL, &vertexBufferMemory);
        vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, sizeof(vertices), 0, &vData);
        memcpy(vData, vertices, sizeof(vertices));
        vkUnmapMemory(device, vertexBufferMemory);

        // Index Buffer Setup
        VkBufferCreateInfo iBufferInfo = {};
        iBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        iBufferInfo.size = sizeof(indices);
        iBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        iBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer indexBuffer;
        vkCreateBuffer(device, &iBufferInfo, NULL, &indexBuffer);

        VkMemoryRequirements iMemReq;
        vkGetBufferMemoryRequirements(device, indexBuffer, &iMemReq);
        VkMemoryAllocateInfo iAlloc = {};
        iAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        iAlloc.allocationSize = iMemReq.size;
        iAlloc.memoryTypeIndex = findMemoryType(memProperties, iMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory indexBufferMemory;
        vkAllocateMemory(device, &iAlloc, NULL, &indexBufferMemory);
        vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

        void* iData;
        vkMapMemory(device, indexBufferMemory, 0, sizeof(indices), 0, &iData);
        memcpy(iData, indices, sizeof(indices));
        vkUnmapMemory(device, indexBufferMemory);

        // 12. Create Shader Modules from custom COBOL-V shader compiled spv files
        std::vector<char> vsCode = readFile("cube_vs.spv");
        std::vector<char> fsCode = readFile("cube_fs.spv");

        VkShaderModuleCreateInfo vsInfo = {};
        vsInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vsInfo.codeSize = vsCode.size();
        vsInfo.pCode = reinterpret_cast<const uint32_t*>(vsCode.data());
        VkShaderModule vsModule;
        vkCreateShaderModule(device, &vsInfo, NULL, &vsModule);

        VkShaderModuleCreateInfo fsInfo = {};
        fsInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fsInfo.codeSize = fsCode.size();
        fsInfo.pCode = reinterpret_cast<const uint32_t*>(fsCode.data());
        VkShaderModule fsModule;
        vkCreateShaderModule(device, &fsInfo, NULL, &fsModule);

        // 13. Setup Pipelines with 64-byte push constant range for MVP matrix
        VkPushConstantRange pushRange = {};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(Matrix4);

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(device, &layoutInfo, NULL, &pipelineLayout);

        // Pipeline States
        VkPipelineShaderStageCreateInfo vsStage = {};
        vsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsStage.module = vsModule;
        vsStage.pName = "MAIN";

        VkPipelineShaderStageCreateInfo fsStage = {};
        fsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fsStage.module = fsModule;
        fsStage.pName = "MAIN";

        VkPipelineShaderStageCreateInfo stages[] = { vsStage, fsStage };

        VkVertexInputBindingDescription bindDesc = {};
        bindDesc.binding = 0;
        bindDesc.stride = sizeof(Vertex);
        bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrDesc[2] = {};
        attrDesc[0].binding = 0;
        attrDesc[0].location = 0;
        attrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrDesc[0].offset = offsetof(Vertex, pos);

        attrDesc[1].binding = 0;
        attrDesc[1].location = 1;
        attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrDesc[1].offset = offsetof(Vertex, color);

        VkPipelineVertexInputStateCreateInfo vInputState = {};
        vInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vInputState.vertexBindingDescriptionCount = 1;
        vInputState.pVertexBindingDescriptions = &bindDesc;
        vInputState.vertexAttributeDescriptionCount = 2;
        vInputState.pVertexAttributeDescriptions = attrDesc;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = { 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {800, 600} };
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Keep simple to avoid winding issues
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &blendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vInputState;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // 14. Command Pool and Sync Structures Setup
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool commandPool;
        vkCreateCommandPool(device, &poolInfo, NULL, &commandPool);

        VkCommandBufferAllocateInfo cbAllocInfo = {};
        cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAllocInfo.commandPool = commandPool;
        cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cbAllocInfo, &commandBuffer);

        VkSemaphoreCreateInfo semInfo = {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        vkCreateSemaphore(device, &semInfo, NULL, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &semInfo, NULL, &renderFinishedSemaphore);

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence inFlightFence;
        vkCreateFence(device, &fenceInfo, NULL, &inFlightFence);

        std::cout << "Initialization complete! Entering draw render loop..." << std::endl;

        // 15. Render Draw Loop with real-time perspective matrix calculations
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!shouldQuit) {
            MSG msg;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }

            if (shouldQuit) break;

            // Wait for previous frame completion
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            // Acquire Swapchain Image
            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swapchain image!");
            }

            // Calculate rotating Perspective Projection MVP matrix
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            Matrix4 model = Matrix4::rotationX(time * 0.6f) * Matrix4::rotationY(time * 0.9f);
            Matrix4 view = Matrix4::translation(0.0f, 0.0f, -2.5f);
            Matrix4 proj = Matrix4::perspective(45.0f, 800.0f / 600.0f, 0.1f, 10.0f);
            Matrix4 mvp = proj * view * model;

            // Record Render Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkRenderPassBeginInfo rpBegin = {};
            rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass = renderPass;
            rpBegin.framebuffer = framebuffers[imageIndex];
            rpBegin.renderArea.offset = { 0, 0 };
            rpBegin.renderArea.extent = { 800, 600 };

            VkClearValue clearValues[2] = {};
            clearValues[0].color = { {0.05f, 0.05f, 0.12f, 1.0f} }; // Harmonic slate blue
            clearValues[1].depthStencil = { 1.0f, 0 };
            rpBegin.clearValueCount = 2;
            rpBegin.pClearValues = clearValues;

            vkCmdBeginRenderPass(commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Push MVP Matrix Constants to COBOL-V Vertex Shader
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix4), &mvp);

            // Draw Index Cube Geometry
            vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);

            vkCmdEndRenderPass(commandBuffer);
            vkEndCommandBuffer(commandBuffer);

            // Submit Graphics Queue Work
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            // Present Image to Swaphchain on Display
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);

            // Frame Delay to prevent GPU starvation
            Sleep(8);
        }

        // Cleanup resources
        vkDeviceWaitIdle(device);
        vkDestroyFence(device, inFlightFence, NULL);
        vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
        vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
        vkDestroyCommandPool(device, commandPool, NULL);
        vkDestroyBuffer(device, indexBuffer, NULL);
        vkFreeMemory(device, indexBufferMemory, NULL);
        vkDestroyBuffer(device, vertexBuffer, NULL);
        vkFreeMemory(device, vertexBufferMemory, NULL);
        vkDestroyPipeline(device, graphicsPipeline, NULL);
        vkDestroyPipelineLayout(device, pipelineLayout, NULL);
        vkDestroyShaderModule(device, fsModule, NULL);
        vkDestroyShaderModule(device, vsModule, NULL);
        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, NULL);
        vkDestroyRenderPass(device, renderPass, NULL);
        vkDestroyImageView(device, depthImageView, NULL);
        vkDestroyImage(device, depthImage, NULL);
        vkFreeMemory(device, depthImageMemory, NULL);
        for (auto view : swapImageViews) vkDestroyImageView(device, view, NULL);
        vkDestroySwapchainKHR(device, swapchain, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        std::cout << "Vulkan resources released cleanly. Exiting application successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception occurred: " << e.what() << std::endl;
        MessageBoxA(NULL, e.what(), "Fatal Error", MB_ICONERROR);
        return 1;
    }
    return 0;
}
