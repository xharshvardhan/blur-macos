// [Blur2PC][macOS] Original asset-platform validation and optional real PC model metadata deserialisation.
#include <core/Core.h>
#include <core/Maths.h>
#include <system/serialise.h>
#include <system/serialise/C_FileBlock.h>
#include <system/misc/Platform.h>
#include <game_resource/GameResource.h>
#include <game_resource/ModelData.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){try{
    require(argc==2,"Expected authored platform fixture directory");const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(root.c_str()),"Platform fixture filesystem setup failed");
    require(Util::GetPlatform()==Util::E_Platform_PC&&Util::GetPlatformString(Util::GetPlatform())=="pc","Native target did not select the actual PC asset serializer contract");
    auto authored=[&](U32 tag,bool accepted,U32 offset){const auto path=root/"platform.bin";{std::ofstream out(path,std::ios::binary);for(U32 value:{tag,0x12345678U})for(U32 c=0;c<4;++c)out.put(char(value>>(8*c)));require(bool(out),"Platform fixture write failed");}std::unique_ptr<Sys::I_File> file(fs.Open(path.c_str(),Sys::E_FileAccess_Read));require(file&&GameResource::ValidatePlatformId(file.get())==accepted&&file->GetPosition()==offset,"Original asset validator accepted the wrong platform/endianness or moved to the wrong byte");};
    const U32 pc=Util::GetPlatformFourCC(Util::E_Platform_PC);authored(pc,true,4);authored(Util::GetPlatformFourCC(Util::E_Platform_PS3),false,4);authored(Util::GetPlatformFourCC(Util::E_Platform_360),false,4);authored((pc>>24)|((pc>>8)&0xff00)|((pc<<8)&0xff0000)|(pc<<24),false,4);authored(0x7f654321,true,0);
    if(const char* retail=std::getenv("BLUR_MAC_MODEL_RETAIL_DIR")){for(const char* name:{"teamcolourcolumn_red.model","start_text_start_en.model"}){
        const auto path=std::filesystem::path(retail)/name;std::unique_ptr<Sys::I_File> file(fs.Open(path.c_str(),Sys::E_FileAccess_Read));require(file&&GameResource::ValidatePlatformId(file.get())&&file->GetPosition()==4,"Original installed PC model platform header failed validation");Sys::C_FileBlock model(file.get());require(model.HeaderId()=="Model","Original PC model block header was not read with its real disk layout");bool header=false,data=false,render=false;U32 children=0;
        while(model.GetPosition()<model.Length()){Sys::C_FileBlock block(&model);++children;if(block.HeaderId()=="Header"){Sys::BinaryDeserialiser<Ut::NoEndianSwap> ar(block);U32 version;ar>>version;require(version==0,"Unexpected original PC model file version");file->SetVersionNumber(version);header=true;}else if(block.HeaderId()=="MdlDat"){
            GameResource::C_ModelData metadata;require(metadata.Read(&block)&&metadata.GetModelCount()==1&&metadata.GetConstraintCount()==0,"Original PC model metadata reader failed native field/index deserialisation");const auto* rootModel=metadata.GetRootModel();require(rootModel&&rootModel->GetElementCount()==1&&rootModel->GetName()[0]&&rootModel->GetElement(0)&&rootModel->GetElement(0)->GetModel()==rootModel&&rootModel->GetElement(0)->GetName().Length()>0,"Original PC model strings/native parent pointers were not reconstructed from disk indices");data=true;
        }else if(block.HeaderId()=="Render"){Sys::C_FileBlock rendering(&block);require(rendering.HeaderId()=="Render","Original PC nested rendering block layout differs from its real serializer");render=true;}}
        require(header&&data&&render&&children==4,"Original PC model block traversal lost required payloads");std::cout<<"Original "<<name<<": PC tag, fixed-width block headers, field/index/string metadata and native pointer reconstruction passed\n";
    }}
    Sys::SetDefaultFileSystem(nullptr);std::cout<<"Original resource platform: native PC selection, strict PC/PS3/360/endian validation and legacy rewind passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
