#ifndef __BINARY_SERIALISE_H__
#define __BINARY_SERIALISE_H__

#if defined(PLATFORM_MACOS)
// [Blur2PC] Packed scalar fields need memcpy on arm64, not unaligned typed accesses.
#include <cstring>
#include <stdexcept>
#endif

namespace Sys
{

// serialize a binary stream and switch endian-ness
template<typename ENDIANSWITCH>
class BinarySerialiser
{
public:
	BinarySerialiser(I_BinaryStream& out) : m_stream(out) {}

	inline bool Write(const void *pData, U32 c)
	{
		assert(m_stream.IsWriting());
        return m_stream.Write(pData, c);
	}

	// serialisation functions for all basic core types, checks buffer overflow once per type
#if defined(PLATFORM_MACOS)
// [Blur2PC] Preserve exact on-disk bytes and report actual IO failure.
#define __BINARY_OUTPUT_STREAM_MACRO(TYPE, SIZE, STYPE) \
FORCE_INLINE BinarySerialiser& operator << (TYPE value) \
{ \
    STYPE storage; \
    memcpy(&storage, &value, SIZE); \
    ENDIANSWITCH::EndianSwap(storage); \
    if (m_stream.m_pBufferEnd - m_stream.m_pCurBuffer < SIZE) { \
        if (!Write(&storage, SIZE)) throw std::runtime_error("native scalar stream write failed"); \
    } else { \
        memcpy(m_stream.m_pCurBuffer, &storage, SIZE); \
        m_stream.m_pCurBuffer += SIZE; \
    } \
    return *this; \
}
#else
#define __BINARY_OUTPUT_STREAM_MACRO(TYPE, SIZE, STYPE) \
	FORCE_INLINE BinarySerialiser& operator << (TYPE _t) \
	{ \
		STYPE t = (STYPE&)_t; \
		ENDIANSWITCH::EndianSwap(t); \
		if ((m_stream.m_pBufferEnd-m_stream.m_pCurBuffer)<(int)SIZE) \
			Write((U8*)&t, SIZE); \
		else \
		{ \
			*((STYPE*)m_stream.m_pCurBuffer) = t; \
			m_stream.m_pCurBuffer += SIZE; \
		} \
		return *this; \
	}
#endif

	__BINARY_OUTPUT_STREAM_MACRO(U8,  1, U8)
	__BINARY_OUTPUT_STREAM_MACRO(S8,  1, U8)
	__BINARY_OUTPUT_STREAM_MACRO(char,1, U8)
	__BINARY_OUTPUT_STREAM_MACRO(U16, 2, U16)
	__BINARY_OUTPUT_STREAM_MACRO(S16, 2, U16)
	__BINARY_OUTPUT_STREAM_MACRO(U32, 4, U32)
	__BINARY_OUTPUT_STREAM_MACRO(S32, 4, U32)
	__BINARY_OUTPUT_STREAM_MACRO(F32, 4, U32)
	__BINARY_OUTPUT_STREAM_MACRO(F64, 8, U64)
	__BINARY_OUTPUT_STREAM_MACRO(U64, 8, U64)
	__BINARY_OUTPUT_STREAM_MACRO(S64, 8, U64)

#undef __BINARY_OUTPUT_STREAM_MACRO

	template<typename T>
	BinarySerialiser &operator <<(T &obj)
	{
		Serialise( *this, obj );
		return *this;
	}

	template<typename T>
	BinarySerialiser &operator <<(const T &obj)
	{
		Serialise( *this, const_cast< T& >( obj ) );
		return *this;
	}
	
	I_BinaryStream &m_stream;
};

// deserialize a binary stream and switch endian-ness
template<typename ENDIANSWITCH>
class BinaryDeserialiser
{
public:
	BinaryDeserialiser(I_BinaryStream& in) : m_stream(in) {}

	inline bool Read(void *pData, U32 c)
	{
		assert(!m_stream.IsWriting());
        return m_stream.Read(pData, c);
	}

	// deserialisation functions for all basic core types, checks buffer underflow once per type
#if defined(PLATFORM_MACOS)
// [Blur2PC] Preserve exact on-disk bytes and report actual IO failure.
#define __BINARY_INPUT_STREAM_MACRO(TYPE, SIZE, STYPE) \
FORCE_INLINE BinaryDeserialiser& operator >> (TYPE& value) \
{ \
    STYPE storage; \
    if (m_stream.m_pBufferEnd - m_stream.m_pCurBuffer < SIZE) { \
        if (!Read(&storage, SIZE)) throw std::runtime_error("unexpected EOF in native scalar stream"); \
    } else { \
        memcpy(&storage, m_stream.m_pCurBuffer, SIZE); \
        m_stream.m_pCurBuffer += SIZE; \
    } \
    ENDIANSWITCH::EndianSwap(storage); \
    memcpy(&value, &storage, SIZE); \
    return *this; \
}
#else
#define __BINARY_INPUT_STREAM_MACRO(TYPE, SIZE, STYPE) \
	FORCE_INLINE BinaryDeserialiser& operator >> (TYPE &t) \
	{ \
		STYPE _t; \
		if ((m_stream.m_pBufferEnd-m_stream.m_pCurBuffer)<(int)SIZE) \
			Read((U8*)&_t, SIZE); \
		else \
		{ \
			_t = *((STYPE*)m_stream.m_pCurBuffer); \
			m_stream.m_pCurBuffer += SIZE; \
		} \
		ENDIANSWITCH::EndianSwap(_t); \
		t = (TYPE&)_t; \
		return *this; \
	}
	
#endif

	__BINARY_INPUT_STREAM_MACRO(U8,  1, U8)
	__BINARY_INPUT_STREAM_MACRO(S8,  1, U8)
	__BINARY_INPUT_STREAM_MACRO(char,1, U8)
	__BINARY_INPUT_STREAM_MACRO(U16, 2, U16)
	__BINARY_INPUT_STREAM_MACRO(S16, 2, U16)
	__BINARY_INPUT_STREAM_MACRO(U32, 4, U32)
	__BINARY_INPUT_STREAM_MACRO(S32, 4, U32)
	__BINARY_INPUT_STREAM_MACRO(F32, 4, U32)
	__BINARY_INPUT_STREAM_MACRO(F64, 8, U64)
	__BINARY_INPUT_STREAM_MACRO(U64, 8, U64)
	__BINARY_INPUT_STREAM_MACRO(S64, 8, U64)

#undef __BINARY_INPUT_STREAM_MACRO


	template<typename T>
	BinaryDeserialiser &operator >>(T &obj)
	{
		Deserialise( *this, obj );
		return *this;
	}

	I_BinaryStream&	m_stream;
};

}

#endif

