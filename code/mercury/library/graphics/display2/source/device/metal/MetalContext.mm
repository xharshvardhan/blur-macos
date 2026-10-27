// [Blur2PC][macOS] Actual engine draw entry points, pipeline state and command ownership.
#include "MetalNative.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
namespace Display2 {
namespace {
void require(bool value,const char* message){if(!value)throw std::logic_error(message);}
MTLVertexFormat vertexFormat(U32 type) {
    switch(type) {
    case 0:return MTLVertexFormatFloat;case 1:return MTLVertexFormatFloat2;case 2:return MTLVertexFormatFloat3;case 3:return MTLVertexFormatFloat4;
    case 4:return MTLVertexFormatUChar4Normalized_BGRA;case 5:return MTLVertexFormatUChar4;
    case 6:return MTLVertexFormatShort2;case 7:return MTLVertexFormatShort4;case 8:return MTLVertexFormatUChar4Normalized;
    case 9:return MTLVertexFormatShort2Normalized;case 10:return MTLVertexFormatShort4Normalized;
    case 11:return MTLVertexFormatUShort2Normalized;case 12:return MTLVertexFormatUShort4Normalized;
    case 15:return MTLVertexFormatHalf2;case 16:return MTLVertexFormatHalf4;default:return MTLVertexFormatInvalid;
    }
}
MTLBlendFactor blendFactor(U32 value) {
    switch(value){case E_Blend_Zero:return MTLBlendFactorZero;case E_Blend_One:return MTLBlendFactorOne;
    case E_Blend_SrcColour:return MTLBlendFactorSourceColor;case E_Blend_InvSrcColour:return MTLBlendFactorOneMinusSourceColor;
    case E_Blend_SrcAlpha:return MTLBlendFactorSourceAlpha;case E_Blend_InvSrcAlpha:return MTLBlendFactorOneMinusSourceAlpha;
    case E_Blend_DstAlpha:return MTLBlendFactorDestinationAlpha;case E_Blend_InvDstAlpha:return MTLBlendFactorOneMinusDestinationAlpha;
    case E_Blend_DstColour:return MTLBlendFactorDestinationColor;case E_Blend_InvDstColour:return MTLBlendFactorOneMinusDestinationColor;
    case E_Blend_SrcAlphaSat:return MTLBlendFactorSourceAlphaSaturated;default:throw std::invalid_argument("Unsupported blend factor");}
}
MTLBlendOperation blendOperation(U32 value) {
    switch(value){case E_BlendOp_Add:return MTLBlendOperationAdd;case E_BlendOp_Subtract:return MTLBlendOperationSubtract;
    case E_BlendOp_RevSubtract:return MTLBlendOperationReverseSubtract;case E_BlendOp_Min:return MTLBlendOperationMin;
    case E_BlendOp_Max:return MTLBlendOperationMax;default:throw std::invalid_argument("Unsupported blend operation");}
}
MTLPrimitiveType primitiveType(E_PrimitiveType type) {
    switch(type){case E_PrimitiveType_PointList:return MTLPrimitiveTypePoint;case E_PrimitiveType_LineList:return MTLPrimitiveTypeLine;
    case E_PrimitiveType_LineStrip:return MTLPrimitiveTypeLineStrip;case E_PrimitiveType_TriangleList:case E_PrimitiveType_TriangleFan:return MTLPrimitiveTypeTriangle;
    case E_PrimitiveType_TriangleStrip:return MTLPrimitiveTypeTriangleStrip;default:throw std::invalid_argument("Unknown primitive type");}
}
MTLColorWriteMask colourMask(U32 value) {
    return MTLColorWriteMask(((value&1)?MTLColorWriteMaskRed:0)|((value&2)?MTLColorWriteMaskGreen:0)|
                            ((value&4)?MTLColorWriteMaskBlue:0)|((value&8)?MTLColorWriteMaskAlpha:0));
}
}
struct C_Context::Impl {
    C_MetalDevice::State* device;
    id<MTLCommandBuffer> command;
    id<MTLRenderCommandEncoder> encoder;
    std::thread::id owner;
    std::array<std::shared_ptr<Metal::Surface>,4> colours;
    std::shared_ptr<Metal::Surface> depth;
    struct Stream { std::shared_ptr<Metal::Buffer> buffer;U32 offset=0,stride=0; };
    std::array<Stream,8> streams;
    std::shared_ptr<Metal::Buffer> indices;
    E_IndexFormat indexFormat=E_IndexFormat_Unknown;
    std::shared_ptr<Metal::VertexDeclaration> declaration;
    std::shared_ptr<Metal::Shader> vertex,pixel;
    struct TextureState { std::shared_ptr<Metal::Texture> texture;U32 flags=0;C_SamplerState sampler;U32 anisotropy=0; };
    std::array<TextureState,16> pixelTextures,vertexTextures;
    U32 maxAnisotropy=1; // D3D9 D3DSAMP_MAXANISOTROPY default.
    std::unordered_map<std::string,id<MTLSamplerState>> samplers;
    std::vector<F32> vertexConstants=std::vector<F32>(1024,0),pixelConstants=std::vector<F32>(1024,0);
    C_Viewport viewport;
    bool viewportExplicit=false,blendEnabled=false,stencilEnabled=false,alphaToCoverageEnabled=false;
    C_AlphaTestState alphaState;C_BlendState blendState;C_ColourState colourState;C_CullState cullState;
    std::array<U32,4> colourMasks={15,15,15,15}; // [Blur2PC] Keep serialized C_ColourState unchanged; native MRT masks are live context state.
    C_DepthState depthState;C_FillState fillState;C_ScissorState scissorState;C_StencilState stencilState;
    struct Alpha {U32 enabled=0,function=8;F32 reference=0;U32 format=8;} alpha;
    std::array<F32,4> hdr={};
    std::vector<std::shared_ptr<Metal::Buffer>> usedBuffers;
    std::vector<std::shared_ptr<Metal::Surface>> usedSurfaces;
    std::shared_ptr<C_OcclusionQuery::State> activeQuery; // [Blur2PC] Encoder retains retired query storage.
    struct Pipeline {
        id<MTLRenderPipelineState> native;
        std::array<U32,16> vertexUniformBytes={},pixelUniformBytes={};
    };
    std::unordered_map<std::string,Pipeline> pipelines;
    // Scene ownership serializes draws. Reuse the lookup bytes on cache hits;
    // each inserted map key still owns an independent copy.
    std::string pipelineKeyScratch;
    std::unordered_map<std::string,id<MTLRenderPipelineState>> clearPipelines;
    explicit Impl(C_MetalDevice::State* state):device(state){}
    void check(bool active=false)const {
        require(!active||bool(command),"Draw operation requires BeginScene");
        require(!command||owner==std::this_thread::get_id(),"Render context belongs to another thread during its scene");
    }
    void endEncoder(){if(encoder){[encoder endEncoding];encoder=nil;}}
    void retireQuery(){if(activeQuery&&activeQuery->retired.load(std::memory_order_acquire)){endEncoder();activeQuery->active=false;activeQuery.reset();}}
    // [Blur2PC] Blocking query reads can flush a scene while leaving all bindings
    // and an unrelated active query intact for subsequent draws.
    void submit(bool continueScene) {
        check(true);endEncoder();
        {
            std::lock_guard<std::mutex> guard(device->mutex);
            [command commit];device->last=command;releaseRecording(true);
        }
        command=nil;
        if(continueScene){command=[device->queue commandBuffer];require(bool(command),"Metal query continuation command creation failed");}
        else owner={};
    }
    MTLScissorRect scissor()const{
        require(bool(colours[0]),"Scissor needs a colour attachment");
        if(!scissorState.m_ScissorEnable)return {0,0,colours[0]->width,colours[0]->height};
        const auto& r=scissorState.m_ScissorRect;
        require(r.left>=0&&r.top>=0&&r.right>=r.left&&r.bottom>=r.top&&U32(r.right)<=colours[0]->width&&U32(r.bottom)<=colours[0]->height,"Scissor rectangle exceeds render target");
        return {U32(r.left),U32(r.top),U32(r.right-r.left),U32(r.bottom-r.top)};
    }
    void releaseRecording(bool submitted) {
        for(auto& buffer:usedBuffers){std::lock_guard<std::mutex> guard(buffer->mutex);if(submitted)buffer->pending=command;else buffer->ranges.clear();if(buffer->recording==this)buffer->recording=nullptr;}
        for(auto& surface:usedSurfaces){std::lock_guard<std::mutex> guard(surface->mutex);if(submitted)surface->pending=command;if(surface->recording==this)surface->recording=nullptr;}
        usedBuffers.clear();usedSurfaces.clear();
    }
    ~Impl(){@autoreleasepool{endEncoder();if(activeQuery)activeQuery->active=false;releaseRecording(false);}}
    void useSurface(const std::shared_ptr<Metal::Surface>& surface) {
        if(!surface)return;
        std::lock_guard<std::mutex> guard(surface->mutex);
        require(!surface->locked&&(!surface->recording||surface->recording==this),"Render target is owned by another CPU/GPU operation");
        require(surface->texture.device==device->device,"Render surface belongs to another GPU");
        require(surface->wait(),"Previous surface GPU command failed");
        if(surface->recording!=this){usedSurfaces.push_back(surface);surface->recording=this;surface->context=device->context;}
    }
    bool hasStencil()const{return depth&&depth->format==E_SurfaceFormat_D24S8;}
    void beginEncoder(U32 clear=0,U32 colour=0,F32 z=1,U8 stencil=0,bool visibility=true) {
        check(true);retireQuery();if(encoder)return;
        require(bool(colours[0]),"Render target zero is not bound");
        const U32 width=colours[0]->width,height=colours[0]->height,samples=colours[0]->samples;
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
        // D3D9 permits a shared depth surface larger than the colour target.
        // Metal constrains rendering to this explicit area within every attachment.
        pass.renderTargetWidth=width;pass.renderTargetHeight=height;
        for(U32 i=0;i<4;++i)if(colours[i]) {
            require(colours[i]->format!=E_SurfaceFormat_D16&&colours[i]->format!=E_SurfaceFormat_D24S8&&colours[i]->width==width&&colours[i]->height==height&&colours[i]->samples==samples,"MRT dimensions/formats disagree");
            require(colours[i]->texture.usage&MTLTextureUsageRenderTarget,"Colour attachment was not allocated for rendering");
            for(U32 j=0;j<i;++j)require(colours[j]!=colours[i],"One texture surface cannot occupy multiple colour attachment slots");
            useSurface(colours[i]);auto attachment=pass.colorAttachments[i];attachment.texture=colours[i]->texture;
            attachment.loadAction=clear&E_Clear_Colour?MTLLoadActionClear:MTLLoadActionLoad;attachment.storeAction=MTLStoreActionStore;
            if(colours[i]->resolve){useSurface(colours[i]->resolve);attachment.resolveTexture=colours[i]->resolve->texture;attachment.storeAction=MTLStoreActionStoreAndMultisampleResolve;}
            attachment.clearColor=MTLClearColorMake(((colour>>16)&255)/255.0,((colour>>8)&255)/255.0,(colour&255)/255.0,(colour>>24)/255.0);
        }
        if(depth) {
            if(!((depth->format==E_SurfaceFormat_D16||hasStencil())&&depth->width>=width&&depth->height>=height&&depth->samples==samples)){
                auto describe=[](const std::shared_ptr<Metal::Surface>& target){return std::to_string(target->width)+"x"+std::to_string(target->height)+" format="+std::to_string(U32(target->format))+" samples="+std::to_string(target->samples)+" label='"+std::string(target->texture.label.UTF8String?:"unnamed")+"'";};
                throw std::logic_error("Depth target dimensions/format disagree: colour0="+describe(colours[0])+"; depth="+describe(depth));
            }
            useSurface(depth);pass.depthAttachment.texture=depth->texture;
            pass.depthAttachment.loadAction=clear&E_Clear_Depth?MTLLoadActionClear:MTLLoadActionLoad;
            pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=z;
            if(depth->resolve){useSurface(depth->resolve);pass.depthAttachment.resolveTexture=depth->resolve->texture;pass.depthAttachment.storeAction=MTLStoreActionStoreAndMultisampleResolve;pass.depthAttachment.depthResolveFilter=MTLMultisampleDepthResolveFilterSample0;}
            if(hasStencil()){pass.stencilAttachment.texture=depth->texture;pass.stencilAttachment.loadAction=clear&E_Clear_Stencil?MTLLoadActionClear:MTLLoadActionLoad;pass.stencilAttachment.storeAction=MTLStoreActionStore;pass.stencilAttachment.clearStencil=stencil;
                if(depth->resolve){pass.stencilAttachment.resolveTexture=depth->resolve->texture;pass.stencilAttachment.storeAction=MTLStoreActionStoreAndMultisampleResolve;pass.stencilAttachment.stencilResolveFilter=MTLMultisampleStencilResolveFilterSample0;}
            }
        } else require(!(clear&E_Clear_Depth),"Depth clear has no depth attachment");
        require(!(clear&E_Clear_Stencil)||hasStencil(),"Stencil clear has no stencil attachment");
        // [Blur2PC] A distinct slot per pass prevents later encoders overwriting
        // earlier samples and permits an arbitrary number of queries per scene.
        if(activeQuery&&visibility) {
            auto result=[device->device newBufferWithLength:sizeof(U64) options:MTLResourceStorageModeShared];
            require(bool(result),"Metal visibility buffer allocation failed");
            std::memset(result.contents,0,sizeof(U64));pass.visibilityResultBuffer=result;
            activeQuery->segments.push_back({result,command});
        }
        encoder=[command renderCommandEncoderWithDescriptor:pass];require(bool(encoder),"Metal render encoder creation failed");
        if(activeQuery&&visibility)[encoder setVisibilityResultMode:MTLVisibilityResultModeCounting offset:0];
    }
    void clearRectangle(U32 flags,U32 colour,F32 z,U8 stencil,const C_Viewport& viewport) {
        endEncoder();beginEncoder(0,0,1,0,false); // [Blur2PC] API clears never contribute to query samples.
        std::string key(reinterpret_cast<const char*>(&flags),sizeof(flags));
        auto description=[MTLRenderPipelineDescriptor new];
        description.rasterSampleCount=colours[0]->samples;key.append(reinterpret_cast<const char*>(&colours[0]->samples),sizeof(U32));
        std::string output="struct ClearOutput { ";
        for(U32 i=0;i<4;++i) {
            auto format=colours[i]?colours[i]->texture.pixelFormat:MTLPixelFormatInvalid;
            key.append(reinterpret_cast<const char*>(&format),sizeof(format));
            description.colorAttachments[i].pixelFormat=format;
            description.colorAttachments[i].writeMask=flags&E_Clear_Colour?MTLColorWriteMaskAll:MTLColorWriteMaskNone;
            if(colours[i]&&(flags&E_Clear_Colour))output+="float4 c"+std::to_string(i)+" [[color("+std::to_string(i)+")]]; ";
        }
        auto format=depth?depth->texture.pixelFormat:MTLPixelFormatInvalid;
        key.append(reinterpret_cast<const char*>(&format),sizeof(format));description.depthAttachmentPixelFormat=format;
        if(hasStencil())description.stencilAttachmentPixelFormat=format;
        auto found=clearPipelines.find(key);id<MTLRenderPipelineState> pipeline=nil;
        if(found!=clearPipelines.end())pipeline=found->second;
        else {
            if(flags&E_Clear_Depth)output+="float depth [[depth(any)]]; ";output+="};\n";
            std::string source="#include <metal_stdlib>\nusing namespace metal;\nstruct ClearData { float4 colour; float depth; };\n"+output+
                "struct ClearVertex { float4 position [[position]]; };\n"
                "vertex ClearVertex clear_vs(uint index [[vertex_id]]) { const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return {float4(p[index],0,1)}; }\n"
                "fragment ClearOutput clear_ps(constant ClearData& data [[buffer(0)]]) { ClearOutput output; ";
            for(U32 i=0;i<4;++i)if(colours[i]&&(flags&E_Clear_Colour))source+="output.c"+std::to_string(i)+"=data.colour; ";
            if(flags&E_Clear_Depth)source+="output.depth=data.depth; ";source+="return output; }\n";
            if(!(flags&(E_Clear_Colour|E_Clear_Depth))){const auto start=source.find("fragment ClearOutput clear_ps");source.erase(start);source+="fragment void clear_ps() {}\n";}
            NSError* error=nil;auto library=[device->device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:nil error:&error];
            if(!library)throw std::runtime_error(error.localizedDescription.UTF8String?:"Clear shader compilation failed");
            description.vertexFunction=[library newFunctionWithName:@"clear_vs"];description.fragmentFunction=[library newFunctionWithName:@"clear_ps"];
            pipeline=[device->device newRenderPipelineStateWithDescriptor:description error:&error];
            if(!pipeline)throw std::runtime_error(error.localizedDescription.UTF8String?:"Clear pipeline compilation failed");
            clearPipelines.emplace(std::move(key),pipeline);
        }
        require(!(flags&E_Clear_Depth)||bool(depth),"Depth clear has no attachment");
        require(!(flags&E_Clear_Stencil)||hasStencil(),"Stencil clear has no attachment");
        struct { F32 colour[4];F32 depth;F32 padding[3]; } data={
            {F32((colour>>16)&255)/255,F32((colour>>8)&255)/255,F32(colour&255)/255,F32(colour>>24)/255},z,{}};
        [encoder setRenderPipelineState:pipeline];
        [encoder setViewport:MTLViewport{0,0,double(colours[0]->width),double(colours[0]->height),0,1}];
        [encoder setScissorRect:MTLScissorRect{viewport.x,viewport.y,viewport.width,viewport.height}];
        [encoder setCullMode:MTLCullModeNone];
        auto state=[MTLDepthStencilDescriptor new];state.depthCompareFunction=MTLCompareFunctionAlways;state.depthWriteEnabled=flags&E_Clear_Depth;
        if(flags&E_Clear_Stencil){auto value=[MTLStencilDescriptor new];value.stencilCompareFunction=MTLCompareFunctionAlways;value.stencilFailureOperation=MTLStencilOperationReplace;value.depthFailureOperation=MTLStencilOperationReplace;value.depthStencilPassOperation=MTLStencilOperationReplace;value.readMask=0xff;value.writeMask=0xff;state.frontFaceStencil=value;state.backFaceStencil=value;[encoder setStencilReferenceValue:stencil];}
        [encoder setDepthStencilState:[device->device newDepthStencilStateWithDescriptor:state]];
        [encoder setFragmentBytes:&data length:sizeof(data) atIndex:0];[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        endEncoder(); // [Blur2PC] Resume query counting only for subsequent engine draws.
    }
    const Pipeline& pipeline() {
        require(vertex&&pixel&&declaration,"Draw requires shaders and a vertex declaration");
        require(!vertex->unsupportedImages&&!pixel->unsupportedImages,"Shader uses an unsupported image binding type");
        require(vertex->library.device==device->device&&pixel->library.device==device->device,"Shader belongs to another GPU");
        auto& key=pipelineKeyScratch;key.clear();
        auto append=[&](auto value){key.append(reinterpret_cast<const char*>(&value),sizeof(value));};
        append(vertex->identity);append(pixel->identity);append(colours[0]->samples);
        for(const auto& e:declaration->elements)append(e);
        for(const auto& stream:streams)append(stream.stride);
        append(alphaToCoverageEnabled);
        append(blendEnabled);append(blendState.m_BlendColorSrc);append(blendState.m_BlendColorDst);append(blendState.m_BlendAlphaSrc);append(blendState.m_BlendAlphaDst);append(blendState.m_BlendColorOp);append(blendState.m_BlendAlphaOp);
        std::array<MTLPixelFormat,4> colourFormats;
        for(U32 i=0;i<4;++i){colourFormats[i]=colours[i]?colours[i]->texture.pixelFormat:MTLPixelFormatInvalid;append(colourFormats[i]);append(colourMasks[i]);}
        const auto depthFormat=depth?depth->texture.pixelFormat:MTLPixelFormatInvalid;append(depthFormat);
        auto found=pipelines.find(key);if(found!=pipelines.end())return found->second;
        // Descriptors/layouts are compilation inputs, not per-draw state. Build
        // them only for a real miss, preserving the same validation/mappings.
        auto description=[MTLRenderPipelineDescriptor new];description.vertexFunction=vertex->function;description.fragmentFunction=pixel->function;
        auto layout=[MTLVertexDescriptor vertexDescriptor];description.rasterSampleCount=colours[0]->samples;
        description.alphaToCoverageEnabled=alphaToCoverageEnabled;
        // Original PC coverage preserves shader alpha; alpha-to-one is a separate PS3 policy.
        description.alphaToOneEnabled=NO;
        for(U32 i=0;i<4;++i){
            auto colour=description.colorAttachments[i];colour.pixelFormat=colourFormats[i];colour.writeMask=colourMask(colourMasks[i]);
            colour.blendingEnabled=blendEnabled;colour.sourceRGBBlendFactor=blendFactor(blendState.m_BlendColorSrc);colour.destinationRGBBlendFactor=blendFactor(blendState.m_BlendColorDst);
            colour.sourceAlphaBlendFactor=blendFactor(blendState.m_BlendAlphaSrc);colour.destinationAlphaBlendFactor=blendFactor(blendState.m_BlendAlphaDst);
            colour.rgbBlendOperation=blendOperation(blendState.m_BlendColorOp);colour.alphaBlendOperation=blendOperation(blendState.m_BlendAlphaOp);
        }
        description.depthAttachmentPixelFormat=depthFormat;
        if(hasStencil())description.stencilAttachmentPixelFormat=depthFormat;
        U32 packedCount=0;
        for(std::size_t i=0;i<vertex->semantics.size();++i) {
            const auto& semantic=vertex->semantics[i];
            auto element=std::find_if(declaration->elements.begin(),declaration->elements.end(),[&](const auto& e){return e.Usage==semantic.m_Usage&&e.UsageIndex==semantic.m_UsageIndex;});
            require(element!=declaration->elements.end(),"Vertex declaration lacks an active shader semantic");
            const auto& stream=streams[element->Stream];
            require(stream.buffer&&stream.stride>=element->GetOffset()+element->GetSize(),"Vertex stream is missing or its stride is too small");
            const bool packed=element->Type==E_VertexElementType_U10_3||element->Type==E_VertexElementType_S10_3N;
            const auto format=packed?MTLVertexFormatFloat4:vertexFormat(element->Type);require(format!=MTLVertexFormatInvalid,"Unsupported vertex format");
            auto attribute=layout.attributes[vertex->locations[i]];attribute.format=format;
            if(packed){attribute.offset=packedCount++*16;attribute.bufferIndex=24;}
            else{attribute.offset=element->Offset;attribute.bufferIndex=16+element->Stream;layout.layouts[16+element->Stream].stride=stream.stride;layout.layouts[16+element->Stream].stepFunction=MTLVertexStepFunctionPerVertex;}
        }
        if(packedCount){layout.layouts[24].stride=packedCount*16;layout.layouts[24].stepFunction=MTLVertexStepFunctionPerVertex;}
        description.vertexDescriptor=layout;
        NSError* error=nil;MTLRenderPipelineReflection* reflection=nil;
        id<MTLRenderPipelineState> result=[device->device newRenderPipelineStateWithDescriptor:description options:MTLPipelineOptionBindingInfo|MTLPipelineOptionBufferTypeInfo reflection:&reflection error:&error];
        if(!result)throw std::runtime_error(error.localizedDescription.UTF8String?:"Metal pipeline creation failed");
        require(bool(reflection),"Compiled Metal pipeline binding reflection is unavailable");
        Pipeline compiled;compiled.native=result;
        auto reflect=[](NSArray<id<MTLBinding>>* bindings,const std::shared_ptr<Metal::Shader>& shader,bool vertexStage,std::array<U32,16>& sizes){
            for(id<MTLBinding> binding in bindings)if(binding.type==MTLBindingTypeBuffer&&binding.isUsed){
                // Native vertex attributes occupy slots16..24. Uniform buffers
                // occupy the original reserved slots below them in each stage.
                if(vertexStage&&binding.index>=16)continue;
                require(binding.index<16,"Compiled Metal uniform collides with native vertex streams");
                auto declared=std::find_if(shader->uniforms.begin(),shader->uniforms.end(),[&](const auto& uniform){return uniform.slot==binding.index&&uniform.size;});
                require(declared!=shader->uniforms.end(),"Compiled Metal uniform has no serialized constant binding");
                const auto bytes=((id<MTLBufferBinding>)binding).bufferDataSize;
                require(bytes&&bytes<=1024*1024,"Compiled Metal uniform size is outside native limits");
                // Metal reports sizeof(T), including actual MSL aggregate tail
                // padding. SPIRV-Cross block_size only covers occupied members.
                sizes[binding.index]=U32(bytes);
            }
        };
        reflect(reflection.vertexBindings,vertex,true,compiled.vertexUniformBytes);
        reflect(reflection.fragmentBindings,pixel,false,compiled.pixelUniformBytes);
        return pipelines.emplace(key,std::move(compiled)).first->second;
    }
    void constants(const std::shared_ptr<Metal::Shader>& shader,bool vertexStage,const std::array<U32,16>& requiredBytes) {
        auto& bank=vertexStage?vertexConstants:pixelConstants;
        std::array<F32,32> lodState{};
        auto& textureState=vertexStage?vertexTextures:pixelTextures;
        for(U32 i=0;i<16;++i){
            lodState[i*2]=F32(textureState[i].sampler.m_TextureLodBias)/16.f;
            lodState[i*2+1]=textureState[i].sampler.m_TextureFilterMip==E_TextureFilter_None?0.f:std::numeric_limits<F32>::max();
        }
        for(const auto& uniform:shader->uniforms) {
            if(!uniform.size)continue;
            require(uniform.slot<16,"Shader uniform binding collides with native vertex streams");
            const void* data=nullptr;
            if(uniform.kind==1){require(!vertexStage&&uniform.size==sizeof(alpha),"Invalid private alpha-test layout");data=&alpha;}
            else if(uniform.kind==2){require(uniform.size==sizeof(hdr),"Invalid private HDR layout");data=hdr.data();}
            else if(uniform.kind==3){require(uniform.size==sizeof(lodState),"Invalid private sampler LOD layout");data=lodState.data();}
            else{if(bank.size()*4<uniform.size)bank.resize((uniform.size+3)/4,0);data=bank.data();}
            const U32 bytes=std::max(uniform.size,requiredBytes[uniform.slot]);
            // Metal copies these single-draw bytes while encoding. Its documented
            // transient binding contract is strictly smaller than4KB; preserve
            // the explicit buffer path at that boundary and above. Reflection
            // tail padding is part of the binding and remains initialized.
            if(bytes<4096){
                alignas(16) std::array<U8,4096> transient;
                std::memset(transient.data(),0,bytes);std::memcpy(transient.data(),data,uniform.size);
                if(vertexStage)[encoder setVertexBytes:transient.data() length:bytes atIndex:uniform.slot];else[encoder setFragmentBytes:transient.data() length:bytes atIndex:uniform.slot];
            }else{
                id<MTLBuffer> buffer=[device->device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
                require(bool(buffer)&&buffer.contents,"Shader constant upload allocation failed");
                std::memset(buffer.contents,0,bytes);std::memcpy(buffer.contents,data,uniform.size);
                if(vertexStage)[encoder setVertexBuffer:buffer offset:0 atIndex:uniform.slot];else[encoder setFragmentBuffer:buffer offset:0 atIndex:uniform.slot];
            }
        }
    }
    id<MTLSamplerState> sampler(const C_SamplerState& s,U32 anisotropy){
        auto address=[](U32 v){switch(v){case E_TextureAddress_Wrap:return MTLSamplerAddressModeRepeat;case E_TextureAddress_Mirror:return MTLSamplerAddressModeMirrorRepeat;
            case E_TextureAddress_Clamp:return MTLSamplerAddressModeClampToEdge;case E_TextureAddress_Border:return MTLSamplerAddressModeClampToBorderColor;default:throw std::logic_error("Unsupported sampler address mode");}};
        auto filter=[](U32 v){require(v>=1&&v<=3,"Unsupported sampler min/mag filter");return v==E_TextureFilter_Point?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;};
        std::string key;auto append=[&](U32 v){key.append(reinterpret_cast<const char*>(&v),sizeof(v));};
        append(s.m_TextureAddressU);append(s.m_TextureAddressV);append(s.m_TextureAddressW);append(s.m_TextureFilterMag);append(s.m_TextureFilterMin);append(s.m_TextureFilterMip);append(s.m_TextureBorderColour);
        append(anisotropy);
        if(auto found=samplers.find(key);found!=samplers.end())return found->second;
        auto d=[MTLSamplerDescriptor new];d.sAddressMode=address(s.m_TextureAddressU);d.tAddressMode=address(s.m_TextureAddressV);d.rAddressMode=address(s.m_TextureAddressW);
        d.magFilter=filter(s.m_TextureFilterMag);d.minFilter=filter(s.m_TextureFilterMin);
        require(s.m_TextureFilterMip<=E_TextureFilter_Linear,"Unsupported mip filter");
        d.mipFilter=s.m_TextureFilterMip==E_TextureFilter_None?MTLSamplerMipFilterNotMipmapped:s.m_TextureFilterMip==E_TextureFilter_Point?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
        if(s.m_TextureFilterMag==E_TextureFilter_Anisotropic||s.m_TextureFilterMin==E_TextureFilter_Anisotropic)d.maxAnisotropy=anisotropy;
        if(s.m_TextureAddressU==E_TextureAddress_Border||s.m_TextureAddressV==E_TextureAddress_Border||s.m_TextureAddressW==E_TextureAddress_Border){
            require(s.m_TextureBorderColour==0||s.m_TextureBorderColour==0xff000000||s.m_TextureBorderColour==0xffffffff,"Arbitrary sampler border colour requires shader integration");
            d.borderColor=s.m_TextureBorderColour==0?MTLSamplerBorderColorTransparentBlack:s.m_TextureBorderColour==0xff000000?MTLSamplerBorderColorOpaqueBlack:MTLSamplerBorderColorOpaqueWhite;
        }
        auto result=[device->device newSamplerStateWithDescriptor:d];require(bool(result),"Native sampler creation failed");samplers.emplace(std::move(key),result);return result;
    }
    void textures(const std::shared_ptr<Metal::Shader>& shader,bool vertexStage){
        auto& slots=vertexStage?vertexTextures:pixelTextures;
        for(const auto& binding:shader->textures){
            const auto& state=slots[binding.slot];
            if(!state.texture||state.texture->type!=binding.type)
                throw std::logic_error("Shader texture is missing or has a different dimension: stage="+std::string(vertexStage?"vertex":"pixel")+" slot="+std::to_string(binding.slot)+" expectedType="+std::to_string(U32(binding.type))+" actualType="+(state.texture?std::to_string(U32(state.texture->type)):"unbound")+" shader="+std::to_string(shader->identity));
            auto& t=*state.texture;
            for(const auto& surface:t.surfaces){
                require(surface!=depth&&(!depth||surface!=depth->resolve)&&std::none_of(colours.begin(),colours.end(),[&](const auto& colour){return colour&&(surface==colour||surface==colour->resolve);}),"A render attachment cannot simultaneously be sampled");
                useSurface(surface);
            }
            // D3D9 ignores SRGBTEXTURE when the format has no SRGBREAD support
            // (also preserved by the original PC D3D11Formats.cpp view factory).
            id<MTLTexture> resource=(state.sampler.m_TextureGamma||(state.flags&E_Flags_SRGB))&&t.srgb?t.srgb:t.linear;
            if(vertexStage)[encoder setVertexTexture:resource atIndex:binding.slot];else[encoder setFragmentTexture:resource atIndex:binding.slot];
        }
        for(U32 slot:shader->samplers){
            const auto& state=slots[slot].sampler;
            // [Blur2PC] Opaque explicit-level sampling also needs the PC
            // MaxLOD0 clamp when mip filtering is disabled, even at bias0.
            if(state.m_TextureLodBias || state.m_TextureFilterMip==E_TextureFilter_None)
                require(!(shader->lodUnsupportedSamplers&(1u<<slot)),"Sampler LOD state has unsupported shader sampler provenance/options");
            if(state.m_TextureLodBias){
                require(!(shader->lodGradientSamplers&(1u<<slot)) || (state.m_TextureFilterMin!=E_TextureFilter_Anisotropic && state.m_TextureFilterMag!=E_TextureFilter_Anisotropic),
                        "Anisotropic explicit-gradient sampler LOD bias requires exact footprint integration");
            }
            auto sample=sampler(slots[slot].sampler,slots[slot].anisotropy?slots[slot].anisotropy:maxAnisotropy);
            if(vertexStage)[encoder setVertexSamplerState:sample atIndex:slot];else[encoder setFragmentSamplerState:sample atIndex:slot];
        }
    }
    void draw(E_PrimitiveType type,U32 start,U32 count,bool indexed,S32 base,U32 minIndex,U32 vertexCount) {
        @autoreleasepool {
            check(true);auto primitive=primitiveType(type);if(!count||(type==E_PrimitiveType_TriangleFan&&count<3))return;
            require(colours[0]&&vertex&&pixel&&declaration,"Incomplete draw state");
            // [Blur2PC] PC D3D9 NULL depth surfaces disable depth/stencil operations;
            // D3D11 likewise makes each missing attachment test pass. Preserve the
            // requested state so binding an attachment reactivates it on the next draw.
            const bool effectiveDepth=bool(depth)&&depthState.m_DepthEnable;
            const bool effectiveStencil=hasStencil()&&stencilEnabled;
            require(fillState.m_FillMode!=E_FillMode_Point,"Point fill mode has not yet been ported");
            // [Blur2PC] Preserve D3D11GameDeviceNative's recovered float-to-fixed conversion.
            // Native floating depth uses D3D11's exponent/mantissa rule (also verified by Dawn's Metal tests).
            // UNORM constant units differ on this backend; do not hide that difference with a guessed factor.
            const double fixedBias=double(depthState.m_DepthBias)*16777216.0;
            require(fixedBias>=-2147483648.0&&fixedBias<2147483648.0,"Depth bias exceeds the original signed 24-bit-scaled range");
            const S32 constantBias=S32(fixedBias);
            require(!depth||constantBias==0||depth->texture.pixelFormat==MTLPixelFormatDepth32Float_Stencil8,
                    "UNORM constant bias units differ on Metal; exact PC conversion is not established");
            std::vector<std::shared_ptr<Metal::Buffer>> buffers;
            std::array<bool,8> usedStreams={};
            for(const auto& semantic:vertex->semantics) {
                auto e=std::find_if(declaration->elements.begin(),declaration->elements.end(),[&](const auto& item){return item.Usage==semantic.m_Usage&&item.UsageIndex==semantic.m_UsageIndex;});
                require(e!=declaration->elements.end(),"Vertex semantic is not supplied");usedStreams[e->Stream]=true;
            }
            for(U32 i=0;i<8;++i)if(usedStreams[i]){require(bool(streams[i].buffer),"Vertex stream is unbound");buffers.push_back(streams[i].buffer);}
            if(indexed){require(bool(indices),"Index buffer is unbound");buffers.push_back(indices);}
            std::sort(buffers.begin(),buffers.end(),[](const auto& a,const auto& b){return std::less<Metal::Buffer*>()(a.get(),b.get());});
            buffers.erase(std::unique(buffers.begin(),buffers.end()),buffers.end());
            std::vector<std::unique_lock<std::mutex>> guards;for(auto& buffer:buffers)guards.emplace_back(buffer->mutex);
            for(auto& buffer:buffers){require(!buffer->locked&&(!buffer->recording||buffer->recording==this),"Draw buffer is owned by another operation");
                require(buffer->resource.device==device->device&&buffer->wait(),"Draw buffer GPU ownership/completion failed");}
            U64 minimum=start,maximum=U64(start)+count-1;
            const U32 indexWidth=indexFormat==E_IndexFormat_16?2:4;
            std::vector<U32> fan;
            if(indexed) {
                require(indexFormat==E_IndexFormat_16||indexFormat==E_IndexFormat_32,"Unknown index format");
                require(U64(start)+count<=indices->resource.length/indexWidth,"Index read exceeds its buffer");
                minimum=~U64(0);maximum=0;
                const auto* data=static_cast<const U8*>(indices->resource.contents)+U64(start)*indexWidth;
                for(U32 i=0;i<count;++i){U32 value=0;std::memcpy(&value,data+U64(i)*indexWidth,indexWidth);minimum=std::min<U64>(minimum,value);maximum=std::max<U64>(maximum,value);
                    if(type==E_PrimitiveType_TriangleFan)fan.push_back(value);}
                require(vertexCount&&minimum>=minIndex&&maximum<U64(minIndex)+vertexCount,"Indices disagree with the declared vertex range");
                require(S64(minimum)+base>=0,"Base vertex addresses before the stream");minimum=U64(S64(minimum)+base);maximum=U64(S64(maximum)+base);
            }
            std::array<std::pair<U64,U64>,8> ranges={};
            for(U32 i=0;i<8;++i)if(usedStreams[i]) {
                const auto& stream=streams[i];U32 end=0;
                for(const auto& e:declaration->elements)if(e.Stream==i)end=std::max(end,e.GetOffset()+e.GetSize());
                require(stream.stride>=end&&stream.stride,"Vertex stride cannot contain the declaration");
                ranges[i]={U64(stream.offset)+minimum*stream.stride,U64(stream.offset)+maximum*stream.stride+end};
                require(ranges[i].second<=stream.buffer->resource.length,"Vertex fetch exceeds its buffer");
            }
            const auto& renderPipeline=pipeline();beginEncoder();
            const auto vp=viewportExplicit?viewport:C_Viewport(0,0,colours[0]->width,colours[0]->height);
            require(vp.width&&vp.height&&U64(vp.x)+vp.width<=colours[0]->width&&U64(vp.y)+vp.height<=colours[0]->height,"Viewport exceeds render target");
            const auto clip=scissor();if(!clip.width||!clip.height)return;
            [encoder setRenderPipelineState:renderPipeline.native];[encoder setViewport:MTLViewport{double(vp.x),double(vp.y),double(vp.width),double(vp.height),vp.minZ,vp.maxZ}];
            [encoder setScissorRect:clip];[encoder setTriangleFillMode:fillState.m_FillMode==E_FillMode_Wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
            [encoder setFrontFacingWinding:MTLWindingClockwise];[encoder setCullMode:cullState.m_CullMode==E_CullMode_None?MTLCullModeNone:cullState.m_CullMode==E_CullMode_Cw?MTLCullModeFront:MTLCullModeBack];
            auto nativeDepth=[MTLDepthStencilDescriptor new];nativeDepth.depthCompareFunction=effectiveDepth?MTLCompareFunction(depthState.m_DepthFunc-1):MTLCompareFunctionAlways;
            nativeDepth.depthWriteEnabled=effectiveDepth&&depthState.m_DepthWriteEnable;
            if(effectiveStencil){auto stencil=[MTLStencilDescriptor new];stencil.stencilCompareFunction=MTLCompareFunction(stencilState.m_StencilFunc-1);stencil.stencilFailureOperation=MTLStencilOperation(stencilState.m_StencilFail-1);stencil.depthFailureOperation=MTLStencilOperation(stencilState.m_StencilDepthFail-1);stencil.depthStencilPassOperation=MTLStencilOperation(stencilState.m_StencilDepthPass-1);stencil.readMask=stencilState.m_StencilMask;stencil.writeMask=stencilState.m_StencilWriteMask;nativeDepth.frontFaceStencil=stencil;nativeDepth.backFaceStencil=stencil;[encoder setStencilReferenceValue:stencilState.m_StencilRef];}
            [encoder setDepthStencilState:[device->device newDepthStencilStateWithDescriptor:nativeDepth]];
            [encoder setDepthBias:depth?F32(constantBias):0 slopeScale:depth?depthState.m_SlopeScaledBias:0 clamp:0];
            alpha.function=alphaState.m_AlphaTestCmpFunc;alpha.reference=F32(alphaState.m_AlphaTestRef)/255.0f;
            alpha.format=colours[0]->format==E_SurfaceFormat_A16B16G16R16F?1:
                         colours[0]->format==E_SurfaceFormat_A8R8G8B8||colours[0]->format==E_SurfaceFormat_A8B8G8R8?8:0;
            constants(vertex,true,renderPipeline.vertexUniformBytes);constants(pixel,false,renderPipeline.pixelUniformBytes);textures(vertex,true);textures(pixel,false);
            // D3D9's 10:10:10 types have no exact equivalent Metal vertex
            // fetch format. Expand active attributes using the documented PC
            // numeric conversion; keep all other attributes in their buffers.
            std::vector<C_VertexElement> packed;
            for(const auto& semantic:vertex->semantics)for(const auto& element:declaration->elements)
                if(element.Usage==semantic.m_Usage&&element.UsageIndex==semantic.m_UsageIndex&&
                   (element.Type==E_VertexElementType_U10_3||element.Type==E_VertexElementType_S10_3N))packed.push_back(element);
            if(!packed.empty()) {
                require((maximum+1)*packed.size()*16<=device->device.maxBufferLength,"Packed vertex expansion exceeds GPU allocation limits");
                std::vector<F32> expanded((maximum+1)*packed.size()*4,0);
                for(U64 v=minimum;v<=maximum;++v)for(std::size_t i=0;i<packed.size();++i) {
                    const auto& element=packed[i];const auto& stream=streams[element.Stream];U32 value;
                    std::memcpy(&value,static_cast<const U8*>(stream.buffer->resource.contents)+stream.offset+v*stream.stride+element.Offset,4);
                    auto* target=expanded.data()+(v*packed.size()+i)*4;
                    for(U32 c=0;c<3;++c){U32 component=(value>>(c*10))&1023;
                        target[c]=element.Type==E_VertexElementType_U10_3?F32(component):F32(component&512?S32(component)-1024:S32(component))/511.0f;}
                    target[3]=1;
                }
                auto buffer=[device->device newBufferWithBytes:expanded.data() length:expanded.size()*4 options:MTLResourceStorageModeShared];
                require(bool(buffer),"Packed vertex expansion upload failed");[encoder setVertexBuffer:buffer offset:0 atIndex:24];
            }
            for(U32 i=0;i<8;++i)if(usedStreams[i]) {
                auto& stream=streams[i];[encoder setVertexBuffer:stream.buffer->resource offset:stream.offset atIndex:16+i];
                stream.buffer->ranges.push_back(ranges[i]);
            }
            if(indexed)indices->ranges.push_back({U64(start)*indexWidth,(U64(start)+count)*indexWidth});
            for(auto& buffer:buffers)if(buffer->recording!=this){usedBuffers.push_back(buffer);buffer->recording=this;}
            if(type==E_PrimitiveType_TriangleFan) {
                if(!indexed){fan.resize(count);for(U32 i=0;i<count;++i)fan[i]=start+i;base=0;}
                std::vector<U32> triangles;triangles.reserve(U64(count-2)*3);
                for(U32 i=1;i+1<count;++i){triangles.push_back(fan[0]);triangles.push_back(fan[i]);triangles.push_back(fan[i+1]);}
                auto data=[device->device newBufferWithBytes:triangles.data() length:triangles.size()*4 options:MTLResourceStorageModeShared];
                require(bool(data),"Triangle fan expansion allocation failed");
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:triangles.size() indexType:MTLIndexTypeUInt32 indexBuffer:data indexBufferOffset:0 instanceCount:1 baseVertex:base baseInstance:0];
            } else if(indexed) [encoder drawIndexedPrimitives:primitive indexCount:count indexType:indexWidth==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32 indexBuffer:indices->resource indexBufferOffset:U64(start)*indexWidth instanceCount:1 baseVertex:base baseInstance:0];
            else [encoder drawPrimitives:primitive vertexStart:start vertexCount:count];
        }
    }
};
C_Context::C_Context(C_MetalDevice& device):m_Impl(std::make_unique<Impl>(device.m_State.get())){}
C_Context::~C_Context()=default;
void C_Context::BeginScene(){@autoreleasepool{auto& s=*m_Impl;s.check();require(!s.command,"Scene already active");s.command=[s.device->queue commandBuffer];require(bool(s.command),"Metal command buffer creation failed");s.owner=std::this_thread::get_id();}}
void C_Context::EndScene(){@autoreleasepool{auto& s=*m_Impl;s.check(true);s.retireQuery();require(!s.activeQuery,"An occlusion query must be ended before EndScene");s.submit(false);}}
// [Blur2PC][macOS] Queries use the device's lifetime-checked immediate context.
bool C_Context::CreateOcclusionQuery(C_OcclusionQuery& query){
    auto& s=*m_Impl;s.check();if(query.m_State)return query.m_State->context.lock().get()==this;
    if(![s.device->device supportsFamily:MTLGPUFamilyApple3]&&![s.device->device supportsFamily:MTLGPUFamilyMac1])return false;
    auto state=std::make_shared<C_OcclusionQuery::State>();state->context=s.device->context;query.m_State=std::move(state);return true;
}
void C_Context::BeginOcclusionQuery(C_OcclusionQuery& query){@autoreleasepool{
    auto& s=*m_Impl;s.check(true);s.retireQuery();require(query.m_State&&query.m_State->context.lock().get()==this,"Occlusion query belongs to another context");
    require(!s.activeQuery&&!query.m_State->active,"Nested occlusion queries are not supported");
    s.endEncoder();query.m_State->segments.clear();query.m_State->issued=false;query.m_State->active=true;s.activeQuery=query.m_State;
}}
void C_Context::EndOcclusionQuery(C_OcclusionQuery& query){@autoreleasepool{
    auto& s=*m_Impl;s.check(true);require(query.m_State&&s.activeQuery==query.m_State,"Occlusion query End does not match Begin");
    s.endEncoder();query.m_State->active=false;query.m_State->issued=true;s.activeQuery.reset();
}}
bool C_Context::SubmitOcclusionQuery(const C_OcclusionQuery& query){@autoreleasepool{
    auto& s=*m_Impl;s.check();if(!query.m_State||query.m_State->context.lock().get()!=this)return false;
    for(const auto& segment:query.m_State->segments)if(segment.command.status==MTLCommandBufferStatusNotEnqueued){
        if(segment.command!=s.command)return false;s.submit(true);break;
    }
    return true;
}}
bool C_OcclusionQuery::Create(C_Context& context){return context.CreateOcclusionQuery(*this);}
bool C_OcclusionQuery::IsCreated()const{return m_State&&!m_State->context.expired();}
void C_OcclusionQuery::Destroy(){if(m_State)m_State->retired.store(true,std::memory_order_release);m_State.reset();} // In-flight segments remain retained by Metal/the active context.
void C_OcclusionQuery::Begin(){if(!m_State)return;auto context=m_State->context.lock();require(bool(context),"Occlusion query context was destroyed");context->BeginOcclusionQuery(*this);}
void C_OcclusionQuery::End(){if(!m_State)return;auto context=m_State->context.lock();require(bool(context),"Occlusion query context was destroyed");context->EndOcclusionQuery(*this);}
bool C_OcclusionQuery::GetResult(U64& samples)const{@autoreleasepool{
    samples=0;if(!m_State||!m_State->issued||m_State->active)return false;
    for(const auto& segment:m_State->segments)if(segment.command.status==MTLCommandBufferStatusNotEnqueued){
        auto context=m_State->context.lock();if(!context||!context->SubmitOcclusionQuery(*this))return false;break;
    }
    for(const auto& segment:m_State->segments){
        [segment.command waitUntilCompleted];if(segment.command.status!=MTLCommandBufferStatusCompleted){samples=0;return false;}
        U64 value=0;std::memcpy(&value,segment.buffer.contents,sizeof(value));samples=value>~U64(0)-samples?~U64(0):samples+value;
    }
    return true;
}}
// [Blur2PC] A render-texture Resolve closes its actual encoder, including any active query segment.
void C_Context::ResolveSurface(const C_Surface* surface){
    auto& state=*m_Impl;state.check();require(surface&&surface->m_Surface,"Resolve requires a created surface");
    const auto& native=surface->m_Surface;
    require(native->texture.device==state.device->device,"Resolve surface belongs to another GPU");
    std::lock_guard<std::mutex> guard(native->mutex);
    require(!native->recording||native->recording==&state,"Resolve surface belongs to another recording context");
    if(native->recording)state.endEncoder();
}
// [Blur2PC][macOS] Copy actual same-format surface storage within the current command stream; no CPU staging or extra scene.
void C_Context::CopySurface(const C_Surface* destination,const C_Surface* source){@autoreleasepool{
    auto& state=*m_Impl;state.check(true);require(destination&&source&&destination->m_Surface&&source->m_Surface,"Surface copy requires created resources");
    auto to=destination->m_Surface,from=source->m_Surface;
    require(to->width==from->width&&to->height==from->height&&to->depth==from->depth,"Surface copy dimensions differ");
    CopySurfaceRegion(destination,source,0,0,from->width,from->height);
}}
// [Blur2PC] Exact native blit for photo strips and bounded image copies. Validate
// every dimension before disturbing the current encoder or resource ownership.
void C_Context::CopySurfaceRegion(const C_Surface* destination,const C_Surface* source,U32 sourceX,U32 sourceY,U32 width,U32 height,U32 destinationX,U32 destinationY){@autoreleasepool{
    auto& state=*m_Impl;state.check(true);require(destination&&source&&destination->m_Surface&&source->m_Surface,"Surface region copy requires created resources");
    auto to=destination->m_Surface,from=source->m_Surface;
    require(to->samples==1,"Surface region copy requires a single-sample destination");
    if(from->samples>1){require(bool(from->resolve),"Multisample source has no readable resolve");from=from->resolve;}
    require(to!=from&&to->texture.device==state.device->device&&from->texture.device==state.device->device,"Invalid surface copy identity/device");
    require(to->depth==1&&from->depth==1&&to->texture.pixelFormat==from->texture.pixelFormat,"Surface copy depth/formats differ");
    require(width&&height&&U64(sourceX)+width<=from->width&&U64(sourceY)+height<=from->height&&
            U64(destinationX)+width<=to->width&&U64(destinationY)+height<=to->height,"Surface region copy exceeds texture bounds");
    const U32 block=from->block;
    require(block==to->block&&sourceX%block==0&&sourceY%block==0&&destinationX%block==0&&destinationY%block==0&&
            (width%block==0||(U64(sourceX)+width==from->width&&U64(destinationX)+width==to->width))&&
            (height%block==0||(U64(sourceY)+height==from->height&&U64(destinationY)+height==to->height)),"Compressed surface regions must align to blocks or matching mip edges");
    state.endEncoder();state.useSurface(from);state.useSurface(to);
    auto blit=[state.command blitCommandEncoder];require(bool(blit),"Native surface copy encoder failed");
    [blit copyFromTexture:from->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(sourceX,sourceY,0) sourceSize:MTLSizeMake(width,height,1)
                toTexture:to->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(destinationX,destinationY,0)];[blit endEncoding];
}}
void C_Context::SetViewport(const C_Viewport& viewport){m_Impl->check();require(std::isfinite(viewport.minZ)&&std::isfinite(viewport.maxZ)&&viewport.minZ>=0&&viewport.maxZ<=1&&viewport.minZ<=viewport.maxZ,"Invalid depth viewport");m_Impl->viewport=viewport;m_Impl->viewportExplicit=true;}
void C_Context::SetColourSurface(U32 index,const C_Surface* surface){m_Impl->check();require(index<4,"Render target index exceeds engine limit");require(!surface||(surface->m_Surface&&surface->m_Surface->texture.device==m_Impl->device->device),"Colour attachment must be created on this GPU");m_Impl->endEncoder();m_Impl->colours[index]=surface?surface->m_Surface:nullptr;
    if(!index){m_Impl->viewportExplicit=false;m_Impl->scissorState.m_ScissorRect={0,0,surface?S32(surface->GetWidth()):0,surface?S32(surface->GetHeight()):0};}}
void C_Context::SetDepthStencilSurface(const C_Surface* surface){m_Impl->check();require(!surface||(surface->m_Surface&&surface->m_Surface->texture.device==m_Impl->device->device),"Depth attachment must be created on this GPU");m_Impl->endEncoder();m_Impl->depth=surface?surface->m_Surface:nullptr;}
void C_Context::Clear(U32 flags,U32 colour,F32 depth,U8 stencil){@autoreleasepool{auto& s=*m_Impl;s.check(true);require(!(flags&~7U),"Invalid native clear flags");require(!(flags&E_Clear_Stencil)||s.hasStencil(),"Stencil clear has no stencil attachment");require(std::isfinite(depth)&&depth>=0&&depth<=1,"Invalid clear depth");
    if(!flags)return;require(bool(s.colours[0]),"Clear requires a render target");
    auto vp=s.viewportExplicit?s.viewport:C_Viewport(0,0,s.colours[0]->width,s.colours[0]->height);
    require(vp.width&&vp.height&&U64(vp.x)+vp.width<=s.colours[0]->width&&U64(vp.y)+vp.height<=s.colours[0]->height,"Clear viewport exceeds render target");
    const auto clip=s.scissor();const U32 left=std::max<U32>(vp.x,clip.x),top=std::max<U32>(vp.y,clip.y);
    const U32 right=std::min<U32>(vp.x+vp.width,clip.x+clip.width),bottom=std::min<U32>(vp.y+vp.height,clip.y+clip.height);
    if(right<=left||bottom<=top)return;vp=C_Viewport(left,top,right-left,bottom-top);
    // Load-action clears can touch an entire larger depth attachment. Preserve
    // its pixels outside the current colour viewport with the real scissored clear.
    const bool sharedDepthClear=(flags&(E_Clear_Depth|E_Clear_Stencil))&&s.depth&&(s.depth->width!=s.colours[0]->width||s.depth->height!=s.colours[0]->height);
    if(vp.x||vp.y||vp.width!=s.colours[0]->width||vp.height!=s.colours[0]->height||sharedDepthClear)s.clearRectangle(flags,colour,depth,stencil,vp);
    else{s.endEncoder();s.beginEncoder(flags,colour,depth,stencil);}}}
void C_Context::SetVertexDeclaration(const C_VertexDeclaration* declaration){m_Impl->check();m_Impl->declaration=declaration?declaration->m_Declaration:nullptr;}
void C_Context::SetVertexBuffer(U32 stream,const C_VertexBuffer* buffer,U32 offset,U32 stride){m_Impl->check();require(stream<8,"Vertex stream exceeds engine limit");m_Impl->streams[stream]={buffer?buffer->m_Buffer:nullptr,offset,stride};}
void C_Context::SetVertexBuffer(U32 stream,const C_VertexBuffer* buffer,U32 offset,U32 stride,const C_VertexDeclaration* declaration){SetVertexDeclaration(declaration);SetVertexBuffer(stream,buffer,offset,stride);}
void C_Context::SetIndexBuffer(const C_IndexBuffer* buffer){m_Impl->check();m_Impl->indices=buffer?buffer->m_Buffer:nullptr;m_Impl->indexFormat=buffer?buffer->m_Format:E_IndexFormat_Unknown;}
void C_Context::SetVertexShader(const C_VertexShader* shader){m_Impl->check();m_Impl->vertex=shader?shader->m_Shader:nullptr;}
void C_Context::SetPixelShader(const C_PixelShader* shader){m_Impl->check();m_Impl->pixel=shader?shader->m_Shader:nullptr;}
void C_Context::SetTexture(U32 index,const C_Texture* texture){m_Impl->check();require(index<16,"Pixel texture slot exceeds PC limits");auto& s=m_Impl->pixelTextures[index];s.texture=texture?texture->m_Texture:nullptr;s.flags=texture?texture->m_Flags:0;}
// [Blur2PC] Match the existing PC DX11 game binder's SetNativeTexture contract:
// copy actual prior colour contents at binding time, then sample that owned
// snapshot while the original target remains writable. Raw Display2 bindings
// retain their strict feedback guard; depth attachment feedback is not guessed.
void C_Context::SetTextureWithColourSnapshot(U32 index,const C_Texture* texture){@autoreleasepool{
    auto& state=*m_Impl;state.check();require(index<16,"Pixel texture slot exceeds PC limits");
    if(!texture||!texture->m_Texture){SetTexture(index,texture);return;}
    const auto& source=texture->m_Texture;
    bool colourAlias=false,depthAlias=false;
    for(const auto& surface:source->surfaces){
        depthAlias|=surface==state.depth||(state.depth&&surface==state.depth->resolve);
        for(const auto& colour:state.colours)colourAlias|=colour&&(surface==colour||surface==colour->resolve);
    }
    if(!colourAlias){SetTexture(index,texture);return;}
    require(!depthAlias,"A depth attachment cannot be snapshotted as colour feedback");
    require(source->type==E_TextureType_2D||source->type==E_TextureType_Cube,"Colour feedback requires a PC 2D/cube texture");
    require(source->linear.device==state.device->device&&source->linear.sampleCount==1,"Colour snapshot source GPU/sample identity differs");
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:source->linear.pixelFormat width:source->width height:source->height mipmapped:source->levels>1];
    descriptor.textureType=source->linear.textureType;descriptor.mipmapLevelCount=source->levels;
    descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    auto storage=[state.device->device newTextureWithDescriptor:descriptor];require(bool(storage),"Native colour snapshot allocation failed");
    const U32 faces=source->type==E_TextureType_Cube?6:1;
    auto snapshot=std::make_shared<Metal::Texture>();
    snapshot->linear=[storage newTextureViewWithPixelFormat:source->linear.pixelFormat textureType:source->linear.textureType levels:NSMakeRange(0,source->levels) slices:NSMakeRange(0,faces) swizzle:source->linear.swizzle];
    require(bool(snapshot->linear),"Native colour snapshot linear view failed");
    if(source->srgb){snapshot->srgb=[storage newTextureViewWithPixelFormat:source->srgb.pixelFormat textureType:source->srgb.textureType levels:NSMakeRange(0,source->levels) slices:NSMakeRange(0,faces) swizzle:source->srgb.swizzle];require(bool(snapshot->srgb),"Native colour snapshot sRGB view failed");}
    snapshot->width=source->width;snapshot->height=source->height;snapshot->depth=source->depth;snapshot->levels=source->levels;
    snapshot->format=source->format;snapshot->type=source->type;snapshot->size=source->size;
    for(U32 face=0;face<faces;++face)for(U32 level=0;level<source->levels;++level){
        const auto& from=source->surfaces[face*source->levels+level];auto to=std::make_shared<Metal::Surface>();
        to->texture=[storage newTextureViewWithPixelFormat:from->texture.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(level,1) slices:NSMakeRange(face,1)];
        require(bool(to->texture),"Native colour snapshot mip/face view failed");
        to->width=from->width;to->height=from->height;to->depth=from->depth;to->pitch=from->pitch;to->bytes=from->bytes;to->block=from->block;to->format=from->format;to->queue=state.device->queue;
        snapshot->surfaces.push_back(std::move(to));
    }
    const bool ownCommand=!state.command;
    if(ownCommand)BeginScene();
    try{
        // Ending the current encoder makes every active MSAA resolve available
        // before the blit. Visibility queries resume on their next real draw.
        state.endEncoder();
        for(U32 i=0;i<source->surfaces.size();++i){C_Surface from,to;from.m_Surface=source->surfaces[i];to.m_Surface=snapshot->surfaces[i];CopySurface(&to,&from);}
        if(ownCommand)EndScene();
    }catch(...){
        if(ownCommand){state.endEncoder();state.releaseRecording(false);state.command=nil;state.owner={};}
        throw;
    }
    auto& slot=state.pixelTextures[index];slot.texture=std::move(snapshot);slot.flags=texture->m_Flags;
}}
void C_Context::SetVertexTexture(U32 index,const C_Texture* texture){m_Impl->check();require(index<4,"Vertex texture slot exceeds PC limits");auto& s=m_Impl->vertexTextures[index];s.texture=texture?texture->m_Texture:nullptr;s.flags=texture?texture->m_Flags:0;}
void C_Context::SetSamplerState(U32 index,const C_SamplerState& state){m_Impl->check();require(index<16,"Pixel sampler slot exceeds PC limits");m_Impl->pixelTextures[index].sampler=state;m_Impl->pixelTextures[index].anisotropy=0;}
void C_Context::SetVertexSamplerState(U32 index,const C_SamplerState& state){m_Impl->check();require(index<4,"Vertex sampler slot exceeds PC limits");m_Impl->vertexTextures[index].sampler=state;m_Impl->vertexTextures[index].anisotropy=0;}
// [Blur2PC][macOS] Upper context carries a separate maximum per sampler; lower callers may use the global default.
void C_Context::SetSamplerState(U32 index,const C_SamplerState& state,U32 anisotropy){require(anisotropy>=1&&anisotropy<=16,"Pixel anisotropy is outside native limits");SetSamplerState(index,state);m_Impl->pixelTextures[index].anisotropy=anisotropy;}
void C_Context::SetVertexSamplerState(U32 index,const C_SamplerState& state,U32 anisotropy){require(anisotropy>=1&&anisotropy<=16,"Vertex anisotropy is outside native limits");SetVertexSamplerState(index,state);m_Impl->vertexTextures[index].anisotropy=anisotropy;}
const C_SamplerState& C_Context::GetSamplerState(U32 index)const{m_Impl->check();require(index<16,"Pixel sampler slot exceeds PC limits");return m_Impl->pixelTextures[index].sampler;}
const C_VertexSamplerState& C_Context::GetVertexSamplerState(U32 index)const{m_Impl->check();require(index<4,"Vertex sampler slot exceeds PC limits");return m_Impl->vertexTextures[index].sampler;}
void C_Context::SetMaxAnisotropy(U32 value){m_Impl->check();require(value>=1&&value<=16,"Anisotropy is outside Metal limits");m_Impl->maxAnisotropy=value;}
static void uploadConstants(std::vector<F32>& bank,U32 start,const F32* data,U32 count){require(start<=65536&&count<=65536-start&&(!count||data),"Shader register upload exceeds limits");if(!count)return;if(bank.size()<(U64(start)+count)*4)bank.resize((U64(start)+count)*4,0);std::memcpy(bank.data()+U64(start)*4,data,U64(count)*16);}
void C_Context::SetVertexShaderConstantF(U32 start,const F32* data,U32 count){m_Impl->check();uploadConstants(m_Impl->vertexConstants,start,data,count);}
void C_Context::SetPixelShaderConstantF(U32 start,const F32* data,U32 count){m_Impl->check();uploadConstants(m_Impl->pixelConstants,start,data,count);}
void C_Context::SetVertexShaderParamF(const C_VertexShaderParam& p,const F32* data){if(p.m_RegisterCount)SetVertexShaderConstantF(p.m_RegisterIndex,data,p.m_RegisterCount);}
// [Blur2PC] As Display2D3D11Bridge.cpp: the caller count is not the reflected
// parameter span. Matrix/array parameters upload every reflected register.
void C_Context::SetPixelShaderParamF(const C_PixelShaderParam& p,const F32* data,U32){if(p.m_RegisterCount)SetPixelShaderConstantF(p.m_RegisterIndex,data,p.m_RegisterCount);}
void C_Context::SetPixelShaderParamArrayF(const C_PixelShaderParam& p,const F32* data,U32 count){SetPixelShaderParamF(p,data,count);}
#define BOOL_STATE(Method,Field) void C_Context::Method(E_Bool value){m_Impl->check();require(value==E_Bool_False||value==E_Bool_True,"Invalid boolean state");m_Impl->Field=value==E_Bool_True;}
BOOL_STATE(SetAlphaToMaskEnable,alphaToCoverageEnabled) BOOL_STATE(SetAlphaTestEnable,alpha.enabled) BOOL_STATE(SetDepthEnable,depthState.m_DepthEnable) BOOL_STATE(SetDepthWriteEnable,depthState.m_DepthWriteEnable) BOOL_STATE(SetBlendEnable,blendEnabled) BOOL_STATE(SetStencilEnable,stencilEnabled)
#undef BOOL_STATE
void C_Context::SetAlphaTestFunc(E_CmpFunc value){m_Impl->check();require(value>=1&&value<=8,"Invalid alpha comparison");m_Impl->alphaState.m_AlphaTestCmpFunc=value;}
void C_Context::SetAlphaTestRef(U32 value){m_Impl->check();require(value<=255,"Alpha reference is not a byte");m_Impl->alphaState.m_AlphaTestRef=value;}
void C_Context::SetDepthFunc(E_CmpFunc value){m_Impl->check();require(value>=1&&value<=8,"Invalid depth comparison");m_Impl->depthState.m_DepthFunc=value;}
void C_Context::SetCullMode(E_CullMode value){m_Impl->check();require(value>=1&&value<=3,"Invalid cull mode");m_Impl->cullState.m_CullMode=value;}
void C_Context::SetColourWriteEnable(U32 value){m_Impl->check();require(!(value&~15U),"Invalid colour write mask");m_Impl->colourState.m_ColourWriteEnable=value;m_Impl->colourMasks.fill(value);}
void C_Context::SetColourWriteEnable(U32 target,U32 value){m_Impl->check();require(target<4&&!(value&~15U),"Invalid MRT colour target/write mask");m_Impl->colourMasks[target]=value;if(!target)m_Impl->colourState.m_ColourWriteEnable=value;}
U32 C_Context::GetColourWriteEnable(U32 target)const{m_Impl->check();require(target<4,"Invalid MRT colour target");return m_Impl->colourMasks[target];}
#define FACTOR_STATE(Method,Field) void C_Context::Method(E_Blend value){m_Impl->check();blendFactor(value);m_Impl->Field=value;}
FACTOR_STATE(SetBlendSrc,blendState.m_BlendColorSrc) FACTOR_STATE(SetBlendDst,blendState.m_BlendColorDst) FACTOR_STATE(SetBlendSrcAlpha,blendState.m_BlendAlphaSrc) FACTOR_STATE(SetBlendDstAlpha,blendState.m_BlendAlphaDst)
#undef FACTOR_STATE
void C_Context::SetBlendOp(E_BlendOp value){m_Impl->check();blendOperation(value);m_Impl->blendState.m_BlendColorOp=value;}
void C_Context::SetBlendOpAlpha(E_BlendOp value){m_Impl->check();blendOperation(value);m_Impl->blendState.m_BlendAlphaOp=value;}
#define STATE_GETTER(Type,Name,Field) const Type& C_Context::Get##Name##State()const{m_Impl->check();return m_Impl->Field;}
STATE_GETTER(C_AlphaTestState,AlphaTest,alphaState) STATE_GETTER(C_BlendState,Blend,blendState) STATE_GETTER(C_ColourState,Colour,colourState)
STATE_GETTER(C_CullState,Cull,cullState) STATE_GETTER(C_DepthState,Depth,depthState) STATE_GETTER(C_FillState,Fill,fillState)
STATE_GETTER(C_ScissorState,Scissor,scissorState) STATE_GETTER(C_StencilState,Stencil,stencilState)
#undef STATE_GETTER
void C_Context::SetAlphaTestState(const C_AlphaTestState& s){m_Impl->check();require(s.m_AlphaTestCmpFunc>=1&&s.m_AlphaTestCmpFunc<=8,"Invalid alpha comparison");m_Impl->alphaState=s;}
void C_Context::SetBlendState(const C_BlendState& s){m_Impl->check();blendFactor(s.m_BlendColorSrc);blendFactor(s.m_BlendColorDst);blendFactor(s.m_BlendAlphaSrc);blendFactor(s.m_BlendAlphaDst);blendOperation(s.m_BlendColorOp);blendOperation(s.m_BlendAlphaOp);m_Impl->blendState=s;}
void C_Context::SetColourState(const C_ColourState& s){SetColourWriteEnable(s.m_ColourWriteEnable);}
void C_Context::SetCullState(const C_CullState& s){SetCullMode(E_CullMode(s.m_CullMode));}
void C_Context::SetDepthState(const C_DepthState& s){m_Impl->check();require(s.m_DepthFunc>=1&&s.m_DepthFunc<=8&&std::isfinite(s.m_DepthBias)&&std::isfinite(s.m_SlopeScaledBias),"Invalid depth state");m_Impl->depthState=s;}
void C_Context::SetFillState(const C_FillState& s){SetFillMode(E_FillMode(s.m_FillMode));}
void C_Context::SetScissorState(const C_ScissorState& s){m_Impl->check();const auto& r=s.m_ScissorRect;require(s.m_ScissorEnable<=1&&r.left>=0&&r.top>=0&&r.right>=r.left&&r.bottom>=r.top,"Invalid scissor state");m_Impl->scissorState=s;}
void C_Context::SetStencilState(const C_StencilState& s){m_Impl->check();require(s.m_StencilFunc>=1&&s.m_StencilFunc<=8&&s.m_StencilFail>=1&&s.m_StencilFail<=8&&s.m_StencilDepthFail>=1&&s.m_StencilDepthFail<=8&&s.m_StencilDepthPass>=1&&s.m_StencilDepthPass<=8,"Invalid stencil state");m_Impl->stencilState=s;}
void C_Context::SetScissorEnable(E_Bool v){m_Impl->check();require(v==E_Bool_False||v==E_Bool_True,"Invalid scissor enable");m_Impl->scissorState.m_ScissorEnable=v;}
void C_Context::SetScissorRect(const C_ScissorRect& r){auto s=GetScissorState();s.m_ScissorRect=r;SetScissorState(s);}
void C_Context::SetFillMode(E_FillMode v){m_Impl->check();require(v>=1&&v<=3,"Invalid fill mode");m_Impl->fillState.m_FillMode=v;}
void C_Context::SetDepthBias(F32 v){m_Impl->check();require(std::isfinite(v),"Invalid depth bias");m_Impl->depthState.m_DepthBias=v;}
void C_Context::SetSlopeScaledBias(F32 v){m_Impl->check();require(std::isfinite(v),"Invalid slope-scaled bias");m_Impl->depthState.m_SlopeScaledBias=v;}
void C_Context::DrawPrimitive(E_PrimitiveType type,U32 start,U32 count){m_Impl->draw(type,start,count,false,0,0,0);}
void C_Context::DrawIndexedPrimitive(E_PrimitiveType type,U32 base,U32 minimum,U32 vertices,U32 start,U32 count){m_Impl->draw(type,start,count,true,S32(base),minimum,vertices);}
void C_Context::DrawIndexedPrimitive(E_PrimitiveType type,U32 base,U32 minimum,U32 vertices,U32 start,U32 count,const C_IndexBuffer* indices){if(indices)SetIndexBuffer(indices);DrawIndexedPrimitive(type,base,minimum,vertices,start,count);}
}
