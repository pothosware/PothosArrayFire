// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "TestUtility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Testing.hpp>

#include <Poco/RandomStream.h>

#include <iostream>
#include <vector>

//
// Test implementations
//

template <typename T>
static void testLogicalArray()
{
    const Pothos::DType dtype(typeid(T));
    constexpr size_t numInputs = 3;

    std::cout << "Testing " << dtype.name() << "..." << std::endl;

    std::vector<Pothos::BufferChunk> inputs;
    for (size_t i = 0; i < numInputs; ++i) inputs.emplace_back(GPUTests::getTestInputs(dtype.name()));

    const auto bufferLen = inputs[0].elements();

    Pothos::BufferChunk expectedAndOutput("int8", bufferLen);
    Pothos::BufferChunk expectedOrOutput("int8", bufferLen);
    Pothos::BufferChunk expectedXOrOutput("int8", bufferLen); // TODO: test

    for (size_t elem = 0; elem < bufferLen; ++elem)
    {
        expectedAndOutput.template as<char*>()[elem] = (inputs[0].template as<T*>()[elem] &&
                                                               inputs[1].template as<T*>()[elem] &&
                                                               inputs[2].template as<T*>()[elem]) ? 1 : 0;
        expectedOrOutput.template as<char*>()[elem]  = (inputs[0].template as<T*>()[elem] ||
                                                               inputs[1].template as<T*>()[elem] ||
                                                               inputs[2].template as<T*>()[elem]) ? 1 : 0;
    }

    std::vector<Pothos::Proxy> sources(numInputs);
    for (size_t input = 0; input < numInputs; ++input)
    {
        sources[input] = Pothos::BlockRegistry::make("/blocks/feeder_source", dtype);
        sources[input].call("feedBuffer", inputs[input]);
    }

    auto andBlock = Pothos::BlockRegistry::make(
                        "/gpu/array/logical",
                        "Auto",
                        "And",
                        dtype,
                        numInputs);
    auto orBlock = Pothos::BlockRegistry::make(
                       "/gpu/array/logical",
                       "Auto",
                       "Or",
                       dtype,
                       numInputs);

    auto andSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "int8");
    auto orSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "int8");

    {
        Pothos::Topology topology;

        for (size_t input = 0; input < numInputs; ++input)
        {
            topology.connect(sources[input], 0, andBlock, input);
            topology.connect(sources[input], 0, orBlock, input);
        }

        topology.connect(andBlock, 0, andSink, 0);
        topology.connect(orBlock, 0, orSink, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    std::cout << " * Testing And..." << std::endl;
    GPUTests::testBufferChunk(
        expectedAndOutput,
        andSink.call<Pothos::BufferChunk>("getBuffer"));

    std::cout << " * Testing Or..." << std::endl;
    GPUTests::testBufferChunk(
        expectedOrOutput,
        orSink.call<Pothos::BufferChunk>("getBuffer"));
}

template <typename T>
static void testLogicalScalar()
{
    const Pothos::DType dtype(typeid(T));

    std::cout << "Testing " << dtype.name() << "..." << std::endl;

    auto input = GPUTests::getTestInputs(dtype.name());
    const auto bufferLen = input.elements();
    const auto scalar = GPUTests::getSingleTestInput(dtype.name()).convert<T>();

    Pothos::BufferChunk expectedAndOutput("int8", bufferLen);
    Pothos::BufferChunk expectedOrOutput("int8", bufferLen);

    for (size_t elem = 0; elem < bufferLen; ++elem)
    {
        expectedAndOutput.template as<char*>()[elem] = (input.template as<T*>()[elem] && scalar) ? 1 : 0;
        expectedOrOutput.template as<char*>()[elem]  = (input.template as<T*>()[elem] || scalar) ? 1 : 0;
    }

    auto source = Pothos::BlockRegistry::make("/blocks/feeder_source", dtype);
    source.call("feedBuffer", input);

    auto andBlock = Pothos::BlockRegistry::make(
                        "/gpu/scalar/logical",
                        "Auto",
                        "And",
                        dtype,
                        scalar);
    POTHOS_TEST_EQUAL(
        scalar,
        andBlock.template call<T>("scalar"));

    auto orBlock = Pothos::BlockRegistry::make(
                       "/gpu/scalar/logical",
                       "Auto",
                       "Or",
                       dtype,
                       scalar);
    POTHOS_TEST_EQUAL(
        scalar,
        orBlock.template call<T>("scalar"));

    auto andSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "int8");
    auto orSink = Pothos::BlockRegistry::make("/blocks/collector_sink", "int8");

    {
        Pothos::Topology topology;

        topology.connect(source, 0, andBlock, 0);
        topology.connect(source, 0, orBlock, 0);

        topology.connect(andBlock, 0, andSink, 0);
        topology.connect(orBlock, 0, orSink, 0);

        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive(0.01));
    }

    std::cout << " * Testing And..." << std::endl;
    GPUTests::testBufferChunk(
        expectedAndOutput,
        andSink.call<Pothos::BufferChunk>("getBuffer"));

    std::cout << " * Testing Or..." << std::endl;
    GPUTests::testBufferChunk(
        expectedOrOutput,
        orSink.call<Pothos::BufferChunk>("getBuffer"));
}

//
// Tests
//

POTHOS_TEST_BLOCK("/gpu/tests", test_array_logical)
{
    testLogicalArray<char>();
    testLogicalArray<short>();
    testLogicalArray<int>();
    testLogicalArray<long long>();
    testLogicalArray<unsigned char>();
    testLogicalArray<unsigned short>();
    testLogicalArray<unsigned>();
    testLogicalArray<unsigned long long>();
}

POTHOS_TEST_BLOCK("/gpu/tests", test_scalar_logical)
{
    testLogicalScalar<char>();
    testLogicalScalar<short>();
    testLogicalScalar<int>();
    testLogicalScalar<long long>();
    testLogicalScalar<unsigned char>();
    testLogicalScalar<unsigned short>();
    testLogicalScalar<unsigned>();
    testLogicalScalar<unsigned long long>();
}
