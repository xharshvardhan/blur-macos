#ifndef RACEBOOK_OBJECT_BASE_H
#define RACEBOOK_OBJECT_BASE_H

#ifdef _MSC_VER
#pragma once
#endif

#include <system/serialise/BinarySerialiseAutoEndian.h>

namespace RaceBook
{

class C_ObjectBase
{
public:
	C_ObjectBase() : mHot(false), mNew(true), mObjectId(0) {};

	bool mHot;
	U32  mObjectId;

	void Serialise( Sys::BinarySerialiserAutoEndian& stream ) {
            Sys::BinarySerialiserAutoEndian out(stream);
#if defined(PLATFORM_MACOS) // [Blur2PC] PC save fields are S32 values, not four-byte aliases of one-byte bool objects.
            out << mObjectId << S32(mHot) << S32(mNew);
#else
            out << mObjectId << (S32&)mHot << (S32&)mNew;
#endif
        };
	void Deserialise( Sys::BinaryDeserialiserAutoEndian& stream ) {
            Sys::BinaryDeserialiserAutoEndian in(stream);
#if defined(PLATFORM_MACOS) // [Blur2PC] Read the same S32 fields without overwriting adjacent members or padding.
            S32 hot,newObject;in >> mObjectId >> hot >> newObject;mHot=hot!=0;mNew=newObject!=0;
#else
            in >> mObjectId >> (S32&)mHot >> (S32&)mNew;
#endif
        };

	bool IsNew()	  { return mNew; };
	void SeenObject() { mNew = false; };

protected:
	bool mNew;
};

// Might be overkill for one variable ...
class C_MessageIdxTracker
{
public:
	static C_MessageIdxTracker& Instance(){ return s_instance; };

	void AssignId( C_ObjectBase* obj );
	void RemoveId( int i );
	void ClearIds()						{ objectList.Clear(); };
	void AddObject( C_ObjectBase* obj ) { objectList.PushBack(obj); };
	void ReorderObjects();


private:
	static C_MessageIdxTracker s_instance;
	Btl::Vector<C_ObjectBase*> objectList;
};

} // namespace RaceBook

#endif // RACEBOOK_OBJECT_BASE_H
