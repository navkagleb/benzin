#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/copy_to_back_buffer_pass.hpp"

#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>

#include "sandbox/resources.hpp"

namespace sandbox
{

    void CopyToBackBufferPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "BackBufferCopy");

        const auto& currentBackBuffer = ms_SwapChain->GetCurrentBackBuffer();
        const auto& imGuiTexture = ms_Resources->GetTexture(+Texture::ImGui);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination },
            benzin::TransitionBarrier{ imGuiTexture, benzin::ResourceState::CopySource },
        );

        commandList.CopyResource(currentBackBuffer, imGuiTexture);
    }

}
