#include "BlockExecutionTests.hpp"
#include "TestUtility.hpp"

#include <Pothos/Testing.hpp>

#include <complex>
#include <cmath>
#include <cstdint>

using namespace GPUTests;

%for k,v in sfinaeMap.items():

template <typename T>
static EnableIf${k}<T, void> blockExecutionTest()
{
    %for block in oneToOneBlocks:
        %if ("supportedTypes" in block) and ("autoTest" in block):
            %if block["supportedTypes"].get("support{0}".format(v), block["supportedTypes"].get("supportAll", False)):
    testOneToOneBlock<T>("/gpu/${block["header"]}/${block["blockName"]}");
            %endif
        %endif
    %endfor

    %for block in twoToOneBlocks:
        %if "autoTest" not in block:

        %elif (block.get("pattern", "") == "FloatToComplex") and (k == "Float"):
    testTwoToOneBlockF2C<T>(
        "/gpu/${block["header"]}/${block["blockName"]}",
        ${"false" if block.get("allowZeroInBuffer1", True) else "true"});
        %elif "supportedTypes" in block:
            %if block["supportedTypes"].get("support{0}".format(v), block["supportedTypes"].get("supportAll", False)):
    testTwoToOneBlock<T>(
        "/gpu/${block["header"]}/${block["blockName"]}",
        ${"false" if block.get("allowZeroInBuffer1", True) else "true"});
            %endif
        %endif
    %endfor

    %for block in NToOneBlocks:
        %if ("supportedTypes" in block) and ("autoTest" in block):
            %if block["supportedTypes"].get("support{0}".format(v), block["supportedTypes"].get("supportAll", False)):
    testNToOneBlock<T>("/gpu/${block["header"]}/${block["blockName"]}", 2);
    testNToOneBlock<T>("/gpu/${block["header"]}/${block["blockName"]}", 5);
            %endif
        %endif
    %endfor
}
%endfor

POTHOS_TEST_BLOCK("/gpu/tests", test_block_execution)
{
    setupTestEnv();

    blockExecutionTest<char>();
    blockExecutionTest<signed char>();
    blockExecutionTest<short>();
    blockExecutionTest<int>();
    blockExecutionTest<long long>();
    blockExecutionTest<unsigned char>();
    blockExecutionTest<unsigned short>();
    blockExecutionTest<unsigned>();
    blockExecutionTest<unsigned long long>();
    blockExecutionTest<float>();
    blockExecutionTest<double>();
    blockExecutionTest<std::complex<float>>();
    blockExecutionTest<std::complex<double>>();
}
