#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NORESOURCE
#define NOHELP
#define NOGDICAPMASKS
#define STRICT
#include <windows.h>

#include <memory>
#include <new>

class IMemAlloc
{
public:
	virtual void *Alloc( size_t nSize ) = 0;
	virtual void *Realloc( void *pMem, size_t nSize ) = 0;
	virtual void Free( void *pMem ) = 0;
	virtual void Expand( void *, size_t ) = 0;
	virtual void Alloc( size_t, const char *, int ) = 0;
	virtual void Realloc( void *, size_t, const char *, int ) = 0;
	virtual void Free( void *, const char *, int ) = 0;
	virtual void Expand( void *, size_t, const char *, int ) = 0;
	virtual void *AllocAlign( size_t nSize, size_t align ) = 0;
	virtual void AllocAlign( size_t, size_t, const char *, int ) = 0;
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align ) = 0;
};
#define CONSTRUCT_EARLY
#pragma warning( disable : 4075 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( ".CRT$XCB" )
static struct Initializer final
{
	void Init()
	{
		if ( alloc )
			return;
		auto tier0 = GetModuleHandleW( L"tier0.dll" );
		if ( !tier0 )
		{
			new ( &stub ) AllocStub();
			stub.Init();
			alloc = &stub;
			return;
		}
		alloc = *(IMemAlloc**)GetProcAddress( tier0, "g_pMemAlloc" );
	}

	struct AllocStub final : IMemAlloc
	{
		void Init()
		{
			auto ucrt = LoadLibraryW( L"ucrtbase.dll" );
			malloc = (decltype(malloc))GetProcAddress( ucrt, "_aligned_malloc" );
			free = (decltype(free))GetProcAddress( ucrt, "_aligned_free" );
			realloc = (decltype(realloc))GetProcAddress( ucrt, "_aligned_realloc" );
		}

		void *Alloc( size_t nSize ) override { return malloc( nSize, 16 ); }
		void *Realloc( void *pMem, size_t nSize ) override { return realloc( pMem, nSize, 16 ); }
		void Free( void *pMem ) override { free( pMem ); }
		void *AllocAlign( size_t nSize, size_t align ) override { return malloc( nSize, align ); }
		void *ReallocAlign( void *pMem, size_t nSize, size_t align ) override { return realloc( pMem, nSize, align ); }
		void Expand( void *, size_t ) override {}
		void Alloc( size_t, const char *, int ) override {}
		void Realloc( void *, size_t, const char *, int ) override {}
		void Free( void *, const char *, int ) override {}
		void Expand( void *, size_t, const char *, int ) override {}
		void AllocAlign(size_t, size_t, const char*, int) override {}

		void *( *malloc )( size_t, size_t );
		void ( *free )( void * );
		void *( *realloc )( void *, size_t, size_t );
	} stub;
	IMemAlloc* alloc;
} s_init;

#ifndef __THROW
#define __THROW
#endif

void *__cdecl operator new( size_t nSize )
{
	return s_init.alloc->Alloc( nSize );
}

void *__cdecl operator new[]( size_t nSize )
{
	return s_init.alloc->Alloc( nSize );
}

void __cdecl operator delete( void *pMem ) noexcept
{
	s_init.alloc->Free( pMem );
}

void __cdecl operator delete[]( void *pMem ) noexcept
{
	s_init.alloc->Free( pMem );
}
extern "C"
{
void *malloc( size_t nSize ) __THROW
{
	return s_init.alloc->Alloc( nSize );
}

void free( void *pMem ) __THROW
{
	s_init.alloc->Free( pMem );
}

void *realloc( void *pMem, size_t nSize ) __THROW
{
	return s_init.alloc->Realloc( pMem, nSize );
}

void *calloc( size_t nCount, size_t nElementSize ) __THROW
{
	void *pMem = s_init.alloc->Alloc( nElementSize * nCount );
	memset( pMem, 0, nElementSize * nCount );
	return pMem;
}

_CRTRESTRICT
void *__cdecl _malloc_base( size_t nSize )
{
	return s_init.alloc->Alloc( nSize );
}

_CRTRESTRICT
void *_calloc_base( size_t nCount, size_t nSize )
{
	if ( !s_init.alloc ) [[unlikely]]
		s_init.Init();
	void *pMem = s_init.alloc->Alloc( nCount * nSize );
	memset( pMem, 0, nCount * nSize );
	return pMem;
}

_CRTRESTRICT
void *_realloc_base( void *pMem, size_t nSize )
{
	return s_init.alloc->Realloc( pMem, nSize );
}

void *_recalloc_base( void *pMem, size_t nCount, size_t nSize )
{
	return _recalloc( pMem, nCount, nSize );
}

void _free_base( void *pMem )
{
	s_init.alloc->Free( pMem );
}

void *__cdecl _expand_base( void *pMem, size_t nNewSize, int nBlockUse )
{
	abort();
}

void *__cdecl _recalloc( void *memblock, size_t count, size_t size )
{
	void *pMem = s_init.alloc->Realloc( memblock, size * count );
	if ( !memblock )
		memset( pMem, 0, size * count );
	return pMem;
}

void * __cdecl _aligned_malloc( size_t size, size_t align )
{
	return s_init.alloc->AllocAlign( size, align );
}

void *__cdecl _aligned_realloc( void *memblock, size_t size, size_t align )
{
    return s_init.alloc->ReallocAlign( memblock, size, align );
}

_CRTRESTRICT
void * __cdecl _aligned_recalloc( void * memblock, size_t count, size_t size, size_t align )
{
	abort();
}

_CRTRESTRICT
void * __cdecl _aligned_offset_malloc( size_t size, size_t align, size_t offset )
{
	return nullptr;
}

_CRTRESTRICT
void * __cdecl _aligned_offset_realloc( void * memblock, size_t size, size_t align, size_t offset)
{
	return nullptr;
}

_CRTRESTRICT
void * __cdecl _aligned_offset_recalloc( void * memblock, size_t count, size_t size, size_t align, size_t offset)
{
	return nullptr;
}

void __cdecl _aligned_free( void *memblock )
{
    s_init.alloc->Free( memblock );
}

void *__cdecl _expand( void *pMem, size_t nSize )
{
	abort();
}

int _heapchk()
{
	return -2;
}

int _heapmin()
{
	return 1;
}

int __cdecl _heapwalk( _HEAPINFO * )
{
	return 0;
}

size_t _msize( void *pMem )
{
	abort();
}

size_t _aligned_msize( void *pMem, size_t alignment, size_t offset )
{
	abort();
}

int _query_new_mode()
{
	return 0;
}

int _set_new_mode( int newhandlermode )
{
	return 0;
}
} // end extern "C"