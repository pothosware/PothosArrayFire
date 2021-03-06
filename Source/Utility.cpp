// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "DeviceCache.hpp"
#include "Utility.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Format.h>
#include <Poco/String.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

// TODO: take in string to give context on what block is unsupported
void validateDType(
    const Pothos::DType& dtype,
    const DTypeSupport& supportedTypes)
{
    // Make sure *something* is supported.
    assert(supportedTypes.supportInt ||
           supportedTypes.supportUInt ||
           supportedTypes.supportFloat ||
           supportedTypes.supportComplexFloat);

    // Specific error for types not supported by any block
    static const std::vector<std::string> globalUnsupportedTypes =
    {
        "complex_int8",
        "complex_int16",
        "complex_int32",
        "complex_int64",
        "complex_uint8",
        "complex_uint16",
        "complex_uint32",
        "complex_uint64",
    };
    if(doesVectorContainValue(globalUnsupportedTypes, dtype.name()))
    {
        throw Pothos::InvalidArgumentException(
                  "PothosGPU blocks do not support this type",
                  dtype.name());
    }

    const bool isDTypeSupported = (isDTypeInt(dtype) && supportedTypes.supportInt) ||
                                  (isDTypeUInt(dtype) && supportedTypes.supportUInt) ||
                                  (isDTypeFloat(dtype) && supportedTypes.supportFloat) ||
                                  (isDTypeComplexFloat(dtype) && supportedTypes.supportComplexFloat);

    if(!isDTypeSupported)
    {
        throw Pothos::InvalidArgumentException(
                  "Unsupported type",
                  dtype.name());
    }
}

bool isSupportedFileSinkType(const Pothos::DType& dtype)
{
    return (dtype.name().find("int32") == std::string::npos) &&
           (dtype.name().find("int64") == std::string::npos);
}

Pothos::Object getArrayValueOfUnknownTypeAtIndex(
    const af::array& afArray,
    dim_t index)
{
    const auto& arrIndex = afArray(index);
    Pothos::Object ret;

    assert(arrIndex.elements() == 1);

    #define SwitchCase(afDType, ctype) \
        case afDType: \
            ret = Pothos::Object(arrIndex.scalar<PothosToAF<ctype>::type>()); \
            break;

    switch(afArray.type())
    {
        SwitchCase(::b8,  char)
        SwitchCase(::s16, short)
        SwitchCase(::s32, int)
        SwitchCase(::s64, long long)
        SwitchCase(::u8,  unsigned char)
        SwitchCase(::u16, unsigned short)
        SwitchCase(::u32, unsigned)
        SwitchCase(::u64, unsigned long long)
        SwitchCase(::f32, float)
        SwitchCase(::f64, double)
        SwitchCase(::c32, std::complex<float>)
        SwitchCase(::c64, std::complex<double>)

        default:
            throw Pothos::AssertionViolationException("Invalid dtype");
            break;
    }
    #undef SwitchCase

    return ret;
}

ssize_t findValueOfUnknownTypeInArray(
    const af::array& afArray,
    const Pothos::Object& value)
{
    #define SwitchCase(afDType, ctype) \
        case afDType: \
        { \
            const size_t size = static_cast<size_t>(afArray.elements()); \
            const ctype* buffer = reinterpret_cast<const ctype*>(afArray.host<PothosToAF<ctype>::type>()); \
            auto iter = std::find(buffer, (buffer+size), value.extract<ctype>()); \
            af::freeHost(buffer); \
            if(iter != (buffer+size)) \
            { \
                return static_cast<ssize_t>(std::distance(buffer, iter)); \
            } \
            break; \
        }

    switch(afArray.type())
    {
        SwitchCase(::b8,  char)
        SwitchCase(::s16, short)
        SwitchCase(::s32, int)
        SwitchCase(::s64, long long)
        SwitchCase(::u8,  unsigned char)
        SwitchCase(::u16, unsigned short)
        SwitchCase(::u32, unsigned)
        SwitchCase(::u64, unsigned long long)
        SwitchCase(::f32, float)
        SwitchCase(::f64, double)
        SwitchCase(::c32, std::complex<float>)
        SwitchCase(::c64, std::complex<double>)

        default:
            throw Pothos::AssertionViolationException("Invalid dtype");
            break;
    }
    #undef SwitchCase

    return -1;
}

af::array getArrayFromSingleElement(
    const af::array& afArray,
    size_t newArraySize)
{
    #define SwitchCase(afDType, ctype) \
        case afDType: \
        { \
            return af::constant(afArray.scalar<ctype>(), static_cast<dim_t>(newArraySize), afDType); \
        }

    switch(afArray.type())
    {
        SwitchCase(::b8,  char)
        SwitchCase(::s16, short)
        SwitchCase(::s32, int)
        SwitchCase(::s64, long long)
        SwitchCase(::u8,  unsigned char)
        SwitchCase(::u16, unsigned short)
        SwitchCase(::u32, unsigned)
        SwitchCase(::u64, unsigned long long)
        SwitchCase(::f32, float)
        SwitchCase(::f64, double)
        SwitchCase(::c32, af::cfloat)
        SwitchCase(::c64, af::cdouble)

        default:
            throw Pothos::AssertionViolationException("Invalid dtype");
            break;
    }
    #undef SwitchCase

    return af::array();
}

Pothos::Object afArrayToStdVector(const af::array& afArray)
{
    Pothos::Object ret;

    #define SwitchCase(afDType, ctype) \
        case afDType: \
        { \
            std::vector<ctype> vec(afArray.elements()); \
            afArray.host(vec.data()); \
            ret = Pothos::Object(vec); \
            break; \
        }

    switch(afArray.type())
    {
        SwitchCase(::b8,  char)
        SwitchCase(::s16, short)
        SwitchCase(::s32, int)
        SwitchCase(::s64, long long)
        SwitchCase(::u8,  unsigned char)
        SwitchCase(::u16, unsigned short)
        SwitchCase(::u32, unsigned)
        SwitchCase(::u64, unsigned long long)
        SwitchCase(::f32, float)
        SwitchCase(::f64, double)
        SwitchCase(::c32, std::complex<float>)
        SwitchCase(::c64, std::complex<double>)

        default:
            throw Pothos::AssertionViolationException("Invalid dtype");
            break;
    }

    return ret;
}

#if defined(__GNUG__) || defined(__clang__) || defined(_MSC_VER)

#if defined(__GNUG__) || defined(__clang__)
#include <cpuid.h>
#elif defined (_MSC_VER)
#include <intrin.h>
#endif

// Based on: https://github.com/culb/cpuid

static void regToString(
    uint32_t reg,
    std::ostringstream& sstr)
{
    auto* cstr = reinterpret_cast<const char*>(&reg);
    sstr << std::string(cstr, 4);
}

static void cpuIDToString(
    uint32_t leaf,
    std::ostringstream& sstr)
{
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

#if defined(__GNUG__) || defined(__clang__)
	__cpuid(leaf, eax, ebx, ecx, edx);
#elif defined(_MSC_VER)
	int32_t registers[4];
	__cpuid(registers, leaf);
	eax = registers[0];
	ebx = registers[1];
	ecx = registers[2];
	edx = registers[3];
#endif

    regToString(eax, sstr);
    regToString(ebx, sstr);
    regToString(ecx, sstr);
    regToString(edx, sstr);
}

bool isCPUIDSupported() {return true;}

static std::string cleanupProcessorName(const std::string& str)
{
    // Scanning the registers results in odd termination behavior,
    // but strlen gives us what we need.
    auto ret = str;
    ret.resize(strlen(ret.c_str()));

    return Poco::trim(ret);
}

std::string getProcessorName()
{
    std::ostringstream sstr;
    cpuIDToString(0x80000002, sstr);
    cpuIDToString(0x80000003, sstr);
    cpuIDToString(0x80000004, sstr);

    return cleanupProcessorName(sstr.str());
}

#else

bool isCPUIDSupported() {return false;}

std::string getProcessorName() {return "";}

#endif
