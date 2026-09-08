#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace rfl {
struct Texture {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    unsigned width=0,height=0;
};
Texture loadHelpImage(ID3D11Device* device);
}
