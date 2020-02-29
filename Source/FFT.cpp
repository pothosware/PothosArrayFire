// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "ArrayFireBlock.hpp"
#include "Utility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Logger.h>

#include <arrayfire.h>

#include <cmath>
#include <functional>
#include <string>
#include <typeinfo>

// TODO: make numBins an initializer

//
// Misc
//

static bool isPowerOfTwo(size_t num)
{
    if(0 == num)
    {
        return false;
    }

    return (std::ceil(std::log2(num)) == std::floor(std::log2(num)));
}

static const std::string fftBlockPath = "/arrayfire/signal/fft";
static const std::string rfftBlockPath = "/arrayfire/signal/rfft";

//
// Block classes
//

using FFTInPlaceFuncPtr = void(*)(af::array&, const double);
using FFTFuncPtr = af::array(*)(const af::array&, const double);
using FFTFunc = std::function<af::array(const af::array&, const double)>;

template <typename In, typename Out>
class FFTBaseBlock: public ArrayFireBlock
{
    public:
        using InType = In;
        using OutType = Out;
        using Class = FFTBaseBlock<In, Out>;

        FFTBaseBlock(
            const std::string& device,
            size_t numBins,
            double norm,
            size_t dtypeDims,
            const std::string& blockRegistryPath
        ):
            ArrayFireBlock(device),
            _numBins(numBins),
            _norm(0.0) // Set with class setter
        {
            if(!isPowerOfTwo(numBins))
            {
                auto& logger = Poco::Logger::get(blockRegistryPath);
                poco_warning(
                    logger,
                    "This block is most efficient when "
                    "numBins is a power of 2.");
            }

            static const Pothos::DType inDType(typeid(InType));
            static const Pothos::DType outDType(typeid(OutType));

            this->setupInput(
                0,
                Pothos::DType::fromDType(inDType, dtypeDims));
            this->setupOutput(
                0,
                Pothos::DType::fromDType(outDType, dtypeDims));

            this->registerProbe("getNormalizationFactor");
            this->registerSignal("normalizationFactorChanged");

            this->setNormalizationFactor(norm);

            this->registerCall(this, POTHOS_FCN_TUPLE(Class, getNormalizationFactor));
            this->registerCall(this, POTHOS_FCN_TUPLE(Class, setNormalizationFactor));
        }

        virtual ~FFTBaseBlock() = default;

        virtual void work() = 0;

        double getNormalizationFactor() const
        {
            return _norm;
        }

        void setNormalizationFactor(double norm)
        {
            _norm = norm;

            this->emitSignal("normalizationFactorChanged", _norm);
        }

    protected:
        size_t _numBins;
        double _norm;
        size_t _nchans;
};

template <typename T>
class FFTBlock: public FFTBaseBlock<T,T>
{
    public:
        FFTBlock(
            const std::string& device,
            FFTInPlaceFuncPtr func,
            size_t numBins,
            double norm,
            size_t dtypeDims
        ):
            FFTBaseBlock<T,T>(device, numBins, norm, dtypeDims, fftBlockPath),
            _func(func)
        {
        }

        virtual ~FFTBlock() = default;

        void work() override
        {
            auto elems = this->workInfo().minElements;
            if(elems < this->_numBins)
            {
                return;
            }

            auto afArray = this->getInputPortAsAfArray(0);
            _func(afArray, this->_norm);
            this->produceFromAfArray(0, afArray);
        }

    private:
        FFTInPlaceFuncPtr _func;
};

template <typename In, typename Out>
class RFFTBlock: public FFTBaseBlock<In,Out>
{
    public:
        RFFTBlock(
            const std::string& device,
            const FFTFunc& func,
            size_t numBins,
            double norm,
            size_t dtypeDims
        ):
            FFTBaseBlock<In,Out>(device, numBins, norm, dtypeDims, rfftBlockPath),
            _func(func)
        {
        }

        virtual ~RFFTBlock() = default;

        void work() override
        {
            auto elems = this->workInfo().minElements;
            if(elems < this->_numBins)
            {
                return;
            }

            auto afInput = this->getInputPortAsAfArray(0);
            auto afOutput = _func(afInput, this->_norm);
            this->produceFromAfArray(0, afOutput);
        }

    private:
        FFTFunc _func;
};

//
// Factories
//

static Pothos::Block* makeFFT(
    const std::string& device,
    const Pothos::DType& dtype,
    size_t numBins,
    double norm,
    bool inverse)
{
    FFTInPlaceFuncPtr func = inverse ? &af::ifftInPlace : &af::fftInPlace;

    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
            return new FFTBlock<T>(device, func, numBins, norm, dtype.dimension());

    ifTypeDeclareFactory(std::complex<float>)
    ifTypeDeclareFactory(std::complex<double>)
    #undef ifTypeDeclareFactory

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
}

static Pothos::Block* makeRFFT(
    const std::string& device,
    const Pothos::DType& dtype,
    size_t numBins,
    double norm,
    bool inverse)
{
    auto getC2RFunc = [&numBins]() -> FFTFunc
    {
        // We need to point to a specific af::fftC2R overload.
        using fftC2RFuncPtr = af::array(*)(const af::array&, bool, const double);
        fftC2RFuncPtr func = &af::fftC2R<1>;

        FFTFunc ret(std::bind(
                        func,
                        std::placeholders::_1,
                        (1 == (numBins % 2)),
                        std::placeholders::_2));
        return ret;
    };

    FFTFunc func = inverse ? getC2RFunc() : FFTFuncPtr(&af::fftR2C<1>);

    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
        { \
            if(inverse) return new RFFTBlock<T,std::complex<T>>(device,func,numBins,norm,dtype.dimension()); \
            else        return new RFFTBlock<std::complex<T>,T>(device,func,numBins,norm,dtype.dimension()); \
        }

    ifTypeDeclareFactory(float)
    ifTypeDeclareFactory(double)
    #undef ifTypeDeclareFactory

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
}

//
// Block registries
//

/*
 * |PothosDoc FFT
 *
 * Calculates the FFT of the input stream. For the forward FFT, this
 * block uses <b>af::fftInPlace</b> For the reverse FFT, this block
 * uses <b>af::ifftInPlace</b>.
 *
 * |category /ArrayFire/Signal
 * |keywords array signal fft ifft fourier
 * |factory /arrayfire/signal/fft(device,dtype,numBins,norm,inverse)
 * |setter setNormalizationFactor(norm)
 *
 * |param device[Device] ArrayFire device to use.
 * |default "Auto"
 *
 * |param dtype[Data Type] The output's data type.
 * |widget DTypeChooser(cfloat=1,dim=1)
 * |default "complex_float64"
 * |preview disable
 *
 * |param numBins[Num FFT Bins] The number of bins per FFT.
 * |default 1024
 * |option 512
 * |option 1024
 * |option 2048
 * |option 4096
 * |widget ComboBox(editable=true)
 * |preview enable
 *
 * |param norm[Normalization Factor]
 * |widget DoubleSpinBox(minimum=0.0)
 * |default 1.0
 * |preview enable
 *
 * |param inverse[Inverse?]
 * |widget ToggleSwitch()
 * |preview enable
 * |default false
 */
static Pothos::BlockRegistry registerFFT(
    fftBlockPath,
    Pothos::Callable(&makeFFT));

/*
 * |PothosDoc Real FFT
 *
 * Calculates the real FFT of the input stream. For the forward FFT, this
 * block uses <b>af::fftR2C\<1\></b>. For the reverse FFT, this block uses
 * <b>af::fftC2R\<1\></b>.
 *
 * |category /ArrayFire/Signal
 * |keywords array signal fft ifft rfft fourier
 * |factory /arrayfire/signal/rfft(device,dtype,numBins,norm,inverse)
 * |setter setNormalizationFactor(norm)
 *
 * |param device[Device] ArrayFire device to use.
 * |default "Auto"
 *
 * |param dtype[Data Type] The floating-type underlying the input types.
 * |widget DTypeChooser(float=1,dim=1)
 * |default "float64"
 * |preview disable
 *
 * |param numBins[Num FFT Bins] The number of bins per FFT.
 * |default 1024
 * |option 512
 * |option 1024
 * |option 2048
 * |option 4096
 * |widget ComboBox(editable=true)
 * |preview enable
 *
 * |param norm[Normalization Factor]
 * |widget DoubleSpinBox(minimum=0.0)
 * |default 1.0
 * |preview enable
 *
 * |param inverse[Inverse?]
 * |widget ToggleSwitch()
 * |preview enable
 * |default false
 */
static Pothos::BlockRegistry registerRFFT(
    rfftBlockPath,
    Pothos::Callable(&makeRFFT));
