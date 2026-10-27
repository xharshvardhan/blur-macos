// [Blur2PC] Original AIR route navigation conventions; no vehicle/game/progress mutations.
#include "../../code/mercury/library/ai/air/StdAfx.h"
#include <trackdata/trackdata.h>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    for(int size:{2,3,8,742}){
        airacingtech::AITrackDataRoutes route;
        route.m_Segments.Resize(size);
        route.SetStepSize(4.f);
        require(route.GetNumSegments()==size && route.GetStepSize()==4.f &&
                route.GetRouteLength()==4.f*size,"Authored route metadata changed");
        route.m_RouteFlags=airacingtech::AITrackDataRoutes::ROUTE_LOOPED;
        require(route.IsLoopedTrack(),"Looped-route flag lost");
        require(route.GetIndexOffset(size-1,1)==1,"Original loop seam must skip sample0");
        for(int index=1;index<size;++index){
            const int next=route.GetIndexOffset(index,1);
            require(next>0 && next<size,"Forward navigation escaped authored segment range");
            if(index<size-1)require(next==index+1,"Interior adjacency skipped an authored sample");
        }
        // Sample0 is an authored boundary point, not the forward navigation seam.
        require(route.GetIndexOffset(0,1)==1,"Original boundary-point adjacency changed");
        route.m_RouteFlags=0;
        require(!route.IsLoopedTrack(),"Open-route flag lost");
        require(route.GetIndexOffset(size-1,1)==size-1,"Open route must clamp its real endpoint");
        require(route.GetIndexOffset(1,-1)==1,"Original open-route lower navigation bound changed");
        for(int index=1;index<size-1;++index)
            require(route.GetIndexOffset(index,1)==index+1,"Open interior adjacency changed");
    }
    std::cout<<"Original AIR authored loop/open route adjacency and metadata PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
