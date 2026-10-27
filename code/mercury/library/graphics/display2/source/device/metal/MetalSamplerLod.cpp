// [Blur2PC] Keep PC sampler bias distinct from instruction bias/explicit LOD.
#include <display2/context/MetalSamplerLod.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace BlurMac::SamplerLod {
namespace {
using U32=std::uint32_t;
struct Part { std::size_t begin,end; };
// Mask comments/string literals without changing offsets. Delimiters in those
// regions cannot change entry/call boundaries or masquerade as sampling code.
std::string codeOnly(const std::string& s) {
    std::string c=s;
    for(std::size_t i=0;i<c.size();) {
        if(c[i]=='/' && i+1<c.size() && (c[i+1]=='/'||c[i+1]=='*')) {
            const bool line=c[i+1]=='/';const auto start=i;i+=2;
            while(i<c.size() && (line?c[i]!='\n':!(c[i]=='*'&&i+1<c.size()&&c[i+1]=='/')))++i;
            if(!line && i<c.size())i+=2;
            std::fill(c.begin()+start,c.begin()+i,' ');
        } else if(c[i]=='"'||c[i]=='\'') {
            const char quote=c[i];const auto start=i++;
            while(i<c.size()){if(c[i]=='\\'){i=std::min(i+2,c.size());continue;}if(c[i++]==quote)break;}
            std::fill(c.begin()+start,c.begin()+i,' ');
        } else ++i;
    }
    return c;
}
std::size_t close(const std::string& c,std::size_t open,char a='(',char b=')') {
    U32 depth=1;for(auto i=open+1;i<c.size();++i){if(c[i]==a)++depth;else if(c[i]==b&&! --depth)return i;}
    throw std::runtime_error("Native sampler integration found unbalanced source");
}
std::string trim(const std::string& s){const auto first=s.find_first_not_of(" \t\r\n");return first==s.npos?"":s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);}
std::vector<std::string> arguments(const std::string& s,std::size_t begin,std::size_t end) {
    const auto c=codeOnly(s);int paren=0,bracket=0,brace=0;std::vector<std::string> result;
    auto last=begin;
    for(auto i=begin;i<end;++i){switch(c[i]){case '(':++paren;break;case ')':--paren;break;case '[':++bracket;break;case ']':--bracket;break;case '{':++brace;break;case '}':--brace;break;case ',':if(!paren&&!bracket&&!brace){result.push_back(trim(s.substr(last,i-last)));last=i+1;}break;}}
    if(last<end)result.push_back(trim(s.substr(last,end-last)));return result;
}
bool constructor(const std::string& value,const std::string& name,std::vector<std::string>& args){
    auto c=codeOnly(value);std::smatch match;if(!std::regex_search(c,match,std::regex("^\\s*"+name+"\\s*\\(")))return false;
    const auto open=c.find('(',match.position());const auto end=close(c,open);if(!trim(c.substr(end+1)).empty())return false;
    args=arguments(value,open+1,end);return true;
}
}
Integration Integrate(const std::string& source,const std::string& entry,bool vertex,U32 samplerMask,U32 buffers) {
    Integration out;out.source=source;if(!samplerMask)return out;
    if(entry.empty()||!std::all_of(entry.begin(),entry.end(),[](unsigned char c){return std::isalnum(c)||c=='_';}))throw std::runtime_error("Invalid native sampler entry name");
    auto code=codeOnly(source);std::smatch signature;
    if(!std::regex_search(code,signature,std::regex("\\b(?:vertex|fragment)\\s+\\w+\\s+"+entry+"\\s*\\("))) {
        out.unsupportedSamplers=samplerMask;return out;
    }
    const auto open=code.find('(',signature.position()),end=close(code,open);
    auto body=code.find_first_not_of(" \t\r\n",end+1);
    if(body==code.npos||code[body]!='{')throw std::runtime_error("Native sampler entry body missing");
    const auto bodyEnd=close(code,body,'{','}');
    std::unordered_map<std::string,U32> slots;
    const auto parameters=code.substr(open+1,end-open-1);
    const std::regex sampler(R"(\bsampler\s+(\w+)\s*\[\[\s*sampler\s*\(\s*(\d+)\s*\)\s*\]\])");
    for(auto it=std::sregex_iterator(parameters.begin(),parameters.end(),sampler);it!=std::sregex_iterator();++it){const U32 slot=std::stoul((*it)[2]);if(slot>=16)throw std::runtime_error("Native sampler slot exceeds PC limit");slots.emplace((*it)[1],slot);}
    struct Call {std::size_t open;std::string method;};std::vector<Call> calls;
    const std::regex sample(R"(\.\s*(sample\w*|gather\w*)\s*\()");
    for(auto it=std::sregex_iterator(code.begin(),code.end(),sample);it!=std::sregex_iterator();++it){
        const auto at=std::size_t(it->position());
        if(at<body||at>bodyEnd){out.unsupportedSamplers|=samplerMask;continue;}
        calls.push_back({code.find('(',at),(*it)[1]});
    }
    if(calls.empty())return out;
    for(U32 binding=16;binding;--binding)if(!(buffers&(1u<<(binding-1)))){out.buffer=binding-1;break;}
    if(out.buffer==0xffffffffu)throw std::runtime_error("Native sampler LOD state has no free constant binding");
    if(code.find("blur_pc_sampler_lod")!=code.npos)throw std::runtime_error("Native sampler LOD private identifier collision");
    // Descending edits preserve every earlier opening offset, including outer
    // calls whose coordinates contain another texture sample.
    for(auto it=calls.rbegin();it!=calls.rend();++it){
        code=codeOnly(out.source);const auto callEnd=close(code,it->open);auto args=arguments(out.source,it->open+1,callEnd);
        if(args.size()<2||!slots.count(args[0])||it->method!="sample") {out.unsupportedSamplers|=samplerMask;continue;}
        const U32 slot=slots.at(args[0]);const std::string state="blur_pc_sampler_lod["+std::to_string(slot)+"]";
        std::vector<std::string> option;bool integrated=false;
        if(args.size()>2&&constructor(args[2],"level",option)&&option.size()==1){
            args[2]="level(clamp(("+option[0]+") + "+state+".x, 0.0f, "+state+".y))";integrated=true;
        }else if(args.size()>2&&constructor(args[2],"bias",option)&&option.size()==1&&!vertex){
            args[2]="bias(("+option[0]+") + "+state+".x)";integrated=true;
        }else if(args.size()>2){
            for(const char* kind:{"gradient2d","gradient3d","gradientcube"})if(constructor(args[2],kind,option)&&option.size()==2){
                args[2]=std::string(kind)+"(("+option[0]+") * exp2("+state+".x), ("+option[1]+") * exp2("+state+".x))";
                out.gradientSamplers|=1u<<slot;integrated=true;break;
            }
        }
        if(!integrated){
            // Two arguments, or the third is a texel offset rather than an LOD
            // qualifier. Preserve that offset after the newly supplied option.
            const bool implicit=args.size()==2||(args.size()==3&&(trim(args[2]).rfind("int",0)==0));
            if(!implicit){out.unsupportedSamplers|=1u<<slot;continue;}
            args.insert(args.begin()+2,vertex?"level(clamp("+state+".x, 0.0f, "+state+".y))":"bias("+state+".x)");
        }
        std::string joined;for(const auto& arg:args){if(!joined.empty())joined+=", ";joined+=arg;}
        out.source.replace(it->open+1,callEnd-it->open-1,joined);
    }
    // No declarations are moved: existing alpha/HDR/authored register ABIs stay
    // intact. CPU supplies float2[16] at the independently reflected free slot.
    out.source.insert(end,std::string(trim(parameters).empty()?"":", ")+"constant float2* blur_pc_sampler_lod [[buffer("+std::to_string(out.buffer)+")]]");
    return out;
}
}
