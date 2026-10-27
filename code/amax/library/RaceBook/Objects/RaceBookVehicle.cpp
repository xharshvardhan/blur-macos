#include "stdafx.h"

#include <RaceBook/Objects/RaceBookVehicle.h>
#include <RaceBook/RaceBookManager.h>

namespace RaceBook
{
	void C_RaceBookVehicle::AddPerkSlot()
	{
		mPerkSlots++;

		if(mPerkSlots > mNumberOfBaseSlots+mAdditionalSlotsCost.Size())
		{
			mPerkSlots=mNumberOfBaseSlots+mAdditionalSlotsCost.Size();
		}
	}

	bool C_RaceBookVehicle::MaxiumumSlots()
	{
		return(mPerkSlots == mNumberOfBaseSlots+mAdditionalSlotsCost.Size());
	}

	float C_RaceBookVehicle::UpgradeCost()
	{
		return (float)mAdditionalSlotsCost[mPerkSlots-mNumberOfBaseSlots];
	}

	void C_RaceBookVehicle::Serialise( Sys::BinarySerialiserAutoEndian& stream )     
	{ 
		Sys::BinarySerialiserAutoEndian out( stream ); 

		// Yet more "we shouldn't need these checks if the data is filled in properly - but they never seem to be" 
		if( mOwner )
			out << mOwner->GetID();
		else
			out << U32(0); // [Blur2PC] The serialized owner ID is always a 32-bit field, including LP64 hosts.
		
		out << mBaseModelID;
		#if defined(PLATFORM_MACOS) // [Blur2PC] Preserve each original 32-bit save field without bool aliasing.
        out << S32(mIsForSale) << S32(mDisallowedInSplitscreen) << mCost << mID;
#else
        out << (S32&)mIsForSale << (S32&)mDisallowedInSplitscreen << mCost << mID;
#endif

		out << mPerkSlots << mNumberOfBaseSlots;

		out << mAdditionalSlotsCost.Size();

		for(int a=0;a<mAdditionalSlotsCost.Size();a++)
		{
			out << mAdditionalSlotsCost[a];
		}

		C_ObjectBase::Serialise(stream);
	}

	void C_RaceBookVehicle::Deserialise( Sys::BinaryDeserialiserAutoEndian& stream ) 
	{ 
		Sys::BinaryDeserialiserAutoEndian in( stream ); 
		int num_additional_slots;
		int cost;

		U32 ownerID;

		#if defined(PLATFORM_MACOS) // [Blur2PC] Deserialize S32 booleans into their actual one-byte members.
        S32 isForSale,disallowed;in >> ownerID >> mBaseModelID >> isForSale >> disallowed >> mCost >> mID;
        mIsForSale=isForSale!=0;mDisallowedInSplitscreen=disallowed!=0;
#else
        in >> ownerID >> mBaseModelID >> (S32&)mIsForSale >> (S32&)mDisallowedInSplitscreen >> mCost >> mID;
#endif

		in >> mPerkSlots >> mNumberOfBaseSlots;

		in >> num_additional_slots;
		mAdditionalSlotsCost.Clear();
		for(int a=0;a<num_additional_slots;a++)
		{
			in >> cost;
			mAdditionalSlotsCost.PushBack(cost);
		}
		
		if( ownerID )
		{
			for( int i = 0; i < C_RaceBookManager::Instance().NumProfiles(); i++ )
			{
				if( C_RaceBookManager::Instance().GetProfileByIndex(i)->GetID() == ownerID )
					mOwner = C_RaceBookManager::Instance().GetProfileByIndex(i);
			}
		}

		C_ObjectBase::Deserialise(stream);
	}
} // namespace RaceBook
