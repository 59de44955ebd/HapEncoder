#include <windows.h>
#include <streams.h>
#include <initguid.h>
#include <olectl.h>

// hap
#include <ppl.h>
//#include "hap.h"
#include "YCoCg.h"
#include "YCoCgDXT.h"

// squish
#include "squish.h"

#include "encoder.h"
#include "resource.h"
#include "uids.h"
#include "conversion.h"
#include "props.h"

//######################################
// OMP
//######################################
#include <omp.h>
#define USE_OPENMP_DXT

#include "dbg.h"

GUID g_usedHapType = MEDIASUBTYPE_Hap1;
bool g_useSnappy = FALSE;
UINT g_chunkCount = 1;
bool g_useOMP = TRUE;


//######################################
// Setup information
//######################################

const AMOVIESETUP_MEDIATYPE sudPinTypes = {
	&MEDIATYPE_Video,       // Major type
	&MEDIASUBTYPE_NULL      // Minor type
};

const AMOVIESETUP_PIN sudpPins[] = {
	{ L"Input",             // Pins string name
	  FALSE,                // Is it rendered
	  FALSE,                // Is it an output
	  FALSE,                // Are we allowed none
	  FALSE,                // And allowed many
	  &CLSID_NULL,          // Connects to filter
	  NULL,                 // Connects to pin
	  1,                    // Number of types
	  &sudPinTypes          // Pin information
	},
	{ L"Output",            // Pins string name
	  FALSE,                // Is it rendered
	  TRUE,                 // Is it an output
	  FALSE,                // Are we allowed none
	  FALSE,                // And allowed many
	  &CLSID_NULL,          // Connects to filter
	  NULL,                 // Connects to pin
	  1,                    // Number of types
	  &sudPinTypes          // Pin information
	}
};

const AMOVIESETUP_FILTER sudHapEncoder = {
	&CLSID_HapEncoder,      // Filter CLSID
	L"HapEncoder",          // String name 
	MERIT_NORMAL,           // Filter merit - MERIT_DO_NOT_USE ?
	2,                      // Number of pins
	sudpPins                // Pin information
};

//######################################
// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance
//######################################
CFactoryTemplate g_Templates[] = {

	{ L"HapDecoder"
	, &CLSID_HapEncoder
	, CHapEncoder::CreateInstance
	, NULL
	, &sudHapEncoder }

	,

	{ L"Settings"
	, &CLSID_HapEncoderPropertyPage
	, CHapEncoderProperties::CreateInstance }

};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

//######################################
// CreateInstance
//######################################
CUnknown *CHapEncoder::CreateInstance(LPUNKNOWN punk, HRESULT *phr) {
	ASSERT(phr);

	CHapEncoder *pNewObject = new CHapEncoder(NAME("HapEncoder"), punk, phr);

	if (pNewObject == NULL) {
		if (phr) *phr = E_OUTOFMEMORY;
	}
	return pNewObject;
}

//######################################
// Constructor
//######################################
CHapEncoder::CHapEncoder(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr) :
	CTransformFilter(tszName, punk, CLSID_HapEncoder),
	CPersistStream(punk, phr)
{
	//DBGI( omp_get_max_threads() );
}

//######################################
// Destructor
//######################################
CHapEncoder::~CHapEncoder() {

	// Hacky fix for crashes related to running OpenMP threads when exiting.
	// There are obviously better solutions - TODO
	Sleep(200);

	LAG_ALIGNED_FREE(_dxtBuffer, "dxtBuffer");
	LAG_ALIGNED_FREE(_tmpBuffer, "tmpBuffer");
}

//######################################
// NonDelegatingQueryInterface
//######################################
STDMETHODIMP CHapEncoder::NonDelegatingQueryInterface(REFIID riid, void **ppv) {
	CheckPointer(ppv,E_POINTER);
	if (riid == IID_ISpecifyPropertyPages) {
		return GetInterface((ISpecifyPropertyPages *) this, ppv);
	} else {
		return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
	}
}

//######################################
//
//######################################
//void HapMTDecode(HapDecodeWorkFunction function, void *info, unsigned int count, void *) {
//	concurrency::parallel_for((unsigned int)0, count, [&](unsigned int i) {
//		function(info, i);
//	});
//}

//######################################
// Transform
//######################################
HRESULT CHapEncoder::Transform (IMediaSample *pMediaSampleIn, IMediaSample *pMediaSampleOut) {

	CheckPointer(pMediaSampleIn, E_POINTER);
	CheckPointer(pMediaSampleOut, E_POINTER);

	HRESULT hr;

	// Copy the sample properties across
	hr = Copy(pMediaSampleIn, pMediaSampleOut);
	if (FAILED(hr)) return hr;

	// Pointers to the actual image buffers
	PBYTE pSrcBuffer;
	hr = pMediaSampleIn->GetPointer(&pSrcBuffer);
	if (FAILED(hr)) return hr;

	PBYTE pDestBuffer;
	hr = pMediaSampleOut->GetPointer(&pDestBuffer);
	if (FAILED(hr)) return hr;

	DWORD outputBufferEncodedSize;

	//######################################
	// Compress RGB32 to texture
	//######################################
	hr = Compress(
		pSrcBuffer, 
		pDestBuffer, 
		pMediaSampleIn->GetActualDataLength(), 
		&outputBufferEncodedSize);

	if (FAILED(hr)) return hr;

	pMediaSampleOut->SetActualDataLength(outputBufferEncodedSize);

	return NOERROR;
}

//######################################
// Compresses RGB32 as DXT texture
//######################################
HRESULT CHapEncoder::Compress (PBYTE pSrcBuffer, PBYTE pDestBuffer, DWORD dwSizeX, DWORD * outputBufferEncodedSize) {

	ConvertBGRAtoRGBA(_width, _height, pSrcBuffer, _tmpBuffer, m_useOMP);
	pSrcBuffer = _tmpBuffer;

	FlipVerticallyInPlace(pSrcBuffer, _width * 4, _height, m_useOMP);

	if (m_subTypeOut == MEDIASUBTYPE_Hap1) {

		if (m_useOMP) {
			//#ifdef USE_OPENMP_DXT
			int numThreads = omp_get_max_threads();
#pragma omp parallel num_threads(numThreads)
			{

				int chunkHeight = _height / numThreads;
				int threadChunkBytes = _width * chunkHeight * 4;
				int threadDxtChunkBytes = threadChunkBytes / 8;
				int id = omp_get_thread_num();
				int offset1 = id * threadChunkBytes;
				int offset2 = id * threadDxtChunkBytes;

#pragma omp for 
				for (int i = 0; i < numThreads; i++)
				{
					squish::CompressImage(pSrcBuffer + offset1, _width, chunkHeight, (unsigned char*)_dxtBuffer + offset2, _dxtFlags, NULL);
				}
			}
			//#else
		}
		else {
			squish::CompressImage(pSrcBuffer, _width, _height, _dxtBuffer, _dxtFlags, NULL);
//#endif
		}

	}

	else if (m_subTypeOut == MEDIASUBTYPE_Hap5) {
//#ifdef USE_OPENMP_DXT
		if (m_useOMP) {
			int numThreads = omp_get_max_threads();
#pragma omp parallel num_threads(numThreads)
			{
				int chunkHeight = _height / numThreads;
				int threadChunkBytes = _width * chunkHeight * 4;
				int threadDxtChunkBytes = threadChunkBytes / 4;
				int id = omp_get_thread_num();
				DBGI(id);
				int offset1 = id * threadChunkBytes;
				int offset2 = id * threadDxtChunkBytes;
#pragma omp for 
				for (int i = 0; i < numThreads; i++)
				{
					squish::CompressImage(pSrcBuffer + offset1, _width, chunkHeight, (unsigned char*)_dxtBuffer + offset2, _dxtFlags, NULL);
				}
			}
			//#else
		}
		else {
			squish::CompressImage(pSrcBuffer, _width, _height, _dxtBuffer, _dxtFlags, NULL);
			//#endif
		}
	} 
	
	else if (m_subTypeOut == MEDIASUBTYPE_HapY) {

		// Convert to YCoCg
		ConvertRGBAToCoCgAY8888(
			(unsigned char*)pSrcBuffer,
			(unsigned char*)_tmpBuffer,
			(unsigned long)_width,
			(unsigned long)_height,
			(size_t)_width * 4,
			(size_t)_width * 4, 
			m_useOMP);

		// Convert to DXT format
		int outputSize = CompressYCoCgDXT5(
			(const unsigned char*)_tmpBuffer,
			(unsigned char*)_dxtBuffer,
			_width,
			_height,
			_width * 4);

		assert(outputSize <= _dxtBufferSize);
	} 
	
	else return E_FAIL; // should never happen

	unsigned long inputBuffersBytes[1];
	inputBuffersBytes[0] = _dxtBufferSize;

	unsigned int textureFormats[1];

	if (m_subTypeOut == MEDIASUBTYPE_Hap1) // Hap
		textureFormats[0] = HapTextureFormat_RGB_DXT1;

	else if (m_subTypeOut == MEDIASUBTYPE_Hap5) // Hap Alpha
		textureFormats[0] = HapTextureFormat_RGBA_DXT5;

	else  if (m_subTypeOut == MEDIASUBTYPE_HapY) // Hap Q
		textureFormats[0] = HapTextureFormat_YCoCg_DXT5;

	unsigned int compressors[1];
	compressors[0] = m_compressor;

	unsigned int chunkCounts[1];
	chunkCounts[0] = m_chunkCount;

	unsigned int res = HapEncode(
		1,
		(const void **)&_dxtBuffer,

		inputBuffersBytes,
		textureFormats,
		compressors,
		chunkCounts,

		pDestBuffer,
		(unsigned long)m_outputBufferSize,
		outputBufferEncodedSize
	);

	if (res != HapResult_No_Error) {
		NOTE("HapEncode error");
		return E_FAIL;
	}

	return S_OK;
}

//######################################
// Copy
// Make destination an identical copy of source
//######################################
HRESULT CHapEncoder::Copy (IMediaSample *pSource, IMediaSample *pDest) const {

	CheckPointer(pSource, E_POINTER);   
	CheckPointer(pDest ,E_POINTER);   

	// Copy the sample times
	REFERENCE_TIME TimeStart, TimeEnd;
	if (NOERROR == pSource->GetTime(&TimeStart, &TimeEnd)) {
		pDest->SetTime(&TimeStart, &TimeEnd);
	}

	LONGLONG MediaStart, MediaEnd;
	if (pSource->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR) {
		pDest->SetMediaTime(&MediaStart,&MediaEnd);
	}

	// Copy the Sync point property
	HRESULT hr = pSource->IsSyncPoint();
	if (hr == S_OK) {
		pDest->SetSyncPoint(TRUE);
	}
	else if (hr == S_FALSE) {
		pDest->SetSyncPoint(FALSE);
	}
	else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	// Copy the media type
	AM_MEDIA_TYPE *pMediaType;
	pSource->GetMediaType(&pMediaType);
	pDest->SetMediaType(pMediaType);
	DeleteMediaType(pMediaType);

	// Copy the preroll property
	hr = pSource->IsPreroll();
	if (hr == S_OK) {
		pDest->SetPreroll(TRUE);
	}
	else if (hr == S_FALSE) {
		pDest->SetPreroll(FALSE);
	}
	else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	// Copy the discontinuity property
	hr = pSource->IsDiscontinuity();
	if (hr == S_OK) {
		pDest->SetDiscontinuity(TRUE);
	}
	else if (hr == S_FALSE) {
		pDest->SetDiscontinuity(FALSE);
	}
	else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	return S_OK;
}

//######################################
// Check the input type is OK - return an error otherwise
//######################################
HRESULT CHapEncoder::CheckInputType (const CMediaType *pMediaType) {
	NOTE("CheckInputType");

	CheckPointer(pMediaType, E_POINTER);

	// check this is a VIDEOINFOHEADER type
	if (*pMediaType->FormatType() != FORMAT_VideoInfo) {
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// Check the format looks reasonably ok
	ULONG Length = pMediaType->FormatLength();
	if (Length < SIZE_VIDEOHEADER) {
		NOTE("Format smaller than a VIDEOHEADER");
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// Check if the media major type is MEDIATYPE_Video
	const GUID *pMajorType = pMediaType->Type();
	if (*pMajorType != MEDIATYPE_Video) {
		NOTE("Major type not MEDIATYPE_Video");
		return VFW_E_TYPE_NOT_ACCEPTED;;
	}

	// Check if the media subtype is RGB32
	const GUID *pSubType = pMediaType->Subtype();
	if (*pSubType != MEDIASUBTYPE_RGB32) return VFW_E_TYPE_NOT_ACCEPTED;;

	return S_OK;
}

//######################################
// GetMediaType
// Returns one of the filter's preferred output types, referenced by index number
//######################################
HRESULT CHapEncoder::GetMediaType (int iPosition, CMediaType *pMediaType) {

	// Is the input pin connected
	if (m_pInput->IsConnected() == FALSE) return E_UNEXPECTED;

	// This should never happen
	if (iPosition < 0) return E_INVALIDARG;

	// Do we have more items to offer
	if (iPosition > 2) return VFW_S_NO_MORE_ITEMS;

	CheckPointer(pMediaType, E_POINTER);

	// The ConnectionMediaType method retrieves the media type for the current pin connection, if any.
	HRESULT hr = m_pInput->ConnectionMediaType(pMediaType);
	if (FAILED(hr))	return hr;

	//ASSERT(pMediaType->formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaType->pbFormat);

	pMediaType->cbFormat = 88;
	pMediaType->bFixedSizeSamples = FALSE;
	pMediaType->SetTemporalCompression(TRUE);

	pVih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	switch (iPosition) {

		case 0: // Hap1
			pMediaType->subtype = MEDIASUBTYPE_Hap1;
			pVih->bmiHeader.biCompression = FOURCC_Hap1;

			pMediaType->SetSampleSize(pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight); // ???

			//dwBitRate: Approximate data rate of the video stream, in bits per second.
			//pVih->dwBitRate = (DWORD)floor(10000000.0 / pVih->AvgTimePerFrame * pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 8.0 + 0.5);
			pVih->bmiHeader.biSizeImage = pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight   * 3;
			pVih->bmiHeader.biBitCount = 24;
			break;

		case 1: // Hap5
			pMediaType->subtype = MEDIASUBTYPE_Hap5;
			pVih->bmiHeader.biCompression = FOURCC_Hap5;

			pMediaType->SetSampleSize(pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 2);

			//pVih->dwBitRate = (DWORD)floor(10000000.0 / pVih->AvgTimePerFrame * pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 16.0 + 0.5);
			pVih->bmiHeader.biSizeImage = pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 2;
			pVih->bmiHeader.biBitCount = 32;
			break;

		case 2: // HapY
			pMediaType->subtype = MEDIASUBTYPE_HapY;
			pVih->bmiHeader.biCompression = FOURCC_HapY;

			pMediaType->SetSampleSize(pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 2);

			//pVih->dwBitRate = (DWORD)floor(10000000.0 / pVih->AvgTimePerFrame * pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 16.0 + 0.5);
			pVih->bmiHeader.biSizeImage = pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 2;
			pVih->bmiHeader.biBitCount = 24;
			break;

	}

	return S_OK;
}

//######################################
// CheckTransform
// Check if a transform can be done between these formats
//######################################
HRESULT CHapEncoder::CheckTransform (const CMediaType *pMediaTypeIn, const CMediaType *pMediaTypeOut) {

	CheckPointer(pMediaTypeIn, E_POINTER);
	CheckPointer(pMediaTypeOut, E_POINTER);

	// check the input subtype - do we need this here?
	GUID subTypeIn = pMediaTypeIn->subtype;
	if (subTypeIn != MEDIASUBTYPE_RGB32) {
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// Check the output major type.
	if (pMediaTypeOut->majortype != MEDIATYPE_Video) {
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// Check the output format type.
	if ((pMediaTypeOut->formattype != FORMAT_VideoInfo) || (pMediaTypeOut->cbFormat < sizeof(VIDEOINFOHEADER))) {
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaTypeIn->pbFormat);
	_width = pVih->bmiHeader.biWidth;
	_height = pVih->bmiHeader.biHeight;

	//######################################
	// Check the output subtype - it must be a Hap format
	//######################################
	GUID subTypeOut = pMediaTypeOut->subtype;
	if (subTypeOut == g_usedHapType
		//subTypeOut == MEDIASUBTYPE_Hap1 || 
		//subTypeOut == MEDIASUBTYPE_Hap5 || 
		//subTypeOut == MEDIASUBTYPE_HapY
	) {
		m_subTypeIn = subTypeIn;
		m_subTypeOut = subTypeOut;
		return S_OK;
	}

	return VFW_E_TYPE_NOT_ACCEPTED;
}

//######################################
// DecideBufferSize
// Tell the output pin's allocator what size buffers we
// require. Can only do this when the input is connected
//######################################
HRESULT CHapEncoder::DecideBufferSize (IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties) {

	// Is the input pin connected
	if (m_pInput->IsConnected() == FALSE) return E_UNEXPECTED;
 
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(pProperties, E_POINTER);

	// Calculate needed output buffer size
	_dxtFlags = (m_subTypeOut == MEDIASUBTYPE_Hap1 ? squish::kDxt1 : squish::kDxt5);
	_dxtBufferSize = squish::GetStorageRequirements(_width, _height, _dxtFlags);

	_dxtBuffer = (unsigned char *)lag_aligned_malloc(_dxtBuffer, _dxtBufferSize, 16, "dxtBuffer");
	if (!_dxtBuffer) return E_OUTOFMEMORY;

	m_outputBufferSize = align_round(_width, 16) * _height * 4 + 4096;

	_tmpBuffer = (unsigned char *)lag_aligned_malloc(_tmpBuffer, _width * _height * 4, 16, "tmpBuffer");
	if (!_tmpBuffer) {
		LAG_ALIGNED_FREE(_dxtBuffer, "dxtBuffer");
		return E_OUTOFMEMORY;
	}

	// Ask the allocator to reserve us some sample memory, NOTE the function
	// can succeed (that is return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted

	pProperties->cBuffers = 1;
	pProperties->cbBuffer = m_outputBufferSize;

	// Set allocator properties.
	ALLOCATOR_PROPERTIES Actual;
	HRESULT hr = pAlloc->SetProperties(pProperties, &Actual);
	if (FAILED(hr)) return hr;

	// Even when it succeeds, check the actual result.
	if (pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer) {
		return E_FAIL;
	}

	// store compression settings for this session
	m_compressor = g_useSnappy ? HapCompressorSnappy : HapCompressorNone;
	m_chunkCount = g_chunkCount;
	m_useOMP = g_useOMP;

	return S_OK;
}

//######################################
// GetClassID
//######################################
STDMETHODIMP CHapEncoder::GetClassID (CLSID *pClsid) {
	return CBaseFilter::GetClassID(pClsid);
}

//######################################
// GetPages
//######################################
STDMETHODIMP CHapEncoder::GetPages(CAUUID *pPages) {
	CheckPointer(pPages,E_POINTER);

	pPages->cElems = 1;
	pPages->pElems = (GUID *) CoTaskMemAlloc(sizeof(GUID));
	if (pPages->pElems == NULL) return E_OUTOFMEMORY;

	*(pPages->pElems) = CLSID_HapEncoderPropertyPage;
	return S_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
////////////////////////////////////////////////////////////////////////

//######################################
// DllRegisterServer
//######################################
STDAPI DllRegisterServer() {
	return AMovieDllRegisterServer2(TRUE);
}

//######################################
// DllUnregisterServer
//######################################
STDAPI DllUnregisterServer() {
	return AMovieDllRegisterServer2(FALSE);
}

//######################################
// DllEntryPoint
//######################################
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved) {
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
