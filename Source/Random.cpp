// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "ArrayFireBlock.hpp"
#include "Utility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Timestamp.h>

#include <arrayfire.h>

#include <cassert>
#include <string>
#include <typeinfo>

#if AF_API_VERSION_CURRENT >= 34

using AfRandomFunc = af::array(*)(const af::dim4&, const af::dtype, af::randomEngine&);

class RandomBlock: public ArrayFireBlock
{
    public:

        using SeedType = unsigned long long;

        static Pothos::Block* make(
            const std::string& device,
            const Pothos::DType& dtype,
            const std::string& distribution)
        {
            return new RandomBlock(device, dtype, distribution);
        }

        RandomBlock(
            const std::string& device,
            const Pothos::DType& dtype,
            const std::string& distribution
        ):
            ArrayFireBlock(device),
            _afRandomFunc(nullptr), // Set in constructor
            _distribution(), // Set in constructor
            _afDType(Pothos::Object(dtype).convert<af::dtype>()),
            _afRandomEngine()
        {
            this->registerCall(this, POTHOS_FCN_TUPLE(RandomBlock, getDistribution));
            this->registerCall(this, POTHOS_FCN_TUPLE(RandomBlock, setDistribution));
            this->registerCall(this, POTHOS_FCN_TUPLE(RandomBlock, getRandomEngineType));
            this->registerCall(this, POTHOS_FCN_TUPLE(RandomBlock, setRandomEngineType));

            // This call is overloaded, so no macro for us.
            this->registerCall(
                this,
                "reseedRandomEngine",
                &RandomBlock::reseedRandomEngineWithTime);
            this->registerCall(
                this,
                "reseedRandomEngine",
                &RandomBlock::reseedRandomEngine);

            this->registerProbe("getDistribution");
            this->registerProbe("getRandomEngineType");

            this->registerSignal("distributionChanged");
            this->registerSignal("randomEngineTypeChanged");

            this->setupOutput(0, dtype, this->getPortDomain());

            this->setDistribution(distribution);
            this->reseedRandomEngineWithTime();
        }

        std::string getDistribution() const
        {
            return _distribution;
        }

        void setDistribution(const std::string& distribution)
        {
            if("UNIFORM" == distribution)
            {
                this->_afRandomFunc = &af::randu;
            }
            else if("NORMAL" == distribution)
            {
                this->_afRandomFunc = &af::randn;
            }
            else
            {
                throw Pothos::InvalidArgumentException(
                          "Invalid distribution",
                          distribution);
            }

            this->_distribution = distribution;
            this->emitSignal("distributionChanged", distribution);
        }

        std::string getRandomEngineType() const
        {
            return Pothos::Object(_afRandomEngine.getType()).convert<std::string>();
        }

        void setRandomEngineType(const std::string& randomEngineType)
        {
            _afRandomEngine.setType(Pothos::Object(randomEngineType).convert<af::randomEngineType>());
            this->emitSignal("randomEngineTypeChanged", randomEngineType);
        }

        void reseedRandomEngineWithTime()
        {
            // Seed this random engine with the current time.
            const auto seed = static_cast<SeedType>(Poco::Timestamp().epochMicroseconds());
            this->reseedRandomEngine(seed);
        }

        void reseedRandomEngine(SeedType seed)
        {
            _afRandomEngine.setSeed(seed);
        }

        void work() override
        {
            const auto elems = this->workInfo().minElements;
            if(0 == elems)
            {
                return;
            }

            const af::dim4 dims(static_cast<dim_t>(elems));

            auto afOutput = _afRandomFunc(dims, _afDType, _afRandomEngine);
            this->postAfArray(0, afOutput);
        }

    private:

        AfRandomFunc _afRandomFunc;
        std::string _distribution;
        af::dtype _afDType;
        mutable af::randomEngine _afRandomEngine;
};

/*
 * |PothosDoc Random Source
 *
 * Generates random values from a <b>normal</b> or <b>uniform</b> distribution.
 * For the normal distribution, this block uses <b>af::randn</b>. For the
 * uniform distribution, this block uses <b>af::randu</b>.
 *
 * The underlying random generation scheme can also be customized, although for
 * most purposes, leaving this value as its default will be fine.
 *
 * |category /ArrayFire/Random
 * |keywords array random uniform normal philox threefry mersenne source
 * |factory /arrayfire/random/source(device,dtype,distribution)
 * |setter setDistribution(distribution)
 * |setter setRandomEngineType(randomEngineType)
 *
 * |param device[Device] ArrayFire device to use.
 * |default "Auto"
 * |widget ComboBox(editable=false)
 * |preview enable
 *
 * |param dtype(Data Type) The output's data type.
 * |widget DTypeChooser(int16=1,int32=1,int64=1,uint=1,float=1,cfloat=1,dim=1)
 * |default "float64"
 * |preview disable
 *
 * |param distribution(Distribution)
 * |widget ComboBox(editable=False)
 * |option [Normal] "NORMAL"
 * |option [Uniform] "UNIFORM"
 * |default "NORMAL"
 * |preview enable
 *
 * |param randomEngineType(Random Engine Type)
 * |widget ComboBox(editable=False)
 * |option [Philox] "Philox"
 * |option [Threefry] "Threefry"
 * |option [Mersenne] "Mersenne"
 * |default "Philox"
 * |preview enable
 */
static Pothos::BlockRegistry registerRandomSource(
    "/arrayfire/random/source",
    Pothos::Callable(&RandomBlock::make));

#endif
