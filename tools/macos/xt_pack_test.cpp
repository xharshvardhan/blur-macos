// [Blur2PC][macOS] Authored PC32 Flask bytes, using the original exporter's
// sequential field offsets and fixed-width Common/XtFlask.h wire structures.
// This tests actual schema conversion, not a native-layout memory image.
#include <core/Core.h>
#include <Xt/XtGame.h>
#include "Common/XtFlask.h"
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <cstdint>

struct PackLeaf { XtI32 value; XtF32 weight; };
struct PackRoot {
    XtU32 id; XtSz8 name; XtSz16 label; XtArray<XtI32> values;
    PackLeaf* child; XtHandle<PackLeaf> target;
    XtArray<PackLeaf*> links; XtArray<XtHandle<PackLeaf>> refs;
    XtF64 ratio; PackLeaf embedded; XtI64 signedWide; XtU64 unsignedWide;
};
XT_TYPE(PackLeaf,"PCPackLeaf")
    XT_FIELD(value,"value") XT_FIELD(weight,"weight")
XT_END_TYPE
XT_TYPE(PackRoot,"PCPackRoot")
    XT_FIELD(id,"id") XT_FIELD(name,"name") XT_FIELD(label,"label")
    XT_FIELD(values,"values") XT_FIELD(child,"child") XT_FIELD(target,"target")
    XT_FIELD(links,"links") XT_FIELD(refs,"refs") XT_FIELD(ratio,"ratio")
    XT_FIELD(embedded,"embedded") XT_FIELD(signedWide,"signedWide") XT_FIELD(unsignedWide,"unsignedWide")
XT_END_TYPE
XT_APPLICATION
    XT_REGISTER_TYPE(PackLeaf) XT_REGISTER_TYPE(PackRoot)
XT_END_APPLICATION
static void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
static_assert(sizeof(FlaskHeader)==72 && sizeof(FlaskType)==12 && sizeof(FlaskField)==8 &&
    sizeof(FlaskRef)==8 && sizeof(FlaskRecord)==16 && sizeof(FlaskComponent)==6 &&
    sizeof(FlaskArray)==8 && sizeof(FlaskPtr)==4 && sizeof(FlaskHandle)==4,"PC wire structures changed");
static_assert(sizeof(XtSz16::Character)==2,"sz16 must remain UTF16");
static_assert(sizeof(PackRoot)>76 && offsetof(PackRoot,ratio)!=44,"fixture must differ from PC wire layout");

// Reflection annotations execute before main. None may create/destroy a game
// object merely to discover its nonvirtual base adjustment.
static int layoutConstructed=0,layoutDestroyed=0;
struct LayoutLeading { virtual ~LayoutLeading()=default; XtU64 marker=1; };
struct LayoutBase { virtual ~LayoutBase()=default; XtI32 field=2; };
struct LayoutNoDefault:LayoutLeading,LayoutBase {
    explicit LayoutNoDefault(int) {++layoutConstructed;}
    ~LayoutNoDefault() override {++layoutDestroyed;}
};
static const int layoutOffset=Xt::FaBase_Offset<LayoutNoDefault,LayoutBase>();
static_assert(__is_convertible_to(int LayoutBase::*,int LayoutNoDefault::*),"nonvirtual fixture");
struct LayoutVirtual:virtual LayoutBase {};
static_assert(!__is_convertible_to(int LayoutBase::*,int LayoutVirtual::*),"virtual bases must be rejected");
static void testLayoutAndRelocation() {
    require(layoutConstructed==0 && layoutDestroyed==0,"static reflection ran gameplay constructors/destructors");
    { LayoutNoDefault object(0);auto* adjusted=reinterpret_cast<LayoutBase*>(reinterpret_cast<XtU8*>(&object)+layoutOffset);
      require(layoutOffset>0 && adjusted==static_cast<LayoutBase*>(&object) && adjusted->field==2,"nonzero base adjustment changed"); }
    require(layoutConstructed==1 && layoutDestroyed==1,"reflection fixture lifetime changed");
    // Separately exercise native memory-image relocation, which is not a Flask
    // PC32 converter. Synthetic old addresses are never accessed.
    struct Slab { XtU8* pointer; XtU64 marker; } slab{};
    const XtU32 fixup=offsetof(Slab,pointer);const uintptr_t current=reinterpret_cast<uintptr_t>(&slab);
    for(uintptr_t old:{current+0x100000000ULL,current-0x100000000ULL}) {
        slab.pointer=reinterpret_cast<XtU8*>(old+offsetof(Slab,marker));slab.marker=0xabcdef;
        XtApplyRecordFixups(&slab,reinterpret_cast<void*>(old),&fixup,1,sizeof(slab));
        require(slab.pointer==reinterpret_cast<XtU8*>(&slab.marker) && slab.marker==0xabcdef,"native relocation delta truncated above2GiB");
    }
}

using Bytes=std::vector<XtU8>;
template<class T> static void put(Bytes& bytes,std::size_t offset,T value) {
    require(offset+sizeof(T)<=bytes.size(),"fixture write outside authored wire data");
    std::memcpy(bytes.data()+offset,&value,sizeof(value));
}
static XtU16 string(Bytes& bytes,const char* value) {
    const auto offset=bytes.size();bytes.insert(bytes.end(),value,value+std::strlen(value)+1);return XtU16(offset);
}
static void align4(Bytes& bytes) {bytes.resize((bytes.size()+3)&~std::size_t(3));}
template<class T> static FlaskBlob blob(Bytes& bytes,const std::vector<T>& values) {
    align4(bytes);FlaskBlob result{XtU32(bytes.size()),XtU32(values.size())};
    const auto* data=reinterpret_cast<const XtU8*>(values.data());
    if(!values.empty())bytes.insert(bytes.end(),data,data+values.size()*sizeof(T));return result;
}
static Bytes pack(int version) {
    Bytes names;std::vector<FlaskType> types;
    auto atom=[&](XtAtomType atomType,XtU16 size) {
        auto* schema=XtGetApplicationSchema();const XtType* found=nullptr;
        for(int i=0;i<schema->typeCount;++i)if(schema->types[i].behaviour==kXtTbAtom && schema->types[i].atom==atomType)found=&schema->types[i];
        require(found,"authored atom missing from actual schema");
        types.push_back({string(names,found->name),0,0,kXtTbAtom,XtU16(atomType),size});
    };
    atom(kXtU32,4);atom(kXtI32,4);atom(kXtF32,4);atom(kXtSz8,4);atom(kXtSz16,4);atom(kXtF64,8);
    types.push_back({string(names,"PCPackLeaf"),2,0,kXtTbStruct,kXtNotAtom,8});
    types.push_back({string(names,"PCPackRoot"),12,0,kXtTbStruct,kXtNotAtom,76});
    atom(kXtI64,8);atom(kXtU64,8);
    std::vector<FlaskField> fields;
    auto field=[&](const char* name,XtU16 type,XtU16 offset,XtFieldBehaviour behaviour,bool array=false) {
        fields.push_back({string(names,name),type,offset,XtU8(behaviour),XtU8(array)});
    };
    field("value",1,0,kXtFbAtom);field("weight",2,4,kXtFbAtom);
    field("id",0,0,kXtFbAtom);field("name",3,4,kXtFbAtom);field("label",4,8,kXtFbAtom);
    field("values",1,12,kXtFbAtom,true);field("child",6,20,kXtFbPtr);field("target",6,24,kXtFbHandle);
    field("links",6,28,kXtFbPtr,true);field("refs",6,36,kXtFbHandle,true);
    field("ratio",5,44,kXtFbAtom);field("embedded",6,52,kXtFbStruct);field("signedWide",8,60,kXtFbAtom);field("unsignedWide",9,68,kXtFbAtom);
    Bytes strings;const auto nameOffset=string(strings,"pack-native");
    if(strings.size()%2)strings.push_back(0);
    const auto labelOffset=XtU32(strings.size());
    const char16_t label[]=u"Fran\u00e7ais \u0939\u093f\u0928\u094d\u0926\u0940 \U0001f3ce";
    const auto* labelBytes=reinterpret_cast<const XtU8*>(label);
    strings.insert(strings.end(),labelBytes,labelBytes+sizeof(label));align4(strings);
    Bytes data(120,0);
    put(data,0,XtU32(0x76543210));put(data,4,XtU32(nameOffset));put(data,8,labelOffset);
    put(data,12,FlaskArray{2,0,3});put(data,20,FlaskPtr{1,1});put(data,24,FlaskHandle{1});
    put(data,28,FlaskArray{3,0,2});put(data,36,FlaskArray{4,0,2});
    put(data,44,XtF64(12345.125));put(data,52,XtI32(-77));put(data,56,XtF32(0.375));
    put(data,60,XtI64(-0x123456789abcdefLL));put(data,68,XtU64(0xfedcba9876543210ULL));
    put(data,76,XtI32(11));put(data,80,XtF32(1.5));put(data,84,XtI32(22));put(data,88,XtF32(2.5));
    put(data,92,XtI32(-3));put(data,96,XtI32(0));put(data,100,XtI32(98765));
    put(data,104,FlaskPtr{1,0});put(data,108,FlaskPtr{1,1});put(data,112,FlaskHandle{1});put(data,116,FlaskHandle{1});
    if(version==2) {XtFlaskEncodeStrings(names.data(),int(names.size()));XtFlaskEncodeStrings(strings.data(),int(strings.size()));}
    data.insert(data.end(),strings.begin(),strings.end());const auto second=data.size();data.resize(second+8);
    put(data,second,XtI32(444));put(data,second+4,XtF32(6.25));
    Bytes bytes(sizeof(FlaskHeader));FlaskHeader header{};header.tag=kXtFlaskTag;header.version=version;
    header.types=blob(bytes,types);header.bases=blob(bytes,std::vector<FlaskBase>{});header.fields=blob(bytes,fields);
    header.strings=blob(bytes,names);header.refs=blob(bytes,std::vector<FlaskRef>{{0x100,7,0},{0x200,6,1}});
    header.records=blob(bytes,std::vector<FlaskRecord>{{0,5,120,XtU32(strings.size())},{1,1,8,0}});
    header.components=blob(bytes,std::vector<FlaskComponent>{{7,1,kXtCbObject},{6,2,kXtCbObject},{1,3,kXtCbObject},{6,2,kXtCbPtr},{6,2,kXtCbHandle},{6,1,kXtCbObject}});
    // Components need only 4-byte alignment. Place this root at an 8-byte address
    // so the exporter's F64 field at 44 is deliberately not 8-byte aligned.
    bytes.resize((bytes.size()+7)&~std::size_t(7));
    header.data=blob(bytes,data);put(bytes,0,header);return bytes;
}
static void testPack(int version) {
    XtCallbackContext context(0);auto bytes=pack(version);auto* db=XtDbCreate(XtGetApplicationSchema(),16,&context);require(db,"Xt database allocation failed");
    auto* loaded=XtPackOpen(db,bytes.data(),bytes.size());require(loaded,"actual PC32 Flask import rejected authored pack");
    XtHandle<PackRoot> root(XtGetRecord(db,0x100));XtHandle<PackLeaf> target(XtGetRecord(db,0x200));
    require(root && target,"actual pack record lookup failed");
    require(root->id==0x76543210 && std::strcmp(root->name.Text(),"pack-native")==0,"PC scalar/string fields changed");
    require(std::char_traits<char16_t>::compare(root->label.Text(),u"Fran\u00e7ais \u0939\u093f\u0928\u094d\u0926\u0940 \U0001f3ce",sizeof(u"Fran\u00e7ais \u0939\u093f\u0928\u094d\u0926\u0940 \U0001f3ce")/sizeof(char16_t))==0,"UTF16 units changed");
    require(root->values.count==3 && root->values[0]==-3 && root->values[2]==98765,"PC array relocation failed");
    require(root->links.count==2 && root->links[0]->value==11 && root->child==root->links[1] && root->child->value==22,"PC pointer component aliases changed");
    require(root->target==target && root->refs.count==2 && root->refs[0]==target && root->refs[1]==target && target->value==444 && target->weight==6.25,"cross-record handle conversion failed");
    require(root->signedWide==-0x123456789abcdefLL && root->unsignedWide==0xfedcba9876543210ULL,"4-aligned 64-bit integer fields changed");
    require(root->ratio==12345.125 && root->embedded.value==-77 && root->embedded.weight==0.375,"4-aligned F64/embedded fields changed");
    // The original native fixup generator must describe both root members and
    // pointer-array elements. Test its actual output separately from disk decode.
    auto* record=XtGetRecord(db,0x100);auto* original=XtGetRecordBaseAddr(record);
    const auto recordSize=XtGetRecordSize(record);const auto count=XtGetRecordFixupCount(record);
    require(count==8 && recordSize>sizeof(PackRoot),"native component fixup count/size changed");
    std::vector<XtU32> fixups(count);XtGenerateRecordFixups(record,fixups.data(),count);
    Bytes copy(recordSize);std::memcpy(copy.data(),original,recordSize);
    XtApplyRecordFixups(copy.data(),original,fixups.data(),count,recordSize);
    for(auto offset:fixups) {
        uintptr_t oldPointer,newPointer;std::memcpy(&oldPointer,original+offset,sizeof(oldPointer));
        std::memcpy(&newPointer,copy.data()+offset,sizeof(newPointer));
        require(newPointer-reinterpret_cast<uintptr_t>(copy.data())==oldPointer-reinterpret_cast<uintptr_t>(original),
                "generated native pointer relocation changed relative component/string position");
    }
    // Slab allocations and strings must survive the caller discarding the input.
    std::fill(bytes.begin(),bytes.end(),0xa5);
    require(root->values[1]==0 && root->label.Text()[4]==u'\u00e7',"import retained pointers to wire data");
    XtPackClose(db,loaded);require(!root && !target,"pack-close did not invalidate handles");XtDbDestroy(db);
    auto broken=pack(version);broken.resize(sizeof(FlaskHeader)-1);db=XtDbCreate(XtGetApplicationSchema(),16,&context);
    require(!XtPackOpen(db,broken.data(),broken.size()),"truncated Flask header accepted");XtDbDestroy(db);
}
int main() {
    try {testLayoutAndRelocation();require(XtInitialise(),"Xt startup failed");testPack(1);testPack(2);XtShutdown();
        std::cout<<"PC32 Flask v1/v2: native fields, UTF16, arrays, pointers, handles, close and rejection PASS\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
