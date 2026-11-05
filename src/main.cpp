#include "Reins.h"
namespace {
bool ready=false;
struct Update {
 static void thunk(RE::PlayerCharacter* p,float dt) {if(ready)Reins::BeforeUpdate();func(p,dt);if(ready)Reins::Update(dt);}
 static inline REL::Relocation<decltype(thunk)> func;
};
void Message(SKSE::MessagingInterface::Message* m) {
 if(m->type==SKSE::MessagingInterface::kDataLoaded) {
  ready=Reins::Init();
  if(ready) {REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};Update::func=table.write_vfunc(0xAD,Update::thunk);}
 } else if(m->type==SKSE::MessagingInterface::kPostLoadGame || m->type==SKSE::MessagingInterface::kNewGame)Reins::OnGameLoad();
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
 SKSE::Init(skse,{.logPattern="[%H:%M:%S.%e] [%l] %v",.trampoline=false});
 auto* s=SKSE::GetSerializationInterface();s->SetUniqueID('RDEA');
 s->SetSaveCallback(Reins::Save);s->SetLoadCallback(Reins::Load);
 s->SetRevertCallback([](SKSE::SerializationInterface*){Reins::Revert();});
 return SKSE::GetMessagingInterface()->RegisterListener(Message);
}
