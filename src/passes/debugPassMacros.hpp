#pragma once
#include <d3d11_1.h>
#include <wrl.h>



using namespace Microsoft::WRL;

#ifdef _DEBUG
#define DEBUG_PASS_START(name) \
	do { \
		ComPtr<ID3DUserDefinedAnnotation> annotation; \
		m_context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&annotation); \
		if (annotation) \
		{ \
			annotation->BeginEvent(name); \
		} \
	} while(0)
#define DEBUG_PASS_END() \
	do { \
		ComPtr<ID3DUserDefinedAnnotation> annotation; \
		m_context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&annotation); \
		if (annotation) \
		{ \
			annotation->EndEvent(); \
		} \
	} while(0)
#else
#define DEBUG_PASS_END()
#define DEBUG_PASS_START(name)
#endif
