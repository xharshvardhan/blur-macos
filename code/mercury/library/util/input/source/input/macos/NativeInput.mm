// [Blur2PC][macOS] Original physical interfaces, driven by local Cocoa window events.
#include <core/Core.h>
#include <system/Debug/Debug.h>
#include <system/debuglog.h>
#include <system/time/time.h>
#include <input/macos/NativeInput.h>
#include <input/Keyboard.h>
#include <input/Mouse.h>
#include "NativeControllers.h"
#include <array>
#include <memory>
#include <mutex>
#include <cmath>
#include <stdexcept>
#include <strings.h>
#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
namespace Input {
Debug::Float g_debugMouseSensitivity("Mouse.Sensitivity",0.25f,Debug::Range(0.0f,1.0f)); // Original MousePC default/units.
namespace Mac {
struct Sample {std::array<F32,C_Keyboard::E_Button_Count> keys{};std::array<F32,5> buttons{};F64 x=0,y=0,dx=0,dy=0,wheel=0;bool positioned=false,focused=true;};
// [Blur2PC] Automation never mutates the physical Cocoa key state. Overlay it
// only while sampling, using the same elapsed hold as the original PC helper.
// [Blur2PC] One independent bounded hold per key can coexist with the single
// pending Tap. Both overlay samples; physical pending keys retain ownership.
struct KeyHold {bool active=false,started=false;F64 seconds=0;Time::C_Time start=0;};
struct State {std::mutex mutex;Sample pending,sample;S32 tapKey=-1;bool tapPressed=false;Time::C_Time tapStart=0;std::array<KeyHold,C_Keyboard::E_Button_Count> holds{};};
class Manager final:public C_DeviceManager {
public:
    explicit Manager(const C_DeviceManagerCreateParam& p):C_DeviceManager(p,4),state(std::make_shared<State>()){}
    std::shared_ptr<State> state;
    Controllers controllers;
private:
    void PlatformUpdate() override {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->sample=state->pending;
            state->pending.dx=state->pending.dy=state->pending.wheel=0;
            const auto now=Time::GetPerformanceCounter();
            for(S32 key=0;key<C_Keyboard::E_Button_Count;++key){
                auto& hold=state->holds[key];if(!hold.active)continue;
                if(!hold.started){hold.started=true;hold.start=now;}
                const F64 elapsed=F64(now-hold.start)/F64(Time::GetPerformanceFrequency());
                if(elapsed<hold.seconds)state->sample.keys[key]=1;
                else hold=KeyHold{};
            }
            if(state->tapKey>=0) {
                if(!state->tapPressed) {state->tapPressed=true;state->tapStart=now;}
                const F64 elapsed=F64(now-state->tapStart)/F64(Time::GetPerformanceFrequency());
                if(elapsed<0.15)state->sample.keys[state->tapKey]=1;
                else {state->tapKey=-1;state->tapPressed=false;}
            }
        }
        controllers.Poll();
    }
};
class Keyboard final:public C_Keyboard {
public:
    Keyboard(Manager* manager,S32 user):C_Keyboard(manager,user),state(manager->state){m_DeviceType=E_Device_Keyboard;m_Connected=true;}
    void Update()override{for(S32 i=0;i<E_Button_Count;++i)m_ButtonValues[i]=state->sample.keys[i];}
private:std::shared_ptr<State> state;
};
class Mouse final:public C_Mouse {
public:
    Mouse(Manager* manager,S32 user):C_Mouse(manager,user),state(manager->state),last(Time::GetPerformanceCounter()){m_DeviceType=E_Device_Mouse;m_Connected=true;}
    void Update()override{const auto& s=state->sample;for(S32 i=0;i<5;++i)m_ButtonValues[i]=s.buttons[i];
        m_ButtonValues[E_Button_WheelUp]=s.wheel>0?1:0;m_ButtonValues[E_Button_WheelDown]=s.wheel<0?1:0;
        const auto now=Time::GetPerformanceCounter();F32 dt=F32(F64(now-last)/F64(Time::GetPerformanceFrequency()));last=now;if(dt<0.001f)dt=0.001f;
        const F32 scale=F32(g_debugMouseSensitivity)*0.01f/dt;
        // ApplicationPC negates downward raw Y before MousePC's elapsed-time/sensitivity scaling.
        m_AxisValues[E_Axis_DeltaX]=s.dx*scale;m_AxisValues[E_Axis_DeltaY]=-s.dy*scale;m_AxisValues[E_Axis_XPos]=s.x;m_AxisValues[E_Axis_YPos]=s.y;m_AxisValues[E_Axis_Wheel]=s.wheel;}
private:std::shared_ptr<State> state;Time::C_Time last;
};
static S32 keyIndex(U16 code){switch(code){
#define KEY(Mac,Game) case kVK_##Mac:return C_Keyboard::E_Button_##Game;
    KEY(Escape,Escape) KEY(Return,Return) KEY(Delete,Back) KEY(Tab,Tab) KEY(Space,Space)
    KEY(ANSI_Minus,Minus) KEY(ANSI_Equal,Equals) KEY(ANSI_LeftBracket,LBracket) KEY(ANSI_RightBracket,RBracket)
    KEY(ANSI_Semicolon,SemiColon) KEY(ANSI_Quote,Apostrophe) KEY(ANSI_Grave,Grave) KEY(ANSI_Backslash,Backslash)
    KEY(ANSI_Comma,Comma) KEY(ANSI_Period,Period) KEY(ANSI_Slash,Slash)
    KEY(Shift,LShift) KEY(RightShift,RShift) KEY(Control,LControl) KEY(RightControl,RControl)
    KEY(Option,LMenu) KEY(RightOption,RMenu) KEY(CapsLock,Capital)
    KEY(Command,LWin) KEY(RightCommand,RWin)
    KEY(LeftArrow,Left) KEY(RightArrow,Right) KEY(UpArrow,Up) KEY(DownArrow,Down)
    KEY(Home,Home) KEY(End,End) KEY(PageUp,Prior) KEY(PageDown,Next) KEY(ForwardDelete,Delete)
    KEY(ANSI_Keypad0,NumPad0) KEY(ANSI_Keypad1,NumPad1) KEY(ANSI_Keypad2,NumPad2) KEY(ANSI_Keypad3,NumPad3)
    KEY(ANSI_Keypad4,NumPad4) KEY(ANSI_Keypad5,NumPad5) KEY(ANSI_Keypad6,NumPad6) KEY(ANSI_Keypad7,NumPad7)
    KEY(ANSI_Keypad8,NumPad8) KEY(ANSI_Keypad9,NumPad9) KEY(ANSI_KeypadDecimal,Decimal)
    KEY(ANSI_KeypadPlus,Add) KEY(ANSI_KeypadMinus,Subtract) KEY(ANSI_KeypadMultiply,Multiply)
    KEY(ANSI_KeypadDivide,Divide) KEY(ANSI_KeypadEnter,NumPadEnter) KEY(ANSI_KeypadEquals,NumPadEquals)
    KEY(F1,F1) KEY(F2,F2) KEY(F3,F3) KEY(F4,F4) KEY(F5,F5) KEY(F6,F6) KEY(F7,F7) KEY(F8,F8)
    KEY(F9,F9) KEY(F10,F10) KEY(F11,F11) KEY(F12,F12) KEY(F13,F13) KEY(F14,F14) KEY(F15,F15)
#define LETTER(C) KEY(ANSI_##C,C)
    LETTER(A) LETTER(B) LETTER(C) LETTER(D) LETTER(E) LETTER(F) LETTER(G) LETTER(H) LETTER(I)
    LETTER(J) LETTER(K) LETTER(L) LETTER(M) LETTER(N) LETTER(O) LETTER(P) LETTER(Q) LETTER(R)
    LETTER(S) LETTER(T) LETTER(U) LETTER(V) LETTER(W) LETTER(X) LETTER(Y) LETTER(Z)
    LETTER(0) LETTER(1) LETTER(2) LETTER(3) LETTER(4) LETTER(5) LETTER(6) LETTER(7) LETTER(8) LETTER(9)
#undef LETTER
#undef KEY
    default:return -1;}}
// [Blur2PC] The same public key spellings as the original Lua Tap helper,
// mapped directly to the game's portable keyboard enum rather than DIK codes.
S32 KeyFromName(const char* name) {
    if(!name)return -1;
    struct Pair {const char* name;S32 key;};
#define NAMED(Name,Game) {Name,C_Keyboard::E_Button_##Game}
#define LETTER(Name) NAMED(#Name,Name)
    static const Pair keys[]={
        NAMED("RETURN",Return),NAMED("ENTER",Return),NAMED("ESCAPE",Escape),NAMED("ESC",Escape),NAMED("SPACE",Space),NAMED("TAB",Tab),
        NAMED("NUMPADPLUS",Add),NAMED("NUMPADENTER",NumPadEnter),NAMED("NUMPAD0",NumPad0),
        NAMED("UP",Up),NAMED("DOWN",Down),NAMED("LEFT",Left),NAMED("RIGHT",Right),
        NAMED("RSHIFT",RShift),NAMED("LSHIFT",LShift),NAMED("RCONTROL",RControl),NAMED("LCONTROL",LControl),
        NAMED("PGUP",Prior),NAMED("PGDN",Next),NAMED("INSERT",Insert),NAMED("DELETE",Delete),
        LETTER(A),LETTER(B),LETTER(C),LETTER(D),LETTER(E),LETTER(F),LETTER(G),LETTER(H),LETTER(I),LETTER(J),LETTER(K),LETTER(L),LETTER(M),
        LETTER(N),LETTER(O),LETTER(P),LETTER(Q),LETTER(R),LETTER(S),LETTER(T),LETTER(U),LETTER(V),LETTER(W),LETTER(X),LETTER(Y),LETTER(Z),
        LETTER(0),LETTER(1),LETTER(2),LETTER(3),LETTER(4),LETTER(5),LETTER(6),LETTER(7),LETTER(8),LETTER(9),
        LETTER(F1),LETTER(F2),LETTER(F3),LETTER(F4),LETTER(F5),LETTER(F6),LETTER(F7),LETTER(F8),LETTER(F9),LETTER(F10),LETTER(F11),LETTER(F12)
    };
#undef LETTER
#undef NAMED
    for(const auto& key:keys)if(strcasecmp(key.name,name)==0)return key.key;
    return -1;
}
bool QueueKeyTap(C_DeviceManager* manager,S32 key) {
    auto* native=dynamic_cast<Manager*>(manager);
    if(!native||key<0||key>=C_Keyboard::E_Button_Count)return false;
    std::lock_guard<std::mutex> lock(native->state->mutex);
    if(native->state->tapKey>=0||native->state->holds[key].active)return false;
    native->state->tapKey=key;native->state->tapPressed=false;return true;
}
// [Blur2PC] Automation is an explicit input overlay, never an OS key event.
bool QueueKeyHold(C_DeviceManager* manager,S32 key,F64 seconds) {
    auto* native=dynamic_cast<Manager*>(manager);
    if(!native||key<0||key>=C_Keyboard::E_Button_Count||!std::isfinite(seconds)||seconds<=0||seconds>60)return false;
    std::lock_guard<std::mutex> lock(native->state->mutex);
    if(native->state->tapKey==key||native->state->holds[key].active)return false;
    auto& hold=native->state->holds[key];hold.active=true;hold.started=false;hold.seconds=seconds;return true;
}
C_Device* CreateDevice(C_DeviceManager* manager,S32 user,E_Device type){auto* m=dynamic_cast<Manager*>(manager);if(!m)return nullptr;
    if(type==E_Device_Keyboard)return new Keyboard(m,user);if(type==E_Device_Mouse)return new Mouse(m,user);if(type==E_Device_Pad360)return new C_Pad360(m,user);return nullptr;}
Events GetEvents(C_DeviceManager* manager){auto* m=dynamic_cast<Manager*>(manager);if(!m)return {};std::weak_ptr<State> weak=m->state;Events e;
    e.key=[weak](U16 code,bool down,U64 modifiers,bool changed){if(auto s=weak.lock()){std::lock_guard<std::mutex> lock(s->mutex);if(!s->pending.focused)return;const S32 key=keyIndex(code);if(key<0)return;
        if(changed){U64 mask=0;switch(code){case kVK_Shift:case kVK_RightShift:mask=NSEventModifierFlagShift;break;case kVK_Control:case kVK_RightControl:mask=NSEventModifierFlagControl;break;case kVK_Option:case kVK_RightOption:mask=NSEventModifierFlagOption;break;case kVK_CapsLock:mask=NSEventModifierFlagCapsLock;break;case kVK_Command:case kVK_RightCommand:mask=NSEventModifierFlagCommand;break;default:return;}if(code==kVK_CapsLock)down=(modifiers&mask)!=0;
        else if(modifiers&mask)down=s->pending.keys[key]==0;
        else {down=false;S32 other=-1;switch(code){case kVK_Shift:other=keyIndex(kVK_RightShift);break;case kVK_RightShift:other=keyIndex(kVK_Shift);break;case kVK_Control:other=keyIndex(kVK_RightControl);break;case kVK_RightControl:other=keyIndex(kVK_Control);break;case kVK_Option:other=keyIndex(kVK_RightOption);break;case kVK_RightOption:other=keyIndex(kVK_Option);break;case kVK_Command:other=keyIndex(kVK_RightCommand);break;case kVK_RightCommand:other=keyIndex(kVK_Command);break;}if(other>=0)s->pending.keys[other]=0;}}
        s->pending.keys[key]=down?1:0;}};
    e.pointer=[weak](F64 x,F64 y){if(!std::isfinite(x)||!std::isfinite(y))return;if(auto s=weak.lock()){std::lock_guard<std::mutex> lock(s->mutex);auto& p=s->pending;if(!p.focused)return;if(p.positioned){p.dx+=x-p.x;p.dy+=y-p.y;}p.x=x;p.y=y;p.positioned=true;}};
    e.button=[weak](S32 b,bool down){if(b<0||b>=5)return;if(auto s=weak.lock()){std::lock_guard<std::mutex> lock(s->mutex);if(s->pending.focused)s->pending.buttons[b]=down?1:0;}};
    e.scroll=[weak](F64 value){if(!std::isfinite(value))return;if(auto s=weak.lock()){std::lock_guard<std::mutex> lock(s->mutex);if(s->pending.focused)s->pending.wheel+=value;}};
    e.focus=[weak](bool active){if(auto s=weak.lock()){std::lock_guard<std::mutex> lock(s->mutex);if(!active){s->pending=Sample{};}s->pending.focused=active;}};
    return e;}
void PollControllerList(C_DeviceManager* manager,void* array){auto* m=dynamic_cast<Manager*>(manager);if(!m)throw std::invalid_argument("Foreign native controller manager");m->controllers.PollList(array);}
}
C_Pad360::C_Pad360(C_DeviceManager* manager,S32 user):C_Device(manager,user,E_Button_Count,E_Axis_Count,E_RumbleMotor_Count),m_StickDeadzone(0.25f){m_DeviceType=E_Device_Pad360;}
C_Pad360::~C_Pad360(){if(auto* manager=dynamic_cast<Mac::Manager*>(m_pDeviceManager))manager->controllers.Rumble(m_UserIndex,0,0);}
void C_Pad360::SetStickDeadzone(F32 value){if(!std::isfinite(value)||value<0||value>=1)throw std::invalid_argument("Native controller deadzone must be in [0,1)");m_StickDeadzone=value;}
void C_Pad360::Update(){auto* manager=dynamic_cast<Mac::Manager*>(m_pDeviceManager);if(!manager)throw std::logic_error("Native pad belongs to a foreign manager");const auto& sample=manager->controllers.Get(m_UserIndex);m_Connected=sample.connected;
    if(!m_Connected||!m_Enabled||!manager->state->sample.focused)ResetValues();else{
        for(U32 i=0;i<E_Button_Count;++i)m_ButtonValues[i]=sample.buttons[i];for(U32 i=0;i<E_Axis_Count;++i)m_AxisValues[i]=sample.axes[i];
        ApplyCircularDeadzone(m_AxisValues[E_Axis_LeftStick_X],m_AxisValues[E_Axis_LeftStick_Y],m_StickDeadzone);ApplyCircularDeadzone(m_AxisValues[E_Axis_RightStick_X],m_AxisValues[E_Axis_RightStick_Y],m_StickDeadzone);
    }UpdateRumble();}
void C_Pad360::UpdateRumble(){auto* manager=dynamic_cast<Mac::Manager*>(m_pDeviceManager);if(!manager)return;const bool active=m_RumbleEnabled&&m_Enabled&&m_Connected&&manager->state->sample.focused;
    if(!manager->controllers.Rumble(m_UserIndex,active?m_RumbleMotorSpeeds[E_RumbleMotor_LowFrequency]:0,active?m_RumbleMotorSpeeds[E_RumbleMotor_HighFrequency]:0)){
        static std::once_flag warning;std::call_once(warning,[]{LOGWARN(g_Log,"Native controller handle haptics unavailable or failed; controller input remains active");});
    }}
void C_Pad360::DumpDeviceState(){LOGINFO(g_Log,"Native pad user %i connected %i",m_UserIndex,m_Connected);for(U32 i=0;i<E_Button_Count;++i)LOGINFO(g_Log,"Button %i = %f",i,m_ButtonValues[i]);for(U32 i=0;i<E_Axis_Count;++i)LOGINFO(g_Log,"Axis %i = %f",i,m_AxisValues[i]);}
C_DeviceManager* CreateDeviceManager(const C_DeviceManagerCreateParam& p){return new Mac::Manager(p);}
}
