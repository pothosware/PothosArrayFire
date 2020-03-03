// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include <arrayfire.h>

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#include "BlockExecutionTests.hpp"
#include "TestUtility.hpp"
#include "Utility.hpp"

#include <Poco/Thread.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>

static constexpr const char* combineRegistryPath = "/arrayfire/arith/combine_complex";
static constexpr const char* splitRegistryPath   = "/arrayfire/arith/split_complex";
static constexpr int sleepTimeMs = 500;

static void testCombineToSplit(const std::string& type)
{
    std::cout << "Testing " << combineRegistryPath << " -> " << splitRegistryPath
              << " (type: " << type << ")" << std::endl;

    auto realTestInputs = PothosArrayFireTests::getTestInputs(type);
    auto imagTestInputs = PothosArrayFireTests::getTestInputs(type);

    auto realFeederSource = Pothos::BlockRegistry::make(
                                "/blocks/feeder_source",
                                type);
    realFeederSource.call("feedBuffer", realTestInputs);

    auto imagFeederSource = Pothos::BlockRegistry::make(
                                "/blocks/feeder_source",
                                type);
    imagFeederSource.call("feedBuffer", imagTestInputs);

    auto combineComplex = Pothos::BlockRegistry::make(
                              combineRegistryPath,
                              "Auto",
                              type);
    auto splitComplex = Pothos::BlockRegistry::make(
                            splitRegistryPath,
                            "Auto",
                            type);

    auto realCollectorSink = Pothos::BlockRegistry::make(
                                 "/blocks/collector_sink",
                                 type);
    auto imagCollectorSink = Pothos::BlockRegistry::make(
                                 "/blocks/collector_sink",
                                 type);

    {
        Pothos::Topology topology;

        topology.connect(
            realFeederSource, 0,
            combineComplex, "re");
        topology.connect(
            imagFeederSource, 0,
            combineComplex, "im");

        topology.connect(
            combineComplex, 0,
            splitComplex, 0);

        topology.connect(
            splitComplex, "re",
            realCollectorSink, 0);
        topology.connect(
            splitComplex, "im",
            imagCollectorSink, 0);

        topology.commit();
        Poco::Thread::sleep(sleepTimeMs);
        POTHOS_TEST_TRUE(topology.waitInactive());
    }
    POTHOS_TEST_TRUE(realCollectorSink.call("getBuffer").call<size_t>("elements") > 0);
    POTHOS_TEST_TRUE(imagCollectorSink.call("getBuffer").call<size_t>("elements") > 0);

    PothosArrayFireTests::testBufferChunk(
        realTestInputs,
        realCollectorSink.call("getBuffer"));
    PothosArrayFireTests::testBufferChunk(
        imagTestInputs,
        imagCollectorSink.call("getBuffer"));
}

static void testSplitToCombine(const std::string& type)
{
    std::cout << "Testing " << splitRegistryPath << " -> " << combineRegistryPath
              << " (type: " << type << ")" << std::endl;

    const auto complexType = "complex_"+type;

    auto testInputs = PothosArrayFireTests::getTestInputs(complexType);

    auto feederSource = Pothos::BlockRegistry::make(
                            "/blocks/feeder_source",
                            complexType);
    feederSource.call("feedBuffer", testInputs);

    auto splitComplex = Pothos::BlockRegistry::make(
                            splitRegistryPath,
                            "Auto",
                            type);
    auto combineComplex = Pothos::BlockRegistry::make(
                              combineRegistryPath,
                              "Auto",
                              type);

    auto collectorSink = Pothos::BlockRegistry::make(
                             "/blocks/collector_sink",
                             complexType);

    {
        Pothos::Topology topology;

        topology.connect(
            feederSource, 0,
            splitComplex, 0);

        topology.connect(
            splitComplex, "re",
            combineComplex, "re");
        topology.connect(
            splitComplex, "im",
            combineComplex, "im");

        topology.connect(
            combineComplex, 0,
            collectorSink, 0);

        topology.commit();
        Poco::Thread::sleep(sleepTimeMs);
        POTHOS_TEST_TRUE(topology.waitInactive());
    }
    POTHOS_TEST_TRUE(collectorSink.call("getBuffer").call<size_t>("elements") > 0);

    PothosArrayFireTests::testBufferChunk(
        testInputs,
        collectorSink.call("getBuffer"));
}

POTHOS_TEST_BLOCK("/arrayfire/tests", test_complex_blocks)
{
    PothosArrayFireTests::setupTestEnv();

    const std::vector<std::string> dtypes = {"float32","float64"};
    
    for(const auto& type: dtypes)
    {
        testCombineToSplit(type);
        testSplitToCombine(type);
    }
}