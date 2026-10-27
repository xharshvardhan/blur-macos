// [Blur2PC][macOS] Private GameController adapter; snapshots also support source-only verification.
#pragma once
#include <input/Pad360.h>
#include <array>
#include <memory>
namespace Input::Mac {
struct PadSample {bool connected=false;std::array<F32,C_Pad360::E_Button_Count> buttons{};std::array<F32,C_Pad360::E_Axis_Count> axes{};};
void PollControllerList(C_DeviceManager* manager,void* array); // Private deterministic host-list entry point.
class Controllers {
public:
    Controllers();~Controllers();
    Controllers(const Controllers&)=delete;Controllers& operator=(const Controllers&)=delete;
    void Poll();
    void PollList(void* array); // Private NSArray<GCController*> entry point, also used for authored snapshot verification.
    const PadSample& Get(U32 player)const;
    bool Rumble(U32 player,F32 low,F32 high);
private:struct Impl;std::unique_ptr<Impl> m_Impl;
};
}
