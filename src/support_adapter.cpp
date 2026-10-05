#include "ui.hpp"
#include "version.hpp"
#include <imgui.h>
#include <algorithm>
namespace rfl {
void Ui::draw_support_panel(){
    adamch_support::AppIdentity identity;
    identity.appName="ReadyForLaunch";identity.appVersion=appVersion;identity.appId="ReadyForLaunch";
    identity.scale=ImGui::GetStyle().FontScaleDpi;
    identity.updateStatus=updates_.status().message;
    identity.appLogo=[this] {
        if(!supportLogo_.view){try{supportLogo_=loadLogoImage(device_);}catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}}
        if(supportLogo_.view){
            const float width=100.f*ImGui::GetStyle().FontScaleDpi;
            const float height=width*float(supportLogo_.height)/float(std::max(supportLogo_.width,1u));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(ImGui::GetContentRegionAvail().x-width)*.5f);
            ImGui::Image(reinterpret_cast<ImTextureID>(supportLogo_.view.Get()),ImVec2(width,height));
        }
    };
    identity.checkForUpdates=[this]{
        const auto state=updates_.status().state;
        if(state!=UpdateState::Checking&&state!=UpdateState::Downloading&&state!=UpdateState::Available&&state!=UpdateState::Ready)updates_.check();
        updateOpen_=true;ImGui::OpenPopup("Updates");
    };
    support_panel_.draw(identity);
}
} // namespace rfl
