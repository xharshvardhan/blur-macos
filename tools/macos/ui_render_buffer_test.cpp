// [Blur2PC][macOS] Actual original Amax UI buffer lifecycle/attachments over Metal.
#include <core/Maths.h>
#include <display.h>
#include <Ui/UiRenderBuffer.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=32;params.m_Height=24;require(device.Create(params)&&device.CreateVolatile(params),"Original UI fixture Display initialization failed");
    for(auto samples:{Display::E_Multisample_None,Display::E_Multisample_2,Display::E_Multisample_4}){
        UI::C_UiRenderBuffer buffer;require(buffer.GetDepthStencilFormat()==Display::E_Format_D24S8,"Native original UI buffer selected a console depth/stencil format");buffer.SetWidth(19);buffer.SetHeight(11);buffer.SetMultisample(samples);require(buffer.Create()&&buffer.CreateVolatile(),"Original UI volatile attachments could not be created");
        for(U32 pass=0;pass<2;++pass){
            const U32 width=buffer.GetWidth(),height=buffer.GetHeight(),colour=0xff123456+pass*0x111111;
            require(device.BeginScene()&&buffer.Bind()&&buffer.IsBound(),"Original UI buffer could not bind its real attachments");const auto& target=device.GetRenderTarget();require(target.GetColourSurface()->GetWidth()==width&&target.GetColourSurface()->GetHeight()==height&&target.GetColourSurface()->GetMultisample()==static_cast<Display2::E_Multisample>(samples)&&target.GetDepthStencilSurface()->GetMultisample()==static_cast<Display2::E_Multisample>(samples)&&target.GetDepthStencilFormat()==Display::E_Format_D24S8,"Original UI colour/depth dimensions/formats/sample counts differ");
            auto& context=device.GetDisplay2().GetContext();context.Clear(Display2::E_Clear_Colour|Display2::E_Clear_Depth|Display2::E_Clear_Stencil,colour,.5f,0x7a);context.SetViewport(Display2::C_Viewport(3,2,width-3,height-2));context.Clear(Display2::E_Clear_Colour,0xff9abcde);buffer.Unbind();require(!buffer.IsBound(),"Original UI buffer unbind retained logical binding");device.SetColourDepthStencilSurface();device.EndScene();buffer.FinalResolve();
            const auto& texture=buffer.GetTexture();require(texture.GetWidth()==width&&texture.GetHeight()==height,"Original UI sampling texture lost real framebuffer dimensions");auto lock=texture.LockRect2D(0,16);require(lock.pBits,"Original UI rendered/AA-resolved texture GPU readback failed");bool valid=true;for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 pixel;std::memcpy(&pixel,lock.pBits+y*lock.pitch+x*4,4);valid&=pixel==(x>=3&&y>=2?0xff9abcde:colour);}texture.UnlockRect2D(0);require(valid,"Original UI single/MSAA resolved GPU pixels differ from actual clears");
            buffer.DestroyVolatile();if(!pass){buffer.SetWidth(23);buffer.SetHeight(9);require(buffer.CreateVolatile(),"Original UI resized volatile recreation failed");}
        }
        buffer.Destroy();
    }
    require(device.GetDisplay2().WaitIdle(),"Original UI attachment GPU work failed");
#if TRACK_DISPLAY2_RESOURCES
    require(device.GetDisplay2().GetRenderTexture2DCount()==0,"Original UI buffer leaked owned GPU render textures");
#endif
    device.DestroyVolatile();device.Destroy();std::cout<<"Original C_UiRenderBuffer: PC D24S8, actual matched None/2x/4x colour/depth/stencil attachments, GPU clear/resolve pixels, padded row readback, resize/recreation/teardown passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
