#pragma once

#include "hap.h"

class CHapEncoder : 
	public CTransformFilter,
	public ISpecifyPropertyPages,
	public CPersistStream
{

public:

	DECLARE_IUNKNOWN;
	static CUnknown * WINAPI CreateInstance (LPUNKNOWN punk, HRESULT *phr);

	// Implement the ISpecifyPropertyPages interface
	STDMETHODIMP NonDelegatingQueryInterface (REFIID riid, void ** ppv);
	STDMETHODIMP GetPages(CAUUID *pPages);

	// Overrriden from CTransformFilter base class
	HRESULT Transform (IMediaSample *pIn, IMediaSample *pOut);
	HRESULT CheckInputType (const CMediaType *mtIn);
	HRESULT CheckTransform (const CMediaType *mtIn, const CMediaType *mtOut);
	HRESULT DecideBufferSize (IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);
	HRESULT GetMediaType (int iPosition, CMediaType *pMediaType);

	// CPersistStream override
	STDMETHODIMP GetClassID (CLSID *pClsid);


	//unsigned long CompressHap(const unsigned char* inputBuffer, void* outputBuffer, unsigned long outputBufferBytes, unsigned int compressorOptions);

	// DXT software compression
	HRESULT Compress (PBYTE pSrcBuffer, PBYTE pDestBuffer, DWORD dwSize, DWORD * outputBufferEncodedSize);

private:

	CHapEncoder (TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr);
	~CHapEncoder();

	HRESULT Copy (IMediaSample *pSource, IMediaSample *pDest) const;

	size_t m_outputBufferSize = 0;

	// DXT DECOMPRESSION
	int m_dxtFlags = 0;
	PBYTE m_dxtBuffer = NULL;
	unsigned int m_dxtBufferSize = 0;

	PBYTE m_tmpBuffer = NULL;

	unsigned int m_width;
	unsigned int m_height;

	GUID m_subTypeIn;
	GUID m_subTypeOut;

	HapCompressor m_compressor;
	UINT m_chunkCount;
	bool m_useOMP;
};

// from hapcodec.h

// y must be 2^n
#define align_round(x,y) ((((unsigned int)(x))+(y-1))&(~(y-1)))

inline void * lag_aligned_malloc(void *ptr, int size, int align, char *str) {
	if (ptr) {
		try {
			_aligned_free(ptr);
		}
		catch (...) {}
	}
	return _aligned_malloc(size, align);
}

#define LAG_ALIGNED_FREE(ptr, str) { \
	if ( ptr ){ \
		try {\
			_aligned_free((void*)ptr);\
		} catch ( ... ){ } \
	} \
	ptr=NULL;\
}

