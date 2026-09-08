#include "ui.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <fstream>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT trayEvent=WM_APP+20,showEvent=WM_APP+21;
constexpr wchar_t className[]=L"ReadyForLaunch.MainWindow";
rfl::Ui* ui=nullptr;
ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swapchain;ComPtr<ID3D11RenderTargetView> view;
bool hidden=false,resize=false;float scale=1;
void createView() {
    ComPtr<ID3D11Texture2D> texture;swapchain->GetBuffer(0,IID_PPV_ARGS(&texture));
    if(!texture||FAILED(device->CreateRenderTargetView(texture.Get(),nullptr,&view)))throw std::runtime_error("Could not create display surface");
}
void graphics(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL feature;
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,&feature,&context);
    if(FAILED(result))result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,&feature,&context);
    if(FAILED(result))throw std::runtime_error("Direct3D could not initialise");createView();
}
void capture(const std::filesystem::path& path) {
    ComPtr<ID3D11Texture2D> back,staging;swapchain->GetBuffer(0,IID_PPV_ARGS(&back));D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    HRESULT hr=device->CreateTexture2D(&desc,nullptr,&staging);if(FAILED(hr))throw std::runtime_error("Capture texture failed");context->CopyResource(staging.Get(),back.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))throw std::runtime_error("Capture readback failed");
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    std::filesystem::create_directories(path.parent_path());
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);if(SUCCEEDED(hr))hr=encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,nullptr);if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);if(SUCCEEDED(hr))hr=frame->SetSize(desc.Width,desc.Height);
    ComPtr<IWICBitmap> bitmap;ComPtr<IWICFormatConverter> converter;
    if(SUCCEEDED(hr))hr=factory->CreateBitmapFromMemory(desc.Width,desc.Height,GUID_WICPixelFormat32bppRGBA,mapped.RowPitch,mapped.RowPitch*desc.Height,static_cast<BYTE*>(mapped.pData),&bitmap);
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(bitmap.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr))hr=frame->WriteSource(converter.Get(),nullptr);
    if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();context->Unmap(staging.Get(),0);
    if(FAILED(hr))throw std::runtime_error("Could not save UI capture");
}
void show(HWND window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);hidden=false;}
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(window,message,w,l))return 1;
    switch(message) {
    case WM_SIZE:resize=w!=SIZE_MINIMIZED;return 0;
    case WM_GETMINMAXINFO:{auto limits=reinterpret_cast<MINMAXINFO*>(l);limits->ptMinTrackSize={LONG(1040*scale),LONG(660*scale)};return 0;}
    case WM_DPICHANGED:{auto bounds=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,bounds->left,bounds->top,bounds->right-bounds->left,bounds->bottom-bounds->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
    case WM_CLOSE:if(ui&&ui->active()){ShowWindow(window,SW_HIDE);hidden=true;}else DestroyWindow(window);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case showEvent:show(window);return 0;
    case trayEvent:
        if(l==WM_LBUTTONUP||l==WM_LBUTTONDBLCLK)show(window);
        if(l==WM_RBUTTONUP) {
            auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Open ReadyForLaunch");AppendMenuW(menu,MF_STRING,2,L"Stop session");
            if(!ui||!ui->active())AppendMenuW(menu,MF_STRING,4,L"Exit");POINT cursor;GetCursorPos(&cursor);SetForegroundWindow(window);
            auto action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,cursor.x,cursor.y,0,window,nullptr);DestroyMenu(menu);
            if(action==1)show(window);if(action==2&&ui)ui->stop();if(action==4)DestroyWindow(window);
        }return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=0;bool unattended=false;
    try {
        auto directory=rfl::dataDirectory();std::filesystem::path screenshot;bool preview=false;std::string captureView;float captureScale=0;
        int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
        for(int i=1;i<count;++i) {
            if(std::wstring(args[i])==L"--data-dir"&&i+1<count)directory=args[++i];
            else if(std::wstring(args[i])==L"--capture"&&i+1<count){screenshot=args[++i];unattended=true;}
            else if(std::wstring(args[i])==L"--capture-view"&&i+1<count)captureView=rfl::utf8(args[++i]);
            else if(std::wstring(args[i])==L"--capture-scale"&&i+1<count)captureScale=std::clamp(wcstof(args[++i],nullptr),1.f,2.5f);
            else if(std::wstring(args[i])==L"--preview")preview=true;
        }LocalFree(args);if(screenshot.empty())captureScale=0;
        HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\ReadyForLaunch_v1");
        if(screenshot.empty()&&mutex&&GetLastError()==ERROR_ALREADY_EXISTS){auto other=FindWindowW(className,nullptr);if(other)PostMessageW(other,showEvent,0,0);CloseHandle(mutex);CoUninitialize();return 0;}
        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=className;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,16,16,LR_DEFAULTCOLOR));RegisterClassExW(&wc);
        auto window=CreateWindowExW(0,className,L"ReadyForLaunch Alpha — by Adam Chesters",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1100,890,nullptr,nullptr,instance,nullptr);
        if(!window)throw std::runtime_error("Window creation failed");graphics(window);
        IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",19);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/georgiab.ttf",34);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisb.ttf",19);
        rfl::applyTheme();scale=captureScale?captureScale:float(GetDpiForWindow(window))/96;ImGui::GetStyle().FontScaleDpi=scale;ImGui::GetStyle().ScaleAllSizes(scale);
        if(scale!=1){int width=int(1100*scale),height=int(890*scale);if(screenshot.empty()){MONITORINFO monitor{sizeof(monitor)};if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)){width=std::min(width,int(monitor.rcWork.right-monitor.rcWork.left));height=std::min(height,int(monitor.rcWork.bottom-monitor.rcWork.top));}}SetWindowPos(window,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}
        ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device.Get(),context.Get());
        NOTIFYICONDATAW tray{};tray.cbSize=sizeof(tray);tray.hWnd=window;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=trayEvent;tray.hIcon=wc.hIcon;wcscpy_s(tray.szTip,L"ReadyForLaunch");
        if(screenshot.empty())Shell_NotifyIconW(NIM_ADD,&tray);
        {
            rfl::Ui app(window,device.Get(),directory,preview,!screenshot.empty()||preview);app.captureView(captureView);ui=&app;if(screenshot.empty())show(window);
            bool quit=false;int frames=0;ULONGLONG lastTick=0,captureStart=GetTickCount64();
            while(!quit) {
                MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)quit=true;TranslateMessage(&msg);DispatchMessageW(&msg);}if(quit)break;
                auto now=GetTickCount64();if(now-lastTick>=100){app.tick();lastTick=now;}
                if((hidden||IsIconic(window))&&screenshot.empty()){MsgWaitForMultipleObjects(0,nullptr,FALSE,100,QS_ALLINPUT);continue;}
                if(resize){view.Reset();swapchain->ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0);createView();resize=false;}
                float nextScale=captureScale?captureScale:float(GetDpiForWindow(window))/96;if(nextScale!=scale){rfl::applyTheme();scale=nextScale;ImGui::GetStyle().ScaleAllSizes(scale);ImGui::GetStyle().FontScaleDpi=scale;}
                ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();app.render();ImGui::Render();
                auto target=view.Get();context->OMSetRenderTargets(1,&target,nullptr);const float color[]={.025f,.029f,.034f,1};context->ClearRenderTargetView(view.Get(),color);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                if(!screenshot.empty()&&++frames>=5&&GetTickCount64()-captureStart>1200){capture(screenshot);break;}
                HRESULT present=swapchain->Present(1,0);if(FAILED(present))throw std::runtime_error("Display device lost. Session processes have been left running.");
                if(present==DXGI_STATUS_OCCLUDED)MsgWaitForMultipleObjects(0,nullptr,FALSE,100,QS_ALLINPUT);
            }ui=nullptr;
        }
        if(screenshot.empty())Shell_NotifyIconW(NIM_DELETE,&tray);
        ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();view.Reset();swapchain.Reset();context.Reset();device.Reset();
        if(IsWindow(window))DestroyWindow(window);UnregisterClassW(className,instance);if(mutex)CloseHandle(mutex);
    } catch(const std::exception& error) {
        // Also write a file so unattended capture/build verification never depends on a modal dialog.
        auto path=std::filesystem::temp_directory_path()/L"ReadyForLaunch-error.txt";std::ofstream(path)<<error.what();
        OutputDebugStringW(rfl::wide(error.what()).c_str());if(!unattended)MessageBoxW(nullptr,rfl::wide(error.what()).c_str(),L"ReadyForLaunch",MB_OK|MB_ICONERROR);result=1;
    }
    CoUninitialize();return result;
}
