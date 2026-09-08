#include "images.hpp"
#include <wincodec.h>
#include <vector>
#include <stdexcept>
namespace rfl {
Texture loadHelpImage(ID3D11Device* device) {
    using Microsoft::WRL::ComPtr;
    auto instance=GetModuleHandleW(nullptr);auto resource=FindResourceW(instance,MAKEINTRESOURCEW(201),RT_RCDATA);
    if(!resource)throw std::runtime_error("Help image is unavailable");
    auto memory=LoadResource(instance,resource);auto bytes=static_cast<BYTE*>(LockResource(memory));auto size=SizeofResource(instance,resource);
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    auto require=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Could not load the help image");};
    require(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    require(factory->CreateStream(&stream));require(stream->InitializeFromMemory(bytes,size));
    require(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));require(decoder->GetFrame(0,&frame));
    Texture result;require(frame->GetSize(&result.width,&result.height));require(factory->CreateFormatConverter(&converter));
    require(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<BYTE> pixels(size_t(result.width)*result.height*4);require(converter->CopyPixels(nullptr,result.width*4,UINT(pixels.size()),pixels.data()));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=result.width;desc.Height=result.height;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),result.width*4,0};ComPtr<ID3D11Texture2D> texture;
    require(device->CreateTexture2D(&desc,&data,&texture));require(device->CreateShaderResourceView(texture.Get(),nullptr,&result.view));return result;
}
}
