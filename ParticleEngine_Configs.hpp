#include "ParticleEngine_Sizes.hpp"

static const char* extensions[] = {"VK_EXT_debug_report", "VK_KHR_xcb_surface", "VK_KHR_surface"};
static const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
static const char* dev_extensions[] = {"VK_KHR_swapchain"};

#ifdef _DEBUG
constexpr uint32_t layno = sizeof(layers)/sizeof(void*);
#else
constexpr uint32_t layno = 0;
#endif

constexpr uint32_t extno = sizeof(extensions)/sizeof(void*);
constexpr uint32_t devextno = sizeof(dev_extensions)/sizeof(void*);


extern void* _binary_vert_spv_size;
extern void* _binary_vert_spv_start;
extern void* _binary_frag_spv_size;
extern void* _binary_frag_spv_start;

static constexpr uint32_t Modules()
{
    return 2;
}

static std::vector<uint32_t> ModuleSizes()
{
    std::vector<uint32_t> sizes;
    sizes.push_back(VERT_SIZE);
    sizes.push_back(FRAG_SIZE);

    return std::move(sizes);
}


static std::vector<uint32_t*> ModulesData()
{
    std::vector<uint32_t*> datas;
    datas.push_back((uint32_t*)&_binary_vert_spv_start);
    datas.push_back((uint32_t*)&_binary_frag_spv_start);

    return std::move(datas);
}

static std::vector<ShaderStageFlagBits> ModulesStage()
{
    std::vector<ShaderStageFlagBits> stages;
    stages.push_back(ShaderStageFlagBits::eVertex);
    stages.push_back(ShaderStageFlagBits::eFragment);

    return std::move(stages);
}

