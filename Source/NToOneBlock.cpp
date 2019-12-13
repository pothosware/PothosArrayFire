// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "NToOneBlock.hpp"
#include "Utility.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <arrayfire.h>

#include <cassert>
#include <string>
#include <typeinfo>


//
// Factories
//

Pothos::Block* NToOneBlock::make(
    const NToOneFunc& func,
    const Pothos::DType& dtype,
    size_t numChannels,
    const DTypeSupport& supportedTypes)
{
    validateDType(dtype, supportedTypes);

    return new NToOneBlock(
                   func,
                   dtype,
                   numChannels);
}

//
// Class implementation
//

NToOneBlock::NToOneBlock(
    const NToOneFunc& func,
    const Pothos::DType& dtype,
    size_t numChannels
): ArrayFireBlock(),
   _func(func),
   _nchans(0)
{
    if(numChannels < 2)
    {
        throw Pothos::InvalidArgumentException("numChannels must be >= 2.");
    }
    _nchans = numChannels;

    for(size_t chan = 0; chan < _nchans; ++chan)
    {
        this->setupInput(chan, dtype);
    }
    this->setupOutput(0, dtype);
}

NToOneBlock::~NToOneBlock() {}

// TODO: ArrayFire likely has a way to optimize this
void NToOneBlock::work()
{
    const size_t elems = this->workInfo().minAllElements;

    if(0 == elems)
    {
        return;
    }

    auto afArray = this->getNumberedInputPortsAs2DAfArray();
    af::array outputAfArray(afArray.row(0));

    for(size_t chan = 1; chan < _nchans; ++chan)
    {
        outputAfArray = _func(outputAfArray, afArray.row(chan));
    }
    this->postAfArray(0, outputAfArray);
}
