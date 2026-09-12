#include "Crowny/Utils/Cryptography.h"
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Digest strings retain the entire digest", "[Common][Cryptography]")
{
    CHECK(Cryptography::MD5("abc") == "900150983CD24FB0D6963F7D28E17F72");
    CHECK(Cryptography::SHA1("abc") == "A9993E364706816ABA3E25717850C26C9CD0D89D");
    CHECK(Cryptography::SHA256("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
}
