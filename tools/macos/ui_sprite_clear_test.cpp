// [Blur2PC][macOS] Original clear macro across adjacent LP64 sprite storage.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <display.h>
#include <ui/Graphics/Simple2d.h>
#include <ui/Graphics/Simple2d.inl>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    using UI::C_Sprite;
    static_assert(sizeof(C_Sprite)>128,"Fixture requires original LP64 sprite layout");
    constexpr size_t count=8;
    alignas(128) std::array<U8,sizeof(C_Sprite)*count+256> memory;
    // Start one full cache line in, so both neighboring guards are addressable.
    auto* begin=memory.data()+128;
    for(size_t i=0;i<count;++i)new(begin+i*sizeof(C_Sprite)) C_Sprite;
    for(size_t target=0;target<count;++target){
        memory.fill(0x11);
        auto* sprite=reinterpret_cast<C_Sprite*>(begin+target*sizeof(C_Sprite));
        UI_CLEAR_SPRITE(sprite); // Actual production macro, including native selection.
        require(sprite->m_PreTransformIndex==0&&sprite->m_PostTransformIndex==0,"Clear left stale pre/post transform indices");
        const size_t offset=128+target*sizeof(C_Sprite);
        for(size_t i=0;i<memory.size();++i)
            require(memory[i]==(i>=offset&&i<offset+sizeof(C_Sprite)?0:0x11),"Sprite clear missed its tail or modified an adjacent sprite/guard");
        // A second clear must preserve the same exact object boundary.
        UI_CLEAR_SPRITE(sprite);
        for(size_t i=0;i<memory.size();++i)
            require(memory[i]==(i>=offset&&i<offset+sizeof(C_Sprite)?0:0x11),"Repeated clear changed an adjacent object");
    }
    std::cout<<"Original native sprite clear: "<<sizeof(C_Sprite)<<" bytes, adjacent poison/guards, pre/post transform indices and repeated reuse PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
