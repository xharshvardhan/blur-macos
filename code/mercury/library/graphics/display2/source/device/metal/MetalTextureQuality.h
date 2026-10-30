// [Blur2PC][macOS] Optional faithful texture quality; authored filtering remains default.
#pragma once
#include <display2/context/MetalTextures.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace Display2::Metal {
class TextureQualityPolicy {
public:
    static U32 Parse(const char* value){
        if(!value||!*value||std::strcmp(value,"authored")==0)return 0;
        if(std::strcmp(value,"4")==0)return 4;
        if(std::strcmp(value,"8")==0)return 8;
        if(std::strcmp(value,"16")==0)return 16;
        throw std::invalid_argument("BLUR_MAC_TEXTURE_ANISOTROPY must be authored,4,8 or16");
    }
    static bool Eligible(const C_SamplerState& state){
        const bool mag=state.m_TextureFilterMag==E_TextureFilter_Linear||state.m_TextureFilterMag==E_TextureFilter_Anisotropic;
        const bool min=state.m_TextureFilterMin==E_TextureFilter_Linear||state.m_TextureFilterMin==E_TextureFilter_Anisotropic;
        return mag&&min&&(state.m_TextureFilterMag==E_TextureFilter_Anisotropic||state.m_TextureFilterMin==E_TextureFilter_Anisotropic)&&
            (state.m_TextureFilterMip==E_TextureFilter_Point||state.m_TextureFilterMip==E_TextureFilter_Linear);
    }
    TextureQualityPolicy():TextureQualityPolicy(std::getenv("BLUR_MAC_TEXTURE_ANISOTROPY"),StatsEnabled()){}
    TextureQualityPolicy(const char* value,bool stats):m_Floor(Parse(value)),m_Stats(stats){}
    ~TextureQualityPolicy(){
        if(m_Stats)std::fprintf(stderr,"blur2pc.texture_quality mode=%s requests=%llu eligible=%llu promoted=%llu eligible_max_requested=%u eligible_max_effective=%u\n",
            m_Floor==4?"4":m_Floor==8?"8":m_Floor==16?"16":"authored",
            static_cast<unsigned long long>(m_Requests),static_cast<unsigned long long>(m_Eligible),static_cast<unsigned long long>(m_Promoted),m_MaxRequested,m_MaxEffective);
    }
    TextureQualityPolicy(const TextureQualityPolicy&)=delete;
    TextureQualityPolicy& operator=(const TextureQualityPolicy&)=delete;
    U32 Select(const C_SamplerState& state,U32 requested){
        // Callers already validate requested1..16. Default-off work stays one
        // branch; never rewrite an authored point/linear filter or mip policy.
        if(!m_Floor&&!m_Stats)return requested;
        const bool eligible=Eligible(state);const U32 selected=eligible?std::max(requested,m_Floor):requested;
        if(m_Stats){++m_Requests;if(eligible){++m_Eligible;if(selected>requested)++m_Promoted;m_MaxRequested=std::max(m_MaxRequested,requested);m_MaxEffective=std::max(m_MaxEffective,selected);}}
        return selected;
    }
private:
    static bool StatsEnabled(){const char* value=std::getenv("BLUR_MAC_TEXTURE_QUALITY_STATS");return value&&std::strcmp(value,"1")==0;}
    U32 m_Floor=0;bool m_Stats=false;
    U64 m_Requests=0,m_Eligible=0,m_Promoted=0;U32 m_MaxRequested=0,m_MaxEffective=0;
};
}
