// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "OneToOneBlock.hpp"
#include "Utility.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Format.h>

#include <arrayfire.h>

#include <cstdint>
#include <typeinfo>

//
// Test classes
//

template <typename T>
class CombineComplex: public ArrayFireBlock
{
    public:
        using ComplexType = std::complex<T>;

        CombineComplex(
            const std::string& device,
            size_t dtypeDimensions
        ):
            ArrayFireBlock(device)
        {
            this->setupInput(
                "re",
                Pothos::DType(typeid(T), dtypeDimensions));
            this->setupInput(
                "im",
                Pothos::DType(typeid(T), dtypeDimensions));

            this->setupOutput(
                0,
                Pothos::DType(typeid(ComplexType), dtypeDimensions));
        }

        virtual ~CombineComplex() = default;

        void work() override
        {
            const size_t elems = this->workInfo().minAllElements;
            if(0 == elems)
            {
                return;
            }

            auto afReal = this->getInputPortAsAfArray("re");
            auto afImag = this->getInputPortAsAfArray("im");
            
            this->produceFromAfArray(0, af::complex(afReal, afImag));
        }
};

template <typename T>
class SplitComplex: public ArrayFireBlock
{
    public:
        using ComplexType = std::complex<T>;

        SplitComplex(
            const std::string& device,
            size_t dtypeDimensions
        ):
            ArrayFireBlock(device)
        {
            this->setupInput(
                0,
                Pothos::DType(typeid(ComplexType), dtypeDimensions));

            this->setupOutput(
                "re",
                Pothos::DType(typeid(T), dtypeDimensions));
            this->setupOutput(
                "im",
                Pothos::DType(typeid(T), dtypeDimensions));
        }

        virtual ~SplitComplex() = default;

        void work() override
        {
            const size_t elems = this->workInfo().minAllElements;
            if(0 == elems)
            {
                return;
            }

            auto afInput = this->getInputPortAsAfArray(0);
            this->produceFromAfArray("re", af::real(afInput));
            this->produceFromAfArray("im", af::imag(afInput));
        }
};

//
// Factories
//

static Pothos::Block* combineComplexFactory(
    const std::string& device,
    const Pothos::DType& dtype)
{
    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
            return new CombineComplex<T>(device,dtype.dimension());

    ifTypeDeclareFactory(float)
    ifTypeDeclareFactory(double)

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
    #undef ifTypeDeclareFactory
}

static Pothos::Block* splitComplexFactory(
    const std::string& device,
    const Pothos::DType& dtype)
{
    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
            return new SplitComplex<T>(device,dtype.dimension());

    ifTypeDeclareFactory(float)
    ifTypeDeclareFactory(double)

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
    #undef ifTypeDeclareFactory
}

//
// Block registries
//

/*
 * |PothosDoc Combine Complex
 *
 * Calls <b>af::complex</b> on the inputs of the <b>"re"</b> and <b>"im"</b> ports
 * and outputs the combined results.
 *
 * |category /ArrayFire/Convert
 * |keywords arith complex real imag imaginary
 * |factory /arrayfire/arith/combine_complex(device,dtype)
 *
 * |param device[Device] ArrayFire device to use.
 * |default "Auto"
 *
 * |param dtype[Data Type] The block data type. The output type will be the complex form of this type.
 * |widget DTypeChooser(float=1,dim=1)
 * |default "float64"
 * |preview disable
 */
static Pothos::BlockRegistry registerCombineComplex(
    "/arrayfire/arith/combine_complex",
    Pothos::Callable(&combineComplexFactory));

/*
 * |PothosDoc Split Complex
 *
 * Calls <b>af::real</b> and <b>af::imag</b> on all inputs and outputs results
 * in "re" and "im" output channels.
 *
 * |category /ArrayFire/Convert
 * |keywords arith complex real imag imaginary
 * |factory /arrayfire/arith/split_complex(device,dtype)
 *
 * |param device[Device] ArrayFire device to use.
 * |default "Auto"
 *
 * |param dtype[Data Type] The block data type. The input type will be the complex form of this type.
 * |widget DTypeChooser(float=1,dim=1)
 * |default "float64"
 * |preview disable
 */
static Pothos::BlockRegistry registerSplitComplex(
    "/arrayfire/arith/split_complex",
    Pothos::Callable(&splitComplexFactory));