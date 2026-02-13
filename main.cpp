#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>

#define VK_CHECK(x) do { VkResult err = x; if (err != VK_SUCCESS) throw std::runtime_error("Vulkan error at " #x); } while(0)

// Load SPIR-V binary
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

int main() {
    const int N = 16;

    // 1️⃣ Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ComputeTest";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCI{};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pApplicationInfo = &appInfo;

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));

    // 2️⃣ GPU
    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data()));
    VkPhysicalDevice gpu = gpus[0];

    // 3️⃣ Queue
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, qProps.data());

    int computeIndex = -1;
    for (uint32_t i = 0; i < qProps.size(); ++i)
        if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { computeIndex = i; break; }

    float qp = 1.0f;
    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.queueFamilyIndex = computeIndex;
    queueCI.queueCount = 1;
    queueCI.pQueuePriorities = &qp;

    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.queueCreateInfoCount = 1;
    deviceCI.pQueueCreateInfos = &queueCI;

    VkDevice device;
    VK_CHECK(vkCreateDevice(gpu, &deviceCI, nullptr, &device));

    VkQueue queue;
    vkGetDeviceQueue(device, computeIndex, 0, &queue);

    // 4️⃣ Buffer
    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = sizeof(uint32_t) * N;
    bufferCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    VK_CHECK(vkCreateBuffer(device, &bufferCI, nullptr, &buffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

    uint32_t memType = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((memReq.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memType = i;
            break;
        }

    VkMemoryAllocateInfo memAI{};
    memAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAI.allocationSize = memReq.size;
    memAI.memoryTypeIndex = memType;

    VkDeviceMemory bufferMem;
    VK_CHECK(vkAllocateMemory(device, &memAI, nullptr, &bufferMem));
    VK_CHECK(vkBindBufferMemory(device, buffer, bufferMem, 0));

    // 5️⃣ Shader
    auto shaderCode = readFile("shader.spv");
    VkShaderModuleCreateInfo shaderModuleCI{};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCI.codeSize = shaderCode.size();
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shader;
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shader));

    // 6️⃣ Descriptor
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslCI{};
    dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCI.bindingCount = 1;
    dslCI.pBindings = &layoutBinding;

    VkDescriptorSetLayout dsl;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &dslCI, nullptr, &dsl));

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &dsl;

    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCI{};
    computePipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCI.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineCI.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCI.stage.module = shader;
    computePipelineCI.stage.pName = "main";
    computePipelineCI.layout = pipelineLayout;

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));

    // 7️⃣ Descriptor pool & set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes = &poolSize;
    poolCI.maxSets = 1;

    VkDescriptorPool descriptorPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &dsl;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = buffer;
    bufInfo.offset = 0;
    bufInfo.range = bufferCI.size;

    VkWriteDescriptorSet writeDS{};
    writeDS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDS.dstSet = descriptorSet;
    writeDS.dstBinding = 0;
    writeDS.descriptorCount = 1;
    writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDS.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(device, 1, &writeDS, 0, nullptr);

    // 8️⃣ Command pool & buffer
    VkCommandPoolCreateInfo cmdPoolCI{};
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.queueFamilyIndex = computeIndex;

    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolCI, nullptr, &cmdPool));

    VkCommandBufferAllocateInfo cmdBufAI{};
    cmdBufAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAI.commandPool = cmdPool;
    cmdBufAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAI.commandBufferCount = 1;

    VkCommandBuffer cmdBuf;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufAI, &cmdBuf));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo));

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmdBuf, N, 1, 1);

    VK_CHECK(vkEndCommandBuffer(cmdBuf));

    // 9️⃣ Submit & wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

    // 10️⃣ Read back
    void* data;
    VK_CHECK(vkMapMemory(device, bufferMem, 0, bufferCI.size, 0, &data));
    uint32_t* out = static_cast<uint32_t*>(data);

    std::cout << "GPU Output: ";
    for (int i = 0; i < N; ++i) std::cout << out[i] << " ";
    std::cout << "\n";

    vkUnmapMemory(device, bufferMem);

    // Cleanup
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, dsl, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkFreeMemory(device, bufferMem, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
