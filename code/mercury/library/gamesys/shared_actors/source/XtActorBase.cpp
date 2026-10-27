#include "SharedActorsPch.h"

#include <Shared_Actors/XtActorBase.h>

#if defined(PLATFORM_MACOS)
#include <shared_actors/ActorCreation/SystemActorsCreate.h>
// [Blur2PC] Probe abstract base layouts through a real existing concrete type.
namespace Xt {
template<>struct FaBase_LayoutProbe<Actors::C_GameActorCreate>{using Type=Actors::C_NullActorCreate;};
template<>struct FaBase_LayoutProbe<Actors::C_PosActorCreate>{using Type=Actors::C_NullActorCreate;};
}
#endif

// Base Xt annotated type
XT_ABSTRACT_TYPE(Actors::C_GameActorCreate, "GameActorCreate")
XT_BASE(Actor::C_ActorCreationData)
XT_FIELD(m_Name, "Name")
XT_FIELD(m_IsCommented, "IsCommented")
XT_END_TYPE

// Base position actor type
XT_ABSTRACT_TYPE(Actors::C_PosActorCreate, "PosActorCreate")
XT_BASE(Actors::C_GameActorCreate)
XT_FIELD(m_WorldMatrix, "WorldMatrix")
XT_END_TYPE

XT_LIBRARY(XtActorBase)
XT_REGISTER_TYPE(Actor::C_ActorCreationData)
XT_REGISTER_TYPE(Component::I_ComponentCreator)
XT_REGISTER_TYPE(Actors::C_GameActorCreate)
XT_REGISTER_TYPE(Actors::C_PosActorCreate)
XT_END_LIBRARY

namespace Actors
{
	Debug::Logger &g_Log = Debug::GetLogger("mercury.gamesys.actors");
}
