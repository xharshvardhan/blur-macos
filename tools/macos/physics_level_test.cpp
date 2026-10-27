// [Blur2PC][macOS] Optional private retail level read through original loaders.
// Supply a game installation path; no game assets are included by this fixture.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/Maths/HVec/New/IncludePhysics.h>
#include <core/Maths/HVec/New/IncludePhysics.inl>
#include <core/containers.h>
#include <core/threads.h>
#include <system/serialise.h>
#include <system/serialise/C_FileBlock.h>
#include <system/serialise/macos/NativeFile.h>
#include <gravity/sdk/PhysicsSDK.h>
#include <gravity/sdk/ShapeDescriptors.h>
#include <gravity/narrowphase/OptimisedAABBTree.h>
#include <game_resource/PhysicsLevel.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <utility>
#include <iostream>
#include <stdexcept>
namespace GameResource { bool ValidatePlatformId(Sys::I_File*); }
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool assertion(const char* expression,const char*,const char*,int){throw std::runtime_error(expression);}
static void validateTree(Collision::OptimisedAABBTree& tree){
    static_assert(sizeof(Collision::C_TriangleHeader)==16&&sizeof(Collision::AABBTreeTriangle)==12&&sizeof(Collision::OptimisedAABBNode)==32,
                  "The original in-memory triangle stream layout changed");
    require(tree.m_pNodes&&tree.m_pVertices&&tree.m_Triangles.m_pStream,"Missing native mesh allocation");
    for(auto axis:{tree.m_Low.X(),tree.m_Low.Y(),tree.m_Low.Z(),tree.m_High.X(),tree.m_High.Y(),tree.m_High.Z()}){
        F32 value;Maths::Store(axis,&value);require(std::isfinite(value),"Imported global mesh bounds are nonfinite");
    }
    require(bool(tree.m_Low.All_LE(tree.m_High)),"Imported global mesh bounds are inverted");
    std::vector<U32> pending{0};std::vector<bool> visited(tree.m_NumNodes,false);
    std::vector<std::pair<U32,U32>> ranges;U64 triangles=0,leaves=0;
    while(!pending.empty()){
        const U32 index=pending.back();pending.pop_back();
        require(index<tree.m_NumNodes&&!visited[index],"Imported AABB child is outside the tree or repeats a node");visited[index]=true;
        auto& node=tree.m_pNodes[index];
        if(node.m_TriangleSizeIndices==0){
            require(node.m_RightChildOffset>1&&node.m_RightChildOffset<tree.m_NumNodes-index,"Imported AABB child offset is invalid");
            pending.push_back(index+node.m_RightChildOffset);pending.push_back(index+1);continue;
        }
        ++leaves;
        require(node.m_NumVertices>0&&node.m_NumVertices<=256&&node.m_VerticeStartIndex<tree.m_NumVertices&&node.m_NumVertices<=tree.m_NumVertices-node.m_VerticeStartIndex,"Imported leaf vertex range is invalid");
        require(node.m_TriangleStartIndex<tree.m_NumTriangles&&node.m_TriangleStreamLength<=tree.m_NumTriangles-node.m_TriangleStartIndex,"Imported leaf triangle range is invalid");
        Collision::C_AxisBoxMinMax box;node.m_BoundingBox.LoadBox(box,tree.m_Low,tree.m_High);
        require(bool(box.m_min.All_LE(box.m_max)),"Imported quantized leaf bounds are inverted or nonfinite");
        U32 offset=node.m_TriangleStartIndex,count=0;const U32 end=offset+node.m_TriangleStreamLength;
        ranges.push_back({offset,end});
        while(count<node.m_TriangleSizeIndices){
            require(sizeof(Collision::C_TriangleHeader)<=end-offset,"Truncated imported triangle header");
            Collision::C_TriangleHeader header;std::memcpy(&header,tree.m_Triangles.m_pStream+offset,sizeof(header));offset+=sizeof(header);
            require(header.m_NumTriangles>0&&header.m_NumTriangles<=node.m_TriangleSizeIndices-count,"Imported triangle header count exceeds its leaf");
            for(U32 t=0;t<header.m_NumTriangles;++t){
                require(sizeof(Collision::AABBTreeTriangle)<=end-offset,"Truncated imported triangle");
                Collision::AABBTreeTriangle triangle;std::memcpy(&triangle,tree.m_Triangles.m_pStream+offset,sizeof(triangle));offset+=sizeof(triangle);
                require(triangle.m_IndexA<node.m_NumVertices&&triangle.m_IndexB<node.m_NumVertices&&triangle.m_IndexC<node.m_NumVertices,"Imported triangle vertex index exceeds its leaf");
                if(triangle.m_ActiveEdgeDataIndex&7)require(triangle.ActiveEdgeDataIndex()<tree.m_NumActiveEdges,"Imported active edge index exceeds its array");++count;++triangles;
            }
        }
        require(offset==end,"Imported triangle stream has unaccounted bytes");
    }
    require(std::find(visited.begin(),visited.end(),false)==visited.end(),"Imported tree contains unreachable nodes");
    std::sort(ranges.begin(),ranges.end());U32 end=0;for(auto range:ranges){require(range.first==end,"Imported leaf triangle ranges overlap or leave a gap");end=range.second;}
    require(end==tree.m_NumTriangles,"Imported leaves do not cover the complete triangle stream");
    std::cout<<" triangles="<<triangles<<" leaves="<<leaves<<" tree_bounds_valid=true\n";
}
static void clippedPayload(const char* path){
    auto file=Sys::NativeFile::Open(path);require(GameResource::ValidatePlatformId(file.get()),"Original platform validation failed on reread");
    std::vector<U8> payload;
    {
        Sys::C_FileBlock root(file.get());require(root.HeaderId()=="Physic","Expected original physics container");
        while(root.GetPosition()<root.Length()){
            Sys::C_FileBlock block(&root);
            if(block.HeaderId()=="Main"){
                payload.resize(block.Length()-block.GetPosition());require(block.Read(payload.data(),U32(payload.size())),"Original Main payload read failed");
            }else block.SetPosition(block.Length());
        }
    }
    require(payload.size()>1,"Original Main payload is missing");payload.pop_back();
    auto clipped=Sys::NativeFile::FromMemory(std::move(payload),"private truncated actual Main payload");
    auto* sdk=Gravity::C_PhysicsSDK::PhysicsSDK();auto* scene=sdk->CreateImportScene();
    bool rejected=false;
    try{scene->Import(clipped.get());}
    catch(const std::runtime_error& error){rejected=std::string(error.what()).find("unexpected EOF")!=std::string::npos;}
    sdk->ReleaseImportScene(scene);require(rejected,"Original reader accepted a truncated final scalar");
}
int main(int argc,char** argv){try{
    require(argc==2,"Supply private Level.levelphysics path");g_pCoreAssertFunction=assertion;
    auto file=Sys::NativeFile::Open(argv[1],Sys::E_FileAccess_Read,37);
    require(GameResource::ValidatePlatformId(file.get()),"Original platform validation rejected level");
    require(file->GetPosition()==4,"PC platform marker did not consume exactly4 bytes");
    GameResource::C_PhysicsLevel level;require(level.Load(file.get()),"Original physics level load failed");
    require(file->GetPosition()==file->Length(),"Original physics reader did not consume the complete file");
    require(file->GetVersionNumber()==0,"Successful physics load did not restore the file version");
    auto* scene=level.GetMainImportScene();require(scene,"No original main physics scene imported");
    std::cout<<"bytes="<<file->Length()<<" static="<<scene->m_StaticShapesDesc.m_numElements<<" dynamic="<<scene->m_DynamicShapes.m_numElements<<" constraints="<<scene->GetNumConstraintCount()<<" routes="<<level.GetRouteCount()<<'\n';
    for(S32 i=0;i<scene->m_StaticShapesDesc.m_numElements;++i){
        auto* desc=scene->m_StaticShapesDesc[i];require(desc,"Missing static descriptor");
        std::cout<<"static["<<i<<"] type="<<desc->m_Type<<'\n';
        if(desc->m_Type==Collision::E_TriangleMesh_Shape){auto* tree=static_cast<Gravity::C_TriangleMeshDesc*>(desc)->m_OptimisedTree;
            require(tree&&tree->m_NumNodes&&tree->m_NumVertices,"Empty original triangle mesh");
            require(reinterpret_cast<std::uintptr_t>(tree->m_pNodes)%SharedPhysics::SmallCachePageSize==0,"Native node cache alignment changed");
            validateTree(*tree);
            std::cout<<" vertices="<<tree->m_NumVertices<<" normals="<<tree->m_NumNormals<<" triangle_stream_bytes="<<tree->m_NumTriangles<<" nodes="<<tree->m_NumNodes<<" active_edges="<<tree->m_NumActiveEdges<<'\n';
        }
    }
    clippedPayload(argv[1]);
    std::cout<<"Original actual physics level load/destruction/truncated-payload rejection PASS (not a simulation or race)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
