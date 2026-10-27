// [Blur2PC] Original JPEG encoder/decoder against maintained native libjpeg.
#include <core/Core.h>
#include <JpegHelper/JpegEncoder.h>
#include <JpegHelper/JpegDecoder.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try{
    constexpr U32 width=32,height=24,pitch=width*4+16;
    std::array<U8,pitch*height> source{},destination;source.fill(0x6a);destination.fill(0x5b);
    const U32 colour=0xffd23014;
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)std::memcpy(source.data()+y*pitch+x*4,&colour,4);
    JpegHelper::C_JpegEncoder encoder;encoder.SetQuality(100);
    const char comment[]="Authored native photo";encoder.SetComments(comment,sizeof(comment));
    require(encoder.EncodeFromBuffer(source.data(),pitch,width,height,Display::E_Format_A8R8G8B8),"Original JPEG encoding failed");
    require(encoder.JpegLength()>100&&encoder.JpegData()[0]==0xff&&encoder.JpegData()[1]==0xd8,"Original encoder did not produce a JPEG");
    JpegHelper::C_JpegDecoder decoder;decoder.SetData(encoder.JpegData(),encoder.JpegLength());
    require(decoder.DecodeIntoBuffer(destination.data(),pitch,width,height,Display::E_Format_A8R8G8B8),"Original JPEG decoding failed");
    for(U32 y=0;y<height;++y){for(U32 x=0;x<width;++x){const U8* p=destination.data()+y*pitch+x*4;
        require(std::abs(int(p[0])-20)<=3&&std::abs(int(p[1])-48)<=3&&std::abs(int(p[2])-210)<=3&&p[3]==255,"Native JPEG colour channel/alpha conversion changed");}
        for(U32 x=width*4;x<pitch;++x)require(destination[y*pitch+x]==0x5b,"Original JPEG decoder overwrote row padding");}
    require(decoder.CommentsSize()==sizeof(comment)&&std::memcmp(decoder.Comments(),comment,sizeof(comment))==0,"Original JPEG comment bytes changed");
    U8 bad[4]={1,2,3,4};decoder.SetData(bad,sizeof(bad));
    require(!decoder.DecodeIntoBuffer(destination.data(),pitch,width,height,Display::E_Format_A8R8G8B8),"Malformed JPEG reported success");
    decoder.Exit();encoder.Exit();std::cout<<"Original JPEG/native libjpeg photo bytes, channels, comments, padding and rejection PASS\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
