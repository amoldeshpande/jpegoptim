#include "stdafx.h"
#include "CppUnitTest.h"
#include "jpegoptim.h"
#include "modular.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LibUnitTest
{		
	TEST_CLASS(UnitTest1)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
            size_t length;
            unsigned char* filebuf = (unsigned char*)read_all_bytes(R"(f:\photos\targets\WP_20170512_11_37_04_Rich_LI.jpg)",&length);
            unsigned char* outputbuf;
            size_t outlen;
            struct jpegoptim_options options;
            init_options(&options);
            options.sizeKB = 4000;
            options.quality = 95;
            call_jpegoptim(&options, filebuf, length, &outputbuf, &outlen);

            write_all_bytes(R"(I:\github\jpegoptim\msvs\Output\Debug\x64\x.jpg)", (char*)outputbuf, outlen);
		}

	};
}