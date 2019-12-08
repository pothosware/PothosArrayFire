// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "BlockExecutionTests.hpp"
#include "TestUtility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>

/*template <typename In, typename Out>
static void testOneToOneBlockCommon(
    const Pothos::Proxy& block,
    const UnaryFunc<In, Out>& verificationFunc)
{
    static const Pothos::DType inputDType(typeid(In));
    static const Pothos::DType outputDType(typeid(Out));

    const size_t numChannels = block.call<InputPortVector>("inputs").size();

    std::vector<std::vector<In>> testInputs(numChannels);
    std::vector<Pothos::Proxy> feederSources;
    std::vector<Pothos::Proxy> collectorSinks;

    for(size_t chan = 0; chan < numChannels; ++chan)
    {
        testInputs[chan] = getTestInputs<In>();

        feederSources.emplace_back(
            Pothos::BlockRegistry::make(
                "/blocks/feeder_source",
                inputDType));
        feederSources.back().call(
            "feedBuffer",
            stdVectorToBufferChunk<In>(
                inputDType,
                testInputs[chan]));

        collectorSinks.emplace_back(
            Pothos::BlockRegistry::make(
                "/blocks/collector_sink",
                outputDType));
    }

    // Execute the topology.
    {
        Pothos::Topology topology;
        for(size_t chan = 0; chan < numChannels; ++chan)
        {
            topology.connect(
                feederSources[chan],
                0,
                block,
                chan);
            topology.connect(
                block,
                chan,
                collectorSinks[chan],
                0);
        }

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.05));
    }

    // Make sure the blocks output data and, if the caller provided a
    // verification function, that the outputs are valid.
    for(size_t chan = 0; chan < numChannels; ++chan)
    {
        const auto& chanInputs = testInputs[chan];
        const size_t numInputs = chanInputs.size();

        auto chanOutputs = collectorSinks[chan].call<Pothos::BufferChunk>("getBuffer");
        POTHOS_TEST_EQUAL(
            numInputs,
            chanOutputs.elements());
        if(nullptr != verificationFunc)
        {
            std::vector<Out> expectedOutputs;
            std::transform(
                chanInputs.begin(),
                chanInputs.end(),
                std::back_inserter(expectedOutputs),
                verificationFunc);

            testBufferChunk<Out>(
                chanOutputs,
                expectedOutputs);
        }
    }
}*/

static void testCastBlock(
    const std::string& type1,
    const std::string& type2,
    size_t numChannels)
{
    static constexpr const char* blockRegistryPath = "/arrayfire/stream/cast";

    std::cout << "Testing " << blockRegistryPath
              << " (types: " << type1 << " -> " << type2
              << ", chans: " << numChannels << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     type1,
                     type2,
                     numChannels);
}

void testCastBlockForType(const std::string& inputType)
{
    static const std::vector<std::string> AllTypes =
    {
        // ArrayFire doesn't support int8
        "int16",
        "int32",
        "int64",
        "uint8",
        "uint16",
        "uint32",
        "uint64",
        "float32",
        "float64",
        "complex_float32",
        "complex_float64"
    };
    for(const auto& outputType: AllTypes)
    {
        testCastBlock(inputType, outputType, 1);
        testCastBlock(inputType, outputType, 3);
    }
}

/*
template <typename T>
void testOneToOneBlock(
    const std::string& blockRegistryPath,
    size_t numChannels,
    const UnaryFunc<T, T>& verificationFunc)
{
    static const Pothos::DType dtype(typeid(T));

    std::cout << "Testing " << blockRegistryPath << " (type: " << dtype.name()
                            << ", " << "chans: " << numChannels << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     dtype,
                     numChannels);
    auto inputs = block.call<InputPortVector>("inputs");
    auto outputs = block.call<OutputPortVector>("outputs");
    POTHOS_TEST_EQUAL(numChannels, inputs.size());
    POTHOS_TEST_EQUAL(numChannels, outputs.size());

    testOneToOneBlockCommon<T, T>(
        block,
        verificationFunc);
}

template <typename In, typename Out>
void testOneToOneBlock(
    const std::string& blockRegistryPath,
    size_t numChannels,
    const UnaryFunc<In, Out>& verificationFunc)
{
    static const Pothos::DType inputDType(typeid(In));
    static const Pothos::DType outputDType(typeid(Out));

    std::cout << "Testing " << blockRegistryPath
                            << " (types: " << inputDType.name() << " -> " << outputDType.name()
                            << ", " << "chans: " << numChannels << ")" << std::endl;

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     inputDType,
                     outputDType,
                     numChannels);
    auto inputs = block.call<InputPortVector>("inputs");
    auto outputs = block.call<OutputPortVector>("outputs");
    POTHOS_TEST_EQUAL(numChannels, inputs.size());
    POTHOS_TEST_EQUAL(numChannels, outputs.size());

    testOneToOneBlockCommon<In, Out>(
        block,
        verificationFunc);
}

template <typename T>
void testScalarOpBlock(
    const std::string& blockRegistryPath,
    size_t numChannels,
    const BinaryFunc<T, T>& verificationFunc,
    bool allowZeroScalar)
{
    static const Pothos::DType dtype(typeid(T));
    static const T zero(0);

    std::cout << "Testing " << blockRegistryPath << " (type: " << dtype.name()
                            << ", " << "chans: " << numChannels << ")" << std::endl;

    T scalar;
    do
    {
        scalar = getSingleTestInput<T>();
    } while(!allowZeroScalar && (scalar == zero));

    auto block = Pothos::BlockRegistry::make(
                     blockRegistryPath,
                     dtype,
                     scalar,
                     numChannels);
    testEqual(scalar, block.template call<T>("getScalar"));

    // Test explicit getter+setter.
    block.template call("setScalar", scalar);
    testEqual(scalar, block.template call<T>("getScalar"));

    testOneToOneBlockCommon<T, T>(
        block,
        binaryFuncToUnary(verificationFunc, scalar));
}

#define SPECIALIZE_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlock<T>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<T, T>& verificationFunc); \
    template \
    void testScalarOpBlock<T>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const BinaryFunc<T, T>& verificationFunc, \
        bool allowZeroScalar);

#define SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(T) \
    template \
    void testOneToOneBlock<T, std::complex<T>>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<T, std::complex<T>>& verificationFunc); \
    template \
    void testOneToOneBlock<std::complex<T>, T>( \
        const std::string& blockRegistryPath, \
        size_t numChannels, \
        const UnaryFunc<std::complex<T>, T>& verificationFunc);

SPECIALIZE_TEMPLATE_TEST(std::int8_t)
SPECIALIZE_TEMPLATE_TEST(std::int16_t)
SPECIALIZE_TEMPLATE_TEST(std::int32_t)
SPECIALIZE_TEMPLATE_TEST(std::int64_t)
SPECIALIZE_TEMPLATE_TEST(std::uint8_t)
SPECIALIZE_TEMPLATE_TEST(std::uint16_t)
SPECIALIZE_TEMPLATE_TEST(std::uint32_t)
SPECIALIZE_TEMPLATE_TEST(std::uint64_t)
SPECIALIZE_TEMPLATE_TEST(float)
SPECIALIZE_TEMPLATE_TEST(double)
SPECIALIZE_TEMPLATE_TEST(std::complex<float>)
SPECIALIZE_TEMPLATE_TEST(std::complex<double>)

SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(float)
SPECIALIZE_COMPLEX_1TO1_TEMPLATE_TEST(double)
*/
