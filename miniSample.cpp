#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "daccess.h"

struct EEClass
{
    
};

struct MethodTable
{
    explicit MethodTable();
    MethodTable(const MethodTable&) = delete;
    MethodTable& operator=(const MethodTable& ) = delete;

    intptr_t dummy_member;

    union
    {
	DPTR(EEClass) m_pEEClass;
	TADDR m_pCanonMT;
    };
};

typedef DPTR(MethodTable) PTR_MethodTable;
typedef DPTR(EEClass) PTR_EEClass;

extern "C"
{
    PVOID   DacInstantiateTypeByAddress(TADDR addr, ULONG32 size, bool throwEx)
    {
	if (addr == 0 || addr == -1)
	    return (PVOID)addr;
	else
	    return malloc(size);
    }

}

int
main()
{
    TADDR canonicalMethodTable = 0;
    
#if 1
    // original code
    PTR_EEClass h = PTR_EEClass(PTR_MethodTable(canonicalMethodTable)->m_pCanonMT);
#else
    // new code
    PTR_EEClass h = PTR_MethodTable(canonicalMethodTable)->m_pEEClass;
#endif

    printf ("EEClass target addr is 0x%" PRIxPTR "\n", h.GetAddr());
}
