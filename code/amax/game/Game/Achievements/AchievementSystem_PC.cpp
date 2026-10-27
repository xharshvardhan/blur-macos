#include "stdafx.h"

#include <Game\Achievements\AchievementSystem.h>
#include <Game\Messaging\GameflowMessages.h>

namespace Game
{

C_AchievementSystem C_AchievementSystem::s_instance;

void C_AchievementSystem::Init()
{


}

WCHAR *C_AchievementSystem::GetAchievementName(BLUR_ACHIEVEMENT achievement)	// [Blur2PC] matches the declaration
{
	static WCHAR emptyName[]=AMAX_WIDE_LITERAL(""); // [Blur2PC] Preserve the mutable UTF-16 API without casting a native wchar_t literal.
	return emptyName;
}

// [Blur2PC] declared in AchievementSystem.h but never defined for PC (the consoles read it from XLAST/trophy data)
U32 C_AchievementSystem::GetAchievementScore(BLUR_ACHIEVEMENT achievement)
{
	return 0;
}

void C_AchievementSystem::GetAchievementDetails(int user_index)
{

}

void C_AchievementSystem::Update()
{


}

void C_AchievementSystem::InitialiseSubscription( Messaging::C_MessageSubscriberSystem* pSubscriberSystem )
{
}

bool C_AchievementSystem::MessageHandler(const  Messaging::C_MessageBase* pMessage)
{
	return false;
}

bool C_AchievementSystem::HasAchievement(int user_index,BLUR_ACHIEVEMENT achievement)
{
	return false;
}

void C_AchievementSystem::GiveAchievement(int user_index,BLUR_ACHIEVEMENT achievement)
{



}

}; // namespace Game
