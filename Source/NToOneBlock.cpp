// Copyright (c) 2019-2020 Nicholas Corgan
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
    const std::string& device,
    const NToOneFunc& func,
    const Pothos::DType& dtype,
    size_t numChannels,
    const DTypeSupport& supportedTypes,
    bool shouldPostBuffer)
{
    validateDType(dtype, supportedTypes);

    return new NToOneBlock(
                   device,
                   func,
                   dtype,
                   numChannels,
                   shouldPostBuffer);
}

Pothos::Block* NToOneBlock::makeCallable(
    const std::string& device,
    const Pothos::Callable& func,
    const Pothos::DType& dtype,
    size_t numChannels,
    const DTypeSupport& supportedTypes,
    bool shouldPostBuffer)
{
    validateDType(dtype, supportedTypes);

    return new NToOneBlock(
                   device,
                   func,
                   dtype,
                   numChannels,
                   shouldPostBuffer);
}

//
// Class implementation
//

NToOneBlock::NToOneBlock(
    const std::string& device,
    const NToOneFunc& func,
    const Pothos::DType& dtype,
    size_t numChannels,
    bool shouldPostBuffer
): NToOneBlock(
       device,
       Pothos::Callable(func),
       dtype,
       numChannels,
       shouldPostBuffer)
{
}

NToOneBlock::NToOneBlock(
    const std::string& device,
    const Pothos::Callable& func,
    const Pothos::DType& dtype,
    size_t numChannels,
    bool shouldPostBuffer
): ArrayFireBlock(device),
   _func(func),
   _nchans(0),
    _postBuffer(shouldPostBuffer)
{
    if(numChannels < 2)
    {
        throw Pothos::InvalidArgumentException("numChannels must be >= 2.");
    }
    _nchans = numChannels;

    for(size_t chan = 0; chan < _nchans; ++chan)
    {
        this->setupInput(chan, dtype, _domain);
    }
    this->setupOutput(0, dtype, _domain);
}

NToOneBlock::~NToOneBlock() {}

void NToOneBlock::work()
{
    const size_t elems = this->workInfo().minAllElements;

    if(0 == elems)
    {
        return;
    }

    auto afArray = this->getInputPortAsAfArray(0);
    auto outputAfArray = afArray;

    for(size_t chan = 1; chan < _nchans; ++chan)
    {
        afArray = this->getInputPortAsAfArray(chan);
        outputAfArray = _func.call(outputAfArray, afArray).template extract<af::array>();
    }

    if(_postBuffer) this->postAfArray(0, outputAfArray);
    else            this->produceFromAfArray(0, outputAfArray);
}
