// [Blur2PC] Original RaceBook records keep PC-width fields on LP64, using actual engine memory streams.
#include "StdAfx.h"
#include <RaceBook/Objects/RaceBookVehicle.h>
#include <system/serialise/C_MemoryStream.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    for(bool swap:{false,true}){
        RaceBook::C_RaceBookVehicle vehicle;vehicle.mBaseModelID=0x10203040;vehicle.mIsForSale=true;vehicle.mDisallowedInSplitscreen=false;vehicle.mCost=0x12345678;vehicle.mID=77;vehicle.mPerkSlots=3;vehicle.mNumberOfBaseSlots=2;vehicle.mAdditionalSlotsCost.PushBack(500);vehicle.mAdditionalSlotsCost.PushBack(900);vehicle.mObjectId=0x88776655;vehicle.mHot=true;vehicle.SeenObject();
        Sys::C_MemoryStream stream(256);stream.SetEndianSwapFlag(swap);Sys::BinarySerialiserAutoEndian writer(stream);vehicle.Serialise(writer);
        const std::array<U32,14> expected{{0,0x10203040,1,0,0x12345678,77,3,2,2,500,900,0x88776655,1,0}};
        require(U64(stream.GetPosition())==expected.size()*4,"Original RaceBook owner/boolean fields changed PC record width");
        const U8* actual=stream.GetBufferPtr();
        for(std::size_t field=0;field<expected.size();++field)for(U32 byte=0;byte<4;++byte){
            const U32 shift=(swap?(3-byte):byte)*8;
            require(actual[field*4+byte]==U8(expected[field]>>shift),"Original RaceBook serialization changed field order, endianness, or boolean values");
        }
        alignas(4) std::array<U8,12> baseBytes;std::memcpy(baseBytes.data(),actual+11*4,baseBytes.size());
        Sys::C_MemoryStream read(baseBytes.data(),baseBytes.size());read.SetEndianSwapFlag(swap);Sys::BinaryDeserialiserAutoEndian reader(read);
        RaceBook::C_ObjectBase base;base.mObjectId=0;base.mHot=false;base.Deserialise(reader);
        require(base.mObjectId==vehicle.mObjectId&&base.mHot&&!base.IsNew()&&U64(read.GetPosition())==12,"Original RaceBook base deserialization changed adjacent state or PC width");
        require(vehicle.mCost==0x12345678&&vehicle.mIsForSale&&!vehicle.mDisallowedInSplitscreen,"RaceBook serialization modified source state");
    }
    std::cout<<"Original RaceBook PC-width owner/boolean records, exact bytes and both endian memory streams PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
