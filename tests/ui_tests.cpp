#include "ui.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <iostream>
#include <stdexcept>
using namespace rfl;
namespace rfl {
struct UiTestAccess {
    static Settings& settings(Ui& ui){return ui.settings_;}
    static void renameText(Ui& ui){ui.actionName_="Renamed tab";}
};
}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main() {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {
        Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP renderer available");
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1084,851};io.DeltaTime=1.f/60;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",19);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/georgiab.ttf",34);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisb.ttf",19);
        applyTheme();ImGui_ImplDX11_Init(device.Get(),context.Get());
        {
            Ui ui(nullptr,device.Get(),std::filesystem::temp_directory_path()/L"ReadyForLaunch-ui-check",true,true);
            auto& settings=UiTestAccess::settings(ui);settings.profiles[0].name=settings.profiles[1].name="Same name";
            auto a=duplicate(settings.profiles[0],"New profile"),b=duplicate(settings.profiles[0],"New profile");
            const auto secondCustom=b.id;settings.profiles.push_back(a);settings.profiles.push_back(b);
            auto frame=[&] {ImGui_ImplDX11_NewFrame();io.DisplaySize={1084,851};io.DeltaTime=1.f/60;ImGui::NewFrame();ui.render();ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());};
            auto hover=[&](float x,float y){io.AddMousePosEvent(x,y);frame();frame();frame();check(ImGui::GetCurrentContext()->HoveredIdPreviousFrameItemCount<=1,"no duplicate visible ImGui IDs on hover");return ImGui::GetCurrentContext()->HoveredId;};
            auto click=[&](int button){io.AddMouseButtonEvent(button,true);frame();io.AddMouseButtonEvent(button,false);frame();frame();};
            frame();frame();auto first=hover(80,120),second=hover(205,120);
            check(first&&second&&first!=second,"same-named built-in tabs have distinct interactive IDs");
            hover(80,120);click(1);ImGuiWindow* menu=nullptr;
            for(auto window:ImGui::GetCurrentContext()->Windows)if(window->Active&&(window->Flags&ImGuiWindowFlags_Popup))menu=window;
            check(menu!=nullptr,"right-click opens profile menu");ImGui::ActivateItemByID(menu->GetID("Rename"));frame();frame();
            auto dialog=ImGui::FindWindowByName("Profile action");check(dialog&&dialog->Active,"Rename opens the profile action dialog");
            UiTestAccess::renameText(ui);ImGui::ActivateItemByID(dialog->GetID("Save name"));frame();frame();
            check(settings.profiles[0].name=="Renamed tab","rename action updates the intended tab");
            hover(750,120);click(0);ImGuiWindow* combo=nullptr;
            for(auto window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::string(window->Name).starts_with("##Combo_"))combo=window;
            check(combo!=nullptr,"custom profile dropdown opens");float x=combo->Pos.x+40,y=combo->DC.CursorStartPos.y+9;
            auto customFirst=hover(x,y),customSecond=hover(x,y+26);
            check(customFirst&&customSecond&&customFirst!=customSecond,"same-named custom profiles have distinct interactive IDs");
            click(0);check(settings.selected==secondCustom,"click selects the second same-named profile independently");
        }
        ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();CoUninitialize();
        std::cout<<"PASS: actual UI hover IDs, duplicate profile names, right-click Rename, and independent dropdown selection\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';CoUninitialize();return 1;}
}
