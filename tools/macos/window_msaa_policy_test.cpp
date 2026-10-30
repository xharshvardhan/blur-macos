// [Blur2PC][macOS] Strict startup policy grammar and authored-value preservation.
#include <display/device/metal/MetalWindowMSAAPolicy.h>
#include <iostream>
#include <initializer_list>
#include <stdexcept>

int main() {
    try {
        auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
        unsigned selected=99;
        require(Display::Metal::SelectWindowMultisample(nullptr,0,selected)&&selected==0,"Unset policy changed native authored None");
        require(Display::Metal::SelectWindowMultisample("authored",2,selected)&&selected==2,"Authored policy discarded explicit caller samples");
        require(Display::Metal::SelectWindowMultisample("2",0,selected)&&selected==2,"2x policy not selected");
        require(Display::Metal::SelectWindowMultisample("4",2,selected)&&selected==4,"4x policy not selected");
        for(const char* invalid:{"","0","1","8","2x","2.0"," 2","2 ","AUTHORED","4\n"}){
            selected=99;
            require(!Display::Metal::SelectWindowMultisample(invalid,0,selected)&&selected==99,"Invalid MSAA silently selected fallback");
        }
        std::cout<<"Native window MSAA strict policy/authored default/output-on-failure PASS (CPU only)\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
