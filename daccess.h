#ifndef __DACCESS_H_MINI_
#define __DACCESS_H_MINI_

#include <stdint.h>
#include <type_traits>

typedef void *PVOID;
typedef uint32_t ULONG32;

typedef intptr_t TADDR;

extern "C"
{
PVOID   DacInstantiateTypeByAddress(TADDR addr, ULONG32 size, bool throwEx);
}

// Base pointer wrapper which provides common behavior.
class __TPtrBase
{
public:
    __TPtrBase()
    {
        // Make uninitialized pointers obvious.
        m_addr = (TADDR)-1;
    }
    __TPtrBase(TADDR addr)
    {
        m_addr = addr;
    }

    // We use this delayed check to avoid ambiguous overload issues with TADDR
    // on platforms where NULL is defined as anything other than a uintptr_t constant
    // or nullptr_t exactly.
    // Without this, any valid "null pointer constant" that is not directly either type
    // will be implicitly convertible to both TADDR and std::nullptr_t, causing ambiguity.
    // With this, this constructor (and all similarly declared operators) drop out of
    // consideration when used with NULL (and not nullptr_t).
    // With this workaround, we get identical behavior between the DAC and non-DAC builds for assigning NULL
    // to DACized pointer types.
    template<typename T, typename = typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type>
    __TPtrBase(T)
    {
        m_addr = 0;
    }

    __TPtrBase& operator=(TADDR addr)
    {
        m_addr = addr;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type>
    __TPtrBase& operator=(T)
    {
        m_addr = 0;
        return *this;
    }

    bool operator!() const
    {
        return m_addr == 0;
    }

    // We'd like to have an explicit conversion to bool here since the C++
    // standard says all pointer types are implicitly converted to bool.
    // Unfortunately, that would cause ambiguous overload errors for uses
    // of operator== and operator!= with NULL on MSVC (where NULL is a 32-bit int on all platforms).
    // Instead callers will have to compare directly against NULL.

    bool operator==(TADDR addr) const
    {
        return m_addr == addr;
    }
    bool operator!=(TADDR addr) const
    {
        return m_addr != addr;
    }

    template<typename T, typename = typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type>
    bool operator==(T) const
    {
        return m_addr == 0;
    }

    template<typename T, typename = typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type>
    bool operator!=(T) const
    {
        return m_addr != 0;
    }
    bool operator<(TADDR addr) const
    {
        return m_addr < addr;
    }
    bool operator>(TADDR addr) const
    {
        return m_addr > addr;
    }
    bool operator<=(TADDR addr) const
    {
        return m_addr <= addr;
    }
    bool operator>=(TADDR addr) const
    {
        return m_addr >= addr;
    }

    TADDR GetAddr(void) const
    {
        return m_addr;
    }
    TADDR SetAddr(TADDR addr)
    {
        m_addr = addr;
        return addr;
    }

protected:
    TADDR m_addr;
};

// Pointer wrapper base class for various forms of normal data.
// This has the common functionality between __DPtr and __ArrayDPtr.
// The DPtrType type parameter is the actual derived type in use.  This is necessary so that
// inhereted functions preserve exact return types.
template<typename type, template<typename> class DPtrTemplate>
class __DPtrBase : public __TPtrBase
{
public:
    typedef type _Type;
    typedef type* _Ptr;
    using DPtrType = DPtrTemplate<type>;

    using __TPtrBase::__TPtrBase;

    __DPtrBase() = default;

    explicit __DPtrBase(__TPtrBase ptr) : __TPtrBase(ptr.GetAddr()) {}
    
    // construct const from non-const
    __DPtrBase(__DPtrBase<typename std::remove_const<type>::type, DPtrTemplate> const & rhs) : __DPtrBase(rhs.GetAddr()) {}

    explicit __DPtrBase(type const * host)
    {
        m_addr = DacGetTargetAddrForHostAddr(host, true);
    }

public:
    using __TPtrBase::operator=;

    DPtrType& operator=(const __TPtrBase& ptr)
    {
        m_addr = ptr.GetAddr();
        return DPtrType(m_addr);
    }

    type& operator*(void) const
    {
        return *(type*)DacInstantiateTypeByAddress(m_addr, sizeof(type), true);
    }

    using __TPtrBase::operator==;
    using __TPtrBase::operator!=;

    bool operator==(const DPtrType& ptr) const
    {
        return m_addr == ptr.GetAddr();
    }

    bool operator!=(const DPtrType& ptr) const
    {
        return !operator==(ptr);
    }

    bool operator<(const DPtrType& ptr) const
    {
        return m_addr < ptr.GetAddr();
    }
    bool operator>(const DPtrType& ptr) const
    {
        return m_addr > ptr.GetAddr();
    }
    bool operator<=(const DPtrType& ptr) const
    {
        return m_addr <= ptr.GetAddr();
    }
    bool operator>=(const DPtrType& ptr) const
    {
        return m_addr >= ptr.GetAddr();
    }

    // Array index operator
    // we want an operator[] for all possible numeric types (rather than rely on
    // implicit numeric conversions on the argument) to prevent ambiguity with
    // DPtr's implicit conversion to type* and the built-in operator[].
    // @dbgtodo : we could also use this technique to simplify other operators below.
    template<typename indexType>
    type& operator[](indexType index)
    {
        // Compute the address of the element.
        TADDR elementAddr;
        if( index >= 0 )
        {
            elementAddr = DacTAddrOffset(m_addr, index, sizeof(type));
        }
        else
        {
            // Don't bother trying to do overflow checking for negative indexes - they are rare compared to
            // positive ones.  ClrSafeInt doesn't support signed datatypes yet (although we should be able to add it
            // pretty easily).
            elementAddr = m_addr + index * sizeof(type);
        }

        // Marshal over a single instance and return a reference to it.
        return *(type*) DacInstantiateTypeByAddress(elementAddr, sizeof(type), true);
    }

    template<typename indexType>
    type const & operator[](indexType index) const
    {
        return (*const_cast<__DPtrBase*>(this))[index];
    }

    //-------------------------------------------------------------------------
    // operator+

    DPtrType operator+(unsigned short val)
    {
        return DPtrType(DacTAddrOffset(m_addr, val, sizeof(type)));
    }
#if defined(HOST_UNIX) && defined(HOST_64BIT)
    DPtrType operator+(unsigned long long val)
    {
        return DPtrType(DacTAddrOffset(m_addr, val, sizeof(type)));
    }
#endif // HOST_UNIX && HOST_BIT64
    DPtrType operator+(short val)
    {
        return DPtrType(m_addr + val * sizeof(type));
    }
    // size_t is unsigned int on Win32, so we need
    // to ifdef here to make sure the unsigned int
    // and size_t overloads don't collide.  size_t
    // is marked __w64 so a simple unsigned int
    // will not work on Win32, it has to be size_t.
    DPtrType operator+(size_t val)
    {
        return DPtrType(DacTAddrOffset(m_addr, val, sizeof(type)));
    }
#if defined (HOST_64BIT)
    DPtrType operator+(unsigned int val)
    {
        return DPtrType(DacTAddrOffset(m_addr, val, sizeof(type)));
    }
#endif
    DPtrType operator+(int val)
    {
        return DPtrType(m_addr + val * sizeof(type));
    }
    // Because of the size difference between long and int on non MS compilers,
    // we only need to define these operators on Windows. These provide compatible
    // overloads for DWORD addition operations.
#ifdef _MSC_VER
    DPtrType operator+(unsigned long val)
    {
        return DPtrType(DacTAddrOffset(m_addr, val, sizeof(type)));
    }
    DPtrType operator+(long val)
    {
        return DPtrType(m_addr + val * sizeof(type));
    }
#endif

    //-------------------------------------------------------------------------
    // operator-

    DPtrType operator-(unsigned short val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
    DPtrType operator-(short val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
    // size_t is unsigned int on Win32, so we need
    // to ifdef here to make sure the unsigned int
    // and size_t overloads don't collide.  size_t
    // is marked __w64 so a simple unsigned int
    // will not work on Win32, it has to be size_t.
    DPtrType operator-(size_t val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
#ifdef HOST_64BIT
    DPtrType operator-(unsigned int val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
#endif
    DPtrType operator-(int val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
    // Because of the size difference between long and int on non MS compilers,
    // we only need to define these operators on Windows. These provide compatible
    // overloads for DWORD addition operations.
#ifdef _MSC_VER // for now, everything else is 32 bit
    DPtrType operator-(unsigned long val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
    DPtrType operator-(long val)
    {
        return DPtrType(m_addr - val * sizeof(type));
    }
#endif
    size_t operator-(const DPtrType& val)
    {
        return (m_addr - val.m_addr) / sizeof(type);
    }

    //-------------------------------------------------------------------------

    DPtrType& operator+=(size_t val)
    {
        m_addr += val * sizeof(type);
        return static_cast<DPtrType&>(*this);
    }
    DPtrType& operator-=(size_t val)
    {
        m_addr -= val * sizeof(type);
        return static_cast<DPtrType&>(*this);
    }

    DPtrType& operator++()
    {
        m_addr += sizeof(type);
        return static_cast<DPtrType&>(*this);
    }
    DPtrType& operator--()
    {
        m_addr -= sizeof(type);
        return static_cast<DPtrType&>(*this);
    }
    DPtrType operator++(int postfix)
    {
        DPtrType orig = DPtrType(*this);
        m_addr += sizeof(type);
        return orig;
    }
    DPtrType operator--(int postfix)
    {
        DPtrType orig = DPtrType(*this);
        m_addr -= sizeof(type);
        return orig;
    }

    bool IsValid(void) const
    {
        return m_addr &&
            DacInstantiateTypeByAddress(m_addr, sizeof(type),
                                        false) != NULL;
    }
    void EnumMem(void) const
    {
        DacEnumMemoryRegion(m_addr, sizeof(type));
    }
};

// forward declaration
template<typename acc_type, typename store_type>
class __GlobalPtr;

// Pointer wrapper for objects which are just plain data
// and need no special handling.
template<typename type>
class __DPtr : public __DPtrBase<type,__DPtr>
{
public:
    using __DPtrBase<type,__DPtr>::__DPtrBase;

    __DPtr() = default;

    // construct from GlobalPtr
    explicit __DPtr(__GlobalPtr< type*, __DPtr< type > > globalPtr) :
        __DPtrBase<type,__DPtr>(globalPtr.GetAddr()) {}

    operator type*() const
    {
        return (type*)DacInstantiateTypeByAddress(this->m_addr, sizeof(type), true);
    }

    type* operator->() const
    {
        return (type*)(*this);
    }
};

#define DPTR(type) __DPtr< type >

#endif
