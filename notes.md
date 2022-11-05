buffer_ro - ConstantBufferView | ShaderResourceView
buffer_rw - UnorderedAccessView

texture_ro - ShaderResourceView
texture_rw - UnorderedAccessView
texture_rt - RenderTargetView
texture_ds - DepthStencilView

buffer_resource
texture_resource

buffer
    buffer_resource
    buffer_views

texture
    texture_resource
    texture_views
    