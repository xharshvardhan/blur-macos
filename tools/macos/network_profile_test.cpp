// [Blur2PC] Actual system-profile manager lifecycle and native local account persistence, isolated from user saves.
#include <StdAfx.h>
#include <SystemProfile/SystemProfileManager.h>
#include <SystemProfile/GameInvite.h>
#include <bdCore/bdCore.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){alarm(20);auto& manager=SysProfile::C_Manager::Instance();std::filesystem::path directory;
try{
 char location[]="/tmp/blur-native-profiles-XXXXXX";require(mkdtemp(location)!=nullptr,"cannot create isolated native profile store");directory=location;
 const auto store=directory/"local-profile-id";require(setenv("BLUR_MAC_PROFILE_STORE",store.c_str(),1)==0,"cannot select isolated profile store");
 bdCore::init();SysProfile::FullProfile profile;
 require(!manager.GetProfile(0,profile)&&!profile.Active(),"uninitialized native manager reported an active user");
 require(manager.Initialise()&&manager.MaxUsers()==4,"actual profile initialization/local slot limit failed");
 require(manager.GetProfile(0,profile)&&profile.m_Uid!=0&&profile.m_Name[0]&&profile.m_State==SysProfile::eProfileStateOffline&&!profile.m_IsGuest,"native local OS account did not become a real offline profile");
 const U64 identity=profile.m_Uid;const auto reference=profile.m_Ref;
 require(!profile.m_OnlineMultiplayerPrivilege&&!profile.m_ScoreboardPrivilege&&!profile.m_PurchaseContentPrivilege,"native local identity acquired hosted privileges");
 SysProfile::FullProfile again;require(manager.GetProfile(0,again)&&again.m_Uid==identity&&again.m_Ref==reference,"unchanged offline profile snapshot changed identity/reference");
 require(!manager.GetProfile(1,again)&&!again.Active(),"unused local controller became another OS account");
 manager.SetPrimaryPadNo(3);require(manager.GetPrimaryPadNo()==3&&manager.GetProfile(3,again)&&again.m_Uid==identity&&!again.m_IsGuest,"local OS identity changed with the selected controller");
 require(!manager.GetProfile(0,again),"previous controller remained a duplicate signed-in OS account");
 std::set<U64> identities{identity};
 for(unsigned n=0;n<3;++n){require(manager.CreateBlaggedProfile(n,u"Guest %s \u03bb",again)&&again.m_IsGuest&&again.m_State==SysProfile::eProfileStateBlagged&&again.m_Name[6]==u'%',"actual explicit local guest profile/name failed");
  require(again.m_Uid!=0&&identities.insert(again.m_Uid).second&&!again.m_OnlineMultiplayerPrivilege,"native local guest IDs collided or acquired online privileges");
  require(manager.GetProfile(n,profile)&&profile.m_Uid==again.m_Uid&&profile.m_IsGuest,"native local guest slot did not retain its identity");}
 require(manager.CreateBlaggedProfile(3,u"Guest",again)&&again.m_Uid==identity&&again.m_State==SysProfile::eProfileStateOffline&&!again.m_IsGuest,"fallback setup replaced the selected real OS account with a placeholder");
 require(!manager.GetProfile(4,again)&&!again.Active()&&!manager.CreateBlaggedProfile(4,u"Invalid",again),"native profile manager accepted an out-of-range local slot");
 manager.SetPrimaryPadNo(4);require(manager.GetPrimaryPadNo()==3,"invalid primary controller changed native account selection");
 bdOnlineUserID hosted=99;require(!manager.GetDemonwareUserId(3,hosted)&&hosted==0&&!manager.SetDemonwareAuthenticatedPlayerData("local-account"),"native local identity fabricated hosted authentication");
 SysProfile::GameInvite invite;SysProfile::Notifications notifications;manager.Poll(invite,notifications,1.0f/60.0f);
 require(notifications.m_ProfilesChanged&&!notifications.m_ServiceConnectionEstablished&&!invite.m_Active,"local lifecycle notification fabricated hosted service/invite activity");
 struct stat info{};require(stat(store.c_str(),&info)==0&&info.st_size==8&&(info.st_mode&0777)==0600,"native persistent identity file size/protection changed");
 manager.Shutdown();require(!manager.GetProfile(0,again)&&!again.Active(),"shutdown native profile manager retained an active user");
 require(manager.Initialise()&&manager.GetProfile(0,again)&&again.m_Uid==identity,"native local identity did not survive actual manager teardown/reinitialization");manager.Shutdown();
 {std::ofstream corrupt(store,std::ios::binary|std::ios::trunc);corrupt<<"bad";}
 require(!manager.Initialise()&&!manager.GetProfile(0,again),"corrupt native identity silently created a replacement user");
 manager.Shutdown();bdCore::quit();std::filesystem::remove_all(directory);alarm(0);
 std::cout<<"Actual native SysProfile lifecycle: OS account, durable save/LAN ID, four bounded local slots/unique explicit guests, offline privileges, corrupt-store failure and teardown PASS\n";return 0;
}catch(const std::exception& error){manager.Shutdown();if(!directory.empty())std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}}
