#include "Reins.h"

#include "SKSEMenuFramework.h"

namespace Controls {
// Optional named fonts remain available. Use the framework's guarded cleanup:
// a missing named font does not push anything, so raw ImGui::PopFont is unsafe.
struct LanguageFont {
 bool active=false;
 explicit LanguageFont(unsigned language=Translations::language.load()) {
  const char* name=language==2?"Futura Condensed (RU)":language==3?"Futura Condensed (CN)":language==4?"Futura Condensed (JA)":language==7?"Futura Condensed (PL)":nullptr;
  if(name && GetMenuFrameworkFunction<SKSEMenuFramework::Model::PushFontFunction>("PushFont") && GetMenuFrameworkFunction<SKSEMenuFramework::Model::ActionFunction>("Pop")) {
   SKSEMenuFramework::PushFont(name);active=true;
  }
 }
 ~LanguageFont(){if(active)FontAwesome::Pop();}
};
// Query the font actually loaded, preserving supported characters.
const char* Visible(const char* input) {
 using GetFontFn=ImGuiMCP::ImFont* (*)();
 using GlyphFn=const ImGuiMCP::ImFontGlyph* (*)(ImGuiMCP::ImFont*,ImGuiMCP::ImWchar);
 auto getFont=GetMenuFrameworkFunction<GetFontFn>("igGetFont");
 auto findGlyph=GetMenuFrameworkFunction<GlyphFn>("ImFont_FindGlyphNoFallback");
 auto* font=getFont?getFont():nullptr;
 thread_local std::array<std::string,16> buffers;
 thread_local unsigned next=0;
 auto& output=buffers[next++%buffers.size()];output.clear();
 const auto* cursor=reinterpret_cast<const unsigned char*>(input);
 while(*cursor) {
  const auto* begin=cursor;
  unsigned code=*cursor++;unsigned extra=0;
  if(code>=0xC2 && code<=0xDF){code&=0x1F;extra=1;}
  else if(code>=0xE0 && code<=0xEF){code&=0x0F;extra=2;}
  else if(code>=0xF0 && code<=0xF4){code&=0x07;extra=3;}
  else if(code>=0x80){output+='?';continue;}
  bool valid=true;
  for(unsigned i=0;i<extra;++i){if((*cursor&0xC0)!=0x80){valid=false;break;}code=(code<<6)|(*cursor++&0x3F);}
  const bool available=valid && (code<128 || (font && findGlyph && code<=0xFFFF && findGlyph(font,static_cast<ImGuiMCP::ImWchar>(code))));
  if(available)output.append(reinterpret_cast<const char*>(begin),cursor-begin);
  else output+='?';
 }
 return output.c_str();
}
const char* DisplayText(const char* en,const char* pt="") {return Visible(Translations::Text(en,pt));}

void Load() {
 bool endpointPresetSeen=false;

 std::ifstream f("Data/SKSE/Plugins/HorseReins.ini");

 std::string line;

 while (std::getline(f,line)) {
 if(line=="EndpointPreset=Default"){EndpointConfig::hand=false;endpointPresetSeen=true;}
 if(line=="EndpointPreset=Hand"){EndpointConfig::hand=true;endpointPresetSeen=true;}
 if(line.starts_with("EndpointOffset=")) {
  auto text=line.substr(15);std::replace(text.begin(),text.end(),',',' ');
  std::istringstream stream(text);stream.imbue(std::locale::classic());
  auto v=EndpointConfig::defaults;bool valid=true;
  for(auto& x:v)if(!(stream>>x) || !std::isfinite(x) || x< -100.f || x>100.f)valid=false;
  if(valid)EndpointConfig::Set(v);
 }
 if(line=="NeckEnabled=0")NeckConfig::enabled=false;
 if(line=="NeckEnabled=1")NeckConfig::enabled=true;
 if(line.starts_with("NeckShape=")) {
  auto text=line.substr(10);std::replace(text.begin(),text.end(),',',' ');std::istringstream stream(text);stream.imbue(std::locale::classic());
  auto values=NeckConfig::defaults;bool valid=true;
  for(auto& value:values)if(!(stream>>value)||!std::isfinite(value))valid=false;
  for(int i=0;i<7;++i){const float low=i>=3&&i<=5?10.f:(i==6?-80.f:-150.f);const float high=i>=3&&i<=5?150.f:(i==6?80.f:150.f);if(values[i]<low||values[i]>high)valid=false;}
  if(valid)NeckConfig::Set(values);
 }

 if(line.starts_with("Language="))for(unsigned i=0;i<Translations::codes.size();++i)if(line.substr(9)==Translations::codes[i])Translations::language=i;

 if(line=="AutomaticReins=1") automaticReins=true;

 if(line=="AutomaticReins=0") automaticReins=false;

 if(line=="DisableNotifications=1") disableNotifications=true;

 if(line=="DisableNotifications=0") disableNotifications=false;





 if (line.starts_with("Key=")) {

  try { auto n=std::stoul(line.substr(4)); if(ValidKey(static_cast<unsigned>(n))) key=static_cast<unsigned>(n); } catch(...) {}

 }

 }

 if(!endpointPresetSeen){EndpointConfig::hand=false;EndpointConfig::Set(EndpointConfig::defaults);}

}

void Save() {

 std::ofstream f("Data/SKSE/Plugins/HorseReins.ini");

 if(f) f << "[Controls]\nKey=" << key.load() << "\nAutomaticReins=" << (automaticReins?1:0) << "\nDisableNotifications=" << (disableNotifications?1:0) << "\n[Translations]\nLanguage=" << Translations::codes[Translations::language.load()] << "\n";

 else logger::error("Cannot save HorseReins.ini");
 if(f){f.imbue(std::locale::classic());f<<"\n[NeckProtection]\nNeckEnabled="<<(NeckConfig::enabled?1:0)<<"\nNeckShape=";
 const auto values=NeckConfig::Get();for(unsigned i=0;i<7;++i)f<<(i?",":"")<<values[i];f<<"\n";}
 if(f){f<<"\n[RopeEndpoint]\nEndpointPreset="<<(EndpointConfig::hand?"Hand":"Default")<<"\nEndpointOffset=";const auto v=EndpointConfig::Get();
 for(unsigned i=0;i<3;++i)f<<(i?",":"")<<v[i];f<<"\n";}



}

bool Blocked() { return SKSEMenuFramework::IsAnyBlockingWindowOpened(); }

std::string KeyName(unsigned scanCode) {

 if(scanCode>=266 && scanCode<=281)return SKSE::InputMap::GetGamepadButtonName(scanCode);

 // DirectInput stores extended scan codes with bit 7 set; Win32 uses bit 24.

 const LONG flags=static_cast<LONG>(((scanCode&0x7f)<<16) | ((scanCode&0x80)?(1u<<24):0));

 wchar_t name[128]{};

 if(!GetKeyNameTextW(flags,name,128)) return DisplayText("Unknown key","Tecla desconhecida");

 char utf8[512]{};

 WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,sizeof(utf8),nullptr,nullptr);

 return utf8;

}

void __stdcall Render() {
 LanguageFont font;

 bool automatic=automaticReins.load();

 if(ImGuiMCP::Checkbox(DisplayText("Horses already have reins","Cavalos já vêm com rédea"),&automatic)) {automaticReins=automatic;Save();}

 if(!automaticReins) ImGuiMCP::TextWrapped("%s",DisplayText("Fit reins first: hold Leather Strips with E near the horse head for 0.6 seconds.","Primeiro coloque a redea: segure Leather Strips com E junto a cabeca por 0,6 segundo."));

 bool disabled=disableNotifications.load();

 if(ImGuiMCP::Checkbox(DisplayText("Disable notifications","Desligar notificacoes"),&disabled)) {disableNotifications=disabled;Save();}

 ImGuiMCP::TextWrapped("%s",DisplayText("Hold the key for half a second near a horse to take / untie its reins.","Segure a tecla por meio segundo perto do cavalo para pegar a redea ou desamarrar."));

 ImGuiMCP::TextWrapped("%s",DisplayText("Hold the key while leading to aim the endpoint; release to tie.","Ao conduzir, segure a tecla para posicionar a ponta; solte para amarrar."));

 ImGuiMCP::Text(DisplayText("Selected key: %s","Tecla selecionada: %s"),KeyName(key.load()).c_str());

 if(ImGuiMCP::Button(capture ? DisplayText("Press a key or gamepad button...","Pressione uma tecla ou botao do controle...") : DisplayText("Change key","Alterar tecla"))) capture=true;

 if(ImGuiMCP::Button(DisplayText("Reset to G","Restaurar G"))) { key=34; capture=false; Save(); }

}

bool __stdcall Capture(RE::InputEvent* event) {

 if(!capture) return Reins::BlockHorseCall(event);

 if(auto* b=event->AsButtonEvent(); b && (b->GetDevice()==RE::INPUT_DEVICE::kKeyboard || b->GetDevice()==RE::INPUT_DEVICE::kGamepad) && b->IsDown()) {

  auto code=ButtonCode(b);

  if(ValidKey(code)) { key=code; capture=false; Save(); }

 }

 return true;

}

void __stdcall RenderTranslations() {
 LanguageFont font;

 ImGuiMCP::TextWrapped("%s",DisplayText("Choose the language for menus and notifications.","Escolha o idioma dos menus e notificacoes."));

 for(unsigned i=0;i<Translations::labels.size();++i) {
  LanguageFont choiceFont(i);
  if(ImGuiMCP::RadioButton(Visible(Translations::labels[i]),Translations::language.load()==i)) {Translations::language=i;Save();}
 }

}

void __stdcall RenderEndpoint() {
 LanguageFont font;
 ImGuiMCP::TextWrapped("%s",DisplayText("Default: original floating tip (85,0,0). Hand: tip near the hand (10,0,0). Adjust forward, right and up. Resume the game to see changes."));
 if(ImGuiMCP::RadioButton("Default",!EndpointConfig::hand)){EndpointConfig::hand=false;EndpointConfig::Set(EndpointConfig::defaults);Save();}
 ImGuiMCP::SameLine();
 if(ImGuiMCP::RadioButton("Hand",EndpointConfig::hand)){EndpointConfig::hand=true;EndpointConfig::Set(EndpointConfig::handDefaults);Save();}
 auto v=EndpointConfig::Get();
 const char* labels[]={"Forward offset","Right offset","Up offset"};
 for(unsigned i=0;i<3;++i) {
  if(ImGuiMCP::DragFloat(DisplayText(labels[i]),&v[i],0.25f,-100.f,100.f,"%.2f")) {
   v[i]=std::isfinite(v[i])?std::clamp(v[i],-100.f,100.f):EndpointConfig::defaults[i];EndpointConfig::Set(v);
  }
  if(ImGuiMCP::IsItemDeactivatedAfterEdit())Save();
 }
 if(ImGuiMCP::Button(DisplayText("Reset endpoint"))){EndpointConfig::Set(EndpointConfig::hand?EndpointConfig::handDefaults:EndpointConfig::defaults);Save();}
 if(ImGuiMCP::Button(DisplayText("Copy endpoint values"))){std::ostringstream out;out.imbue(std::locale::classic());out<<"EndpointPreset="<<(EndpointConfig::hand?"Hand":"Default")<<"\nEndpointOffset=";
 const auto offsets=EndpointConfig::Get();for(unsigned i=0;i<3;++i)out<<(i?",":"")<<offsets[i];ImGuiMCP::SetClipboardText(out.str().c_str());}
}

void __stdcall RenderNeck() {
 LanguageFont font;
 ImGuiMCP::TextWrapped("%s",DisplayText("A continuous neck shell. Offsets follow the horse withers: X forward, Y right, Z up. Resume the game to see changes."));
 bool enabled=NeckConfig::enabled.load(),preview=NeckConfig::preview.load();
 if(ImGuiMCP::Checkbox(DisplayText("Enable neck protection"),&enabled)){NeckConfig::enabled=enabled;Save();}
 if(ImGuiMCP::Checkbox(DisplayText("Show pink outline"),&preview))NeckConfig::preview=preview;
 auto values=NeckConfig::Get();
 const char* labels[]={"Forward offset","Right offset","Up offset","Length radius","Width radius","Height radius","Tilt (degrees)"};
 for(int i=0;i<7;++i){const float low=i>=3&&i<=5?10.f:(i==6?-80.f:-150.f),high=i>=3&&i<=5?150.f:(i==6?80.f:150.f);
  if(ImGuiMCP::DragFloat(DisplayText(labels[i]),&values[i],0.25f,low,high,"%.2f")){values[i]=std::isfinite(values[i])?std::clamp(values[i],low,high):NeckConfig::defaults[i];NeckConfig::Set(values);}
  if(ImGuiMCP::IsItemDeactivatedAfterEdit())Save();
 }
 if(ImGuiMCP::Button(DisplayText("Reset protection"))){NeckConfig::Set(NeckConfig::defaults);Save();}
 if(ImGuiMCP::Button(DisplayText("Copy protection values"))){std::ostringstream out;out.imbue(std::locale::classic());out<<"NeckShape=";const auto v=NeckConfig::Get();for(int i=0;i<7;++i)out<<(i?",":"")<<v[i];ImGuiMCP::SetClipboardText(out.str().c_str());}
}
RE::NiCamera* FindCamera(RE::NiAVObject* object,int depth=0) {
 if(!object || depth>8)return nullptr;
 if(auto* camera=netimmerse_cast<RE::NiCamera*>(object))return camera;
 if(auto* node=object->AsNode())for(auto& child:node->GetChildren())if(auto* camera=FindCamera(child.get(),depth+1))return camera;
 return nullptr;
}
void DrawNeck() {
 if(!NeckConfig::preview)return;
 auto* playerCamera=RE::PlayerCamera::GetSingleton();auto* camera=playerCamera?FindCamera(playerCamera->cameraRoot.get()):nullptr;
 if(!camera)return;
 std::vector<std::pair<RE::NiPoint3,RE::NiPoint3>> lines;{std::scoped_lock lock(NeckConfig::mutex);lines=NeckConfig::wire;}
 const auto size=ImGuiMCP::GetIO()->DisplaySize;auto* draw=ImGuiMCP::GetForegroundDrawList(ImGuiMCP::GetMainViewport());
 for(auto& [a,b]:lines){float x,y,z,u,v,w;
  if(camera->WorldPtToScreenPt3(a,x,y,z,0.00001f)&&camera->WorldPtToScreenPt3(b,u,v,w,0.00001f)&&z>0&&w>0)
   ImGuiMCP::ImDrawListManager::AddLine(draw,{x*size.x,(1-y)*size.y},{u*size.x,(1-v)*size.y},0xffb050ff,1.5f);
 }
}

void __stdcall RenderHUD() {
 DrawNeck();

 const float progress=Controls::grabProgress.load();

 if(progress<0 || Controls::Blocked())return;

 auto* viewport=ImGuiMCP::GetMainViewport();

 auto* draw=ImGuiMCP::GetForegroundDrawList(viewport);

 auto size=ImGuiMCP::GetIO()->DisplaySize;

 ImGuiMCP::ImDrawListManager::PathArcTo(draw,{size.x*0.5f,size.y*0.5f},12.f,-1.5707963f,-1.5707963f+6.2831853f*std::clamp(progress,0.001f,1.f),48);

 ImGuiMCP::ImDrawListManager::PathStroke(draw,0xffffffff,0,2.f);

}

void RegisterMenu() {

 if(!GetMenuFrameworkModule()) { logger::warn("SKSE Menu Framework not loaded; using INI key"); return; }

 SKSEMenuFramework::SetSection("HorseReins");

 SKSEMenuFramework::AddSectionItem("Controls",Render);

 SKSEMenuFramework::AddSectionItem("Translations",RenderTranslations);
 SKSEMenuFramework::AddSectionItem("Neck Protection",RenderNeck);
 SKSEMenuFramework::AddSectionItem("Rope Endpoint",RenderEndpoint);

 SKSEMenuFramework::AddInputEvent(Capture);

 SKSEMenuFramework::AddHudElement(RenderHUD);

}

}

