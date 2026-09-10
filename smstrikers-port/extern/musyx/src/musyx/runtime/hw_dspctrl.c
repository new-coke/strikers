/*















*/
#include "musyx/assert.h"
#include "musyx/dspvoice.h"
#include "musyx/hardware.h"
#include "musyx/sal.h"
#include "musyx/stream.h"

/* TODO: This needs backend specific rewrites for e.g: SDL, each backend should have their own
 * discrete file so they can handle their unique requirements
 */

#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
#include "dolphin/os/OSCache.h"
#include <dolphin/os.h>
#endif

#include <string.h>

#ifdef _DEBUG
static u32 dbgActiveVoicesMax = 0;
#endif

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 3)
u16 compressorTable[3360] ATTRIBUTE_ALIGN(32) = {
#else
u16 compressorTable[3360] = {
#endif
    0x7FA1, 0x7F43, 0x7EE6, 0x7E88, 0x7E2B, 0x7DCE, 0x7D72, 0x7D16, 0x7CBA, 0x7C5E, 0x7C02, 0x7BA7,
    0x7B4C, 0x7AF1, 0x7A97, 0x7A3D, 0x79E3, 0x7989, 0x7930, 0x78D6, 0x787E, 0x7825, 0x77CD, 0x7774,
    0x771C, 0x76C5, 0x766D, 0x7616, 0x75BF, 0x7569, 0x7512, 0x74BC, 0x7466, 0x7411, 0x73BB, 0x7366,
    0x7311, 0x72BD, 0x7268, 0x7214, 0x71C0, 0x716C, 0x7119, 0x70C6, 0x7073, 0x7020, 0x6FCD, 0x6F7B,
    0x6F29, 0x6ED7, 0x6E86, 0x6E35, 0x6DE3, 0x6D93, 0x6D42, 0x6CF2, 0x6CA1, 0x6C52, 0x6C02, 0x6BB2,
    0x6B63, 0x6B14, 0x6AC5, 0x6A77, 0x6A28, 0x69DA, 0x698C, 0x693F, 0x68F1, 0x68A4, 0x6857, 0x680A,
    0x67BE, 0x6771, 0x6725, 0x66D9, 0x668E, 0x6642, 0x65F7, 0x65AC, 0x6561, 0x6517, 0x64CC, 0x6482,
    0x6438, 0x63EE, 0x63A5, 0x635C, 0x6312, 0x62CA, 0x6281, 0x6238, 0x61F0, 0x61A8, 0x6160, 0x6119,
    0x60D1, 0x608A, 0x6043, 0x5FFC, 0x5FB5, 0x5F6F, 0x5F29, 0x5EE3, 0x5E9D, 0x5E57, 0x5E12, 0x5DCD,
    0x5D88, 0x5D43, 0x5CFE, 0x5CBA, 0x5C76, 0x5C32, 0x5BEE, 0x5BAA, 0x5B67, 0x5B23, 0x5AE0, 0x5A9D,
    0x5A5B, 0x5A18, 0x59D6, 0x5994, 0x5952, 0x5910, 0x58CF, 0x588D, 0x584C, 0x580B, 0x57CB, 0x578A,
    0x574A, 0x5709, 0x56C9, 0x5689, 0x564A, 0x560A, 0x55CB, 0x558C, 0x554D, 0x550E, 0x54D0, 0x5491,
    0x5453, 0x5415, 0x53D7, 0x5399, 0x535C, 0x531E, 0x52E1, 0x52A4, 0x5267, 0x522B, 0x51EE, 0x51B2,
    0x5176, 0x513A, 0x50FE, 0x50C3, 0x79EC, 0x799B, 0x794A, 0x78FA, 0x78AA, 0x785A, 0x780A, 0x77BB,
    0x776C, 0x771C, 0x76CE, 0x767F, 0x7630, 0x75E2, 0x7594, 0x7546, 0x74F9, 0x74AB, 0x745E, 0x7411,
    0x73C4, 0x7377, 0x732B, 0x72DE, 0x7292, 0x7246, 0x71FB, 0x71AF, 0x7164, 0x7119, 0x70CE, 0x7083,
    0x7039, 0x6FEE, 0x6FA4, 0x6F5A, 0x6F11, 0x6EC7, 0x6E7E, 0x6E35, 0x6DEC, 0x6DA3, 0x6D5A, 0x6D12,
    0x6CC9, 0x6C81, 0x6C3A, 0x6BF2, 0x6BAA, 0x6B63, 0x6B1C, 0x6AD5, 0x6A8E, 0x6A48, 0x6A01, 0x69BB,
    0x6975, 0x692F, 0x68EA, 0x68A4, 0x685F, 0x681A, 0x67D5, 0x6790, 0x674B, 0x6707, 0x66C3, 0x667F,
    0x663B, 0x65F7, 0x65B4, 0x6570, 0x652D, 0x64EA, 0x64A7, 0x6464, 0x6422, 0x63E0, 0x639E, 0x635C,
    0x631A, 0x62D8, 0x6297, 0x6255, 0x6214, 0x61D3, 0x6192, 0x6152, 0x6111, 0x60D1, 0x6091, 0x6051,
    0x6011, 0x5FD2, 0x5F92, 0x5F53, 0x5F14, 0x5ED5, 0x5E96, 0x5E57, 0x5E19, 0x5DDB, 0x5D9C, 0x5D5E,
    0x5D21, 0x5CE3, 0x5CA5, 0x5C68, 0x5C2B, 0x5BEE, 0x5BB1, 0x5B74, 0x5B38, 0x5AFB, 0x5ABF, 0x5A83,
    0x5A47, 0x5A0B, 0x59CF, 0x5994, 0x5959, 0x591D, 0x58E2, 0x58A8, 0x586D, 0x5832, 0x57F8, 0x57BE,
    0x5783, 0x574A, 0x5710, 0x56D6, 0x569D, 0x5663, 0x562A, 0x55F1, 0x55B8, 0x557F, 0x5547, 0x550E,
    0x54D6, 0x549E, 0x5466, 0x542E, 0x53F6, 0x53BE, 0x5387, 0x534F, 0x5318, 0x52E1, 0x52AA, 0x5274,
    0x523D, 0x5207, 0x51D0, 0x519A, 0x5164, 0x512E, 0x50F8, 0x50C3, 0x7478, 0x7433, 0x73EF, 0x73AA,
    0x7366, 0x7322, 0x72DE, 0x729B, 0x7257, 0x7214, 0x71D1, 0x718E, 0x714B, 0x7108, 0x70C6, 0x7083,
    0x7041, 0x6FFF, 0x6FBD, 0x6F7B, 0x6F3A, 0x6EF8, 0x6EB7, 0x6E76, 0x6E35, 0x6DF4, 0x6DB3, 0x6D72,
    0x6D32, 0x6CF2, 0x6CB1, 0x6C71, 0x6C32, 0x6BF2, 0x6BB2, 0x6B73, 0x6B34, 0x6AF5, 0x6AB6, 0x6A77,
    0x6A38, 0x69FA, 0x69BB, 0x697D, 0x693F, 0x6901, 0x68C3, 0x6885, 0x6848, 0x680A, 0x67CD, 0x6790,
    0x6753, 0x6716, 0x66D9, 0x669D, 0x6660, 0x6624, 0x65E8, 0x65AC, 0x6570, 0x6534, 0x64F9, 0x64BD,
    0x6482, 0x6447, 0x640C, 0x63D1, 0x6396, 0x635C, 0x6321, 0x62E7, 0x62AC, 0x6272, 0x6238, 0x61FF,
    0x61C5, 0x618B, 0x6152, 0x6119, 0x60DF, 0x60A6, 0x606D, 0x6035, 0x5FFC, 0x5FC4, 0x5F8B, 0x5F53,
    0x5F1B, 0x5EE3, 0x5EAB, 0x5E73, 0x5E3C, 0x5E04, 0x5DCD, 0x5D95, 0x5D5E, 0x5D27, 0x5CF1, 0x5CBA,
    0x5C83, 0x5C4D, 0x5C16, 0x5BE0, 0x5BAA, 0x5B74, 0x5B3E, 0x5B09, 0x5AD3, 0x5A9D, 0x5A68, 0x5A33,
    0x59FE, 0x59C9, 0x5994, 0x595F, 0x592B, 0x58F6, 0x58C2, 0x588D, 0x5859, 0x5825, 0x57F1, 0x57BE,
    0x578A, 0x5756, 0x5723, 0x56F0, 0x56BC, 0x5689, 0x5656, 0x5624, 0x55F1, 0x55BE, 0x558C, 0x5559,
    0x5527, 0x54F5, 0x54C3, 0x5491, 0x545F, 0x542E, 0x53FC, 0x53CB, 0x5399, 0x5368, 0x5337, 0x5306,
    0x52D5, 0x52A4, 0x5274, 0x5243, 0x5213, 0x51E2, 0x51B2, 0x5182, 0x5152, 0x5122, 0x50F2, 0x50C3,
    0x6F42, 0x6F08, 0x6ECF, 0x6E96, 0x6E5D, 0x6E24, 0x6DEC, 0x6DB3, 0x6D7A, 0x6D42, 0x6D0A, 0x6CD2,
    0x6C99, 0x6C61, 0x6C2A, 0x6BF2, 0x6BBA, 0x6B83, 0x6B4B, 0x6B14, 0x6ADD, 0x6AA6, 0x6A6F, 0x6A38,
    0x6A01, 0x69CB, 0x6994, 0x695E, 0x6927, 0x68F1, 0x68BB, 0x6885, 0x684F, 0x681A, 0x67E4, 0x67AE,
    0x6779, 0x6744, 0x670F, 0x66D9, 0x66A4, 0x6670, 0x663B, 0x6606, 0x65D2, 0x659D, 0x6569, 0x6534,
    0x6500, 0x64CC, 0x6498, 0x6464, 0x6431, 0x63FD, 0x63CA, 0x6396, 0x6363, 0x6330, 0x62FD, 0x62CA,
    0x6297, 0x6264, 0x6231, 0x61FF, 0x61CC, 0x619A, 0x6167, 0x6135, 0x6103, 0x60D1, 0x609F, 0x606D,
    0x603C, 0x600A, 0x5FD9, 0x5FA7, 0x5F76, 0x5F45, 0x5F14, 0x5EE3, 0x5EB2, 0x5E81, 0x5E50, 0x5E20,
    0x5DEF, 0x5DBF, 0x5D8F, 0x5D5E, 0x5D2E, 0x5CFE, 0x5CCE, 0x5C9F, 0x5C6F, 0x5C3F, 0x5C10, 0x5BE0,
    0x5BB1, 0x5B82, 0x5B52, 0x5B23, 0x5AF4, 0x5AC6, 0x5A97, 0x5A68, 0x5A3A, 0x5A0B, 0x59DD, 0x59AE,
    0x5980, 0x5952, 0x5924, 0x58F6, 0x58C8, 0x589A, 0x586D, 0x583F, 0x5812, 0x57E4, 0x57B7, 0x578A,
    0x575D, 0x5730, 0x5703, 0x56D6, 0x56A9, 0x567D, 0x5650, 0x5624, 0x55F7, 0x55CB, 0x559F, 0x5573,
    0x5547, 0x551B, 0x54EF, 0x54C3, 0x5497, 0x546C, 0x5440, 0x5415, 0x53EA, 0x53BE, 0x5393, 0x5368,
    0x533D, 0x5312, 0x52E7, 0x52BD, 0x5292, 0x5267, 0x523D, 0x5213, 0x51E8, 0x51BE, 0x5194, 0x516A,
    0x5140, 0x5116, 0x50EC, 0x50C3, 0x6A48, 0x6A19, 0x69EA, 0x69BB, 0x698C, 0x695E, 0x692F, 0x6901,
    0x68D2, 0x68A4, 0x6876, 0x6848, 0x681A, 0x67EC, 0x67BE, 0x6790, 0x6762, 0x6735, 0x6707, 0x66D9,
    0x66AC, 0x667F, 0x6651, 0x6624, 0x65F7, 0x65CA, 0x659D, 0x6570, 0x6543, 0x6517, 0x64EA, 0x64BD,
    0x6491, 0x6464, 0x6438, 0x640C, 0x63E0, 0x63B4, 0x6388, 0x635C, 0x6330, 0x6304, 0x62D8, 0x62AC,
    0x6281, 0x6255, 0x622A, 0x61FF, 0x61D3, 0x61A8, 0x617D, 0x6152, 0x6127, 0x60FC, 0x60D1, 0x60A6,
    0x607C, 0x6051, 0x6027, 0x5FFC, 0x5FD2, 0x5FA7, 0x5F7D, 0x5F53, 0x5F29, 0x5EFF, 0x5ED5, 0x5EAB,
    0x5E81, 0x5E57, 0x5E2E, 0x5E04, 0x5DDB, 0x5DB1, 0x5D88, 0x5D5E, 0x5D35, 0x5D0C, 0x5CE3, 0x5CBA,
    0x5C91, 0x5C68, 0x5C3F, 0x5C16, 0x5BEE, 0x5BC5, 0x5B9D, 0x5B74, 0x5B4C, 0x5B23, 0x5AFB, 0x5AD3,
    0x5AAB, 0x5A83, 0x5A5B, 0x5A33, 0x5A0B, 0x59E3, 0x59BC, 0x5994, 0x596C, 0x5945, 0x591D, 0x58F6,
    0x58CF, 0x58A8, 0x5880, 0x5859, 0x5832, 0x580B, 0x57E4, 0x57BE, 0x5797, 0x5770, 0x574A, 0x5723,
    0x56FC, 0x56D6, 0x56B0, 0x5689, 0x5663, 0x563D, 0x5617, 0x55F1, 0x55CB, 0x55A5, 0x557F, 0x5559,
    0x5534, 0x550E, 0x54E9, 0x54C3, 0x549E, 0x5478, 0x5453, 0x542E, 0x5408, 0x53E3, 0x53BE, 0x5399,
    0x5374, 0x534F, 0x532B, 0x5306, 0x52E1, 0x52BD, 0x5298, 0x5274, 0x524F, 0x522B, 0x5207, 0x51E2,
    0x51BE, 0x519A, 0x5176, 0x5152, 0x512E, 0x510A, 0x50E6, 0x50C3, 0x6587, 0x6561, 0x653C, 0x6517,
    0x64F1, 0x64CC, 0x64A7, 0x6482, 0x645D, 0x6438, 0x6413, 0x63EE, 0x63CA, 0x63A5, 0x6380, 0x635C,
    0x6337, 0x6312, 0x62EE, 0x62CA, 0x62A5, 0x6281, 0x625D, 0x6238, 0x6214, 0x61F0, 0x61CC, 0x61A8,
    0x6184, 0x6160, 0x613C, 0x6119, 0x60F5, 0x60D1, 0x60AD, 0x608A, 0x6066, 0x6043, 0x601F, 0x5FFC,
    0x5FD9, 0x5FB5, 0x5F92, 0x5F6F, 0x5F4C, 0x5F29, 0x5F06, 0x5EE3, 0x5EC0, 0x5E9D, 0x5E7A, 0x5E57,
    0x5E35, 0x5E12, 0x5DEF, 0x5DCD, 0x5DAA, 0x5D88, 0x5D65, 0x5D43, 0x5D21, 0x5CFE, 0x5CDC, 0x5CBA,
    0x5C98, 0x5C76, 0x5C54, 0x5C32, 0x5C10, 0x5BEE, 0x5BCC, 0x5BAA, 0x5B88, 0x5B67, 0x5B45, 0x5B23,
    0x5B02, 0x5AE0, 0x5ABF, 0x5A9D, 0x5A7C, 0x5A5B, 0x5A3A, 0x5A18, 0x59F7, 0x59D6, 0x59B5, 0x5994,
    0x5973, 0x5952, 0x5931, 0x5910, 0x58F0, 0x58CF, 0x58AE, 0x588D, 0x586D, 0x584C, 0x582C, 0x580B,
    0x57EB, 0x57CB, 0x57AA, 0x578A, 0x576A, 0x574A, 0x5729, 0x5709, 0x56E9, 0x56C9, 0x56A9, 0x5689,
    0x566A, 0x564A, 0x562A, 0x560A, 0x55EB, 0x55CB, 0x55AB, 0x558C, 0x556C, 0x554D, 0x552D, 0x550E,
    0x54EF, 0x54D0, 0x54B0, 0x5491, 0x5472, 0x5453, 0x5434, 0x5415, 0x53F6, 0x53D7, 0x53B8, 0x5399,
    0x537B, 0x535C, 0x533D, 0x531E, 0x5300, 0x52E1, 0x52C3, 0x52A4, 0x5286, 0x5267, 0x5249, 0x522B,
    0x520D, 0x51EE, 0x51D0, 0x51B2, 0x5194, 0x5176, 0x5158, 0x513A, 0x511C, 0x50FE, 0x50E0, 0x50C3,
    0x60FC, 0x60DF, 0x60C3, 0x60A6, 0x608A, 0x606D, 0x6051, 0x6035, 0x6018, 0x5FFC, 0x5FE0, 0x5FC4,
    0x5FA7, 0x5F8B, 0x5F6F, 0x5F53, 0x5F37, 0x5F1B, 0x5EFF, 0x5EE3, 0x5EC7, 0x5EAB, 0x5E8F, 0x5E73,
    0x5E57, 0x5E3C, 0x5E20, 0x5E04, 0x5DE8, 0x5DCD, 0x5DB1, 0x5D95, 0x5D7A, 0x5D5E, 0x5D43, 0x5D27,
    0x5D0C, 0x5CF1, 0x5CD5, 0x5CBA, 0x5C9F, 0x5C83, 0x5C68, 0x5C4D, 0x5C32, 0x5C16, 0x5BFB, 0x5BE0,
    0x5BC5, 0x5BAA, 0x5B8F, 0x5B74, 0x5B59, 0x5B3E, 0x5B23, 0x5B09, 0x5AEE, 0x5AD3, 0x5AB8, 0x5A9D,
    0x5A83, 0x5A68, 0x5A4D, 0x5A33, 0x5A18, 0x59FE, 0x59E3, 0x59C9, 0x59AE, 0x5994, 0x597A, 0x595F,
    0x5945, 0x592B, 0x5910, 0x58F6, 0x58DC, 0x58C2, 0x58A8, 0x588D, 0x5873, 0x5859, 0x583F, 0x5825,
    0x580B, 0x57F1, 0x57D7, 0x57BE, 0x57A4, 0x578A, 0x5770, 0x5756, 0x573D, 0x5723, 0x5709, 0x56F0,
    0x56D6, 0x56BC, 0x56A3, 0x5689, 0x5670, 0x5656, 0x563D, 0x5624, 0x560A, 0x55F1, 0x55D8, 0x55BE,
    0x55A5, 0x558C, 0x5573, 0x5559, 0x5540, 0x5527, 0x550E, 0x54F5, 0x54DC, 0x54C3, 0x54AA, 0x5491,
    0x5478, 0x545F, 0x5446, 0x542E, 0x5415, 0x53FC, 0x53E3, 0x53CB, 0x53B2, 0x5399, 0x5381, 0x5368,
    0x534F, 0x5337, 0x531E, 0x5306, 0x52ED, 0x52D5, 0x52BD, 0x52A4, 0x528C, 0x5274, 0x525B, 0x5243,
    0x522B, 0x5213, 0x51FA, 0x51E2, 0x51CA, 0x51B2, 0x519A, 0x5182, 0x516A, 0x5152, 0x513A, 0x5122,
    0x510A, 0x50F2, 0x50DB, 0x50C3, 0x5CA5, 0x5C91, 0x5C7C, 0x5C68, 0x5C54, 0x5C3F, 0x5C2B, 0x5C16,
    0x5C02, 0x5BEE, 0x5BD9, 0x5BC5, 0x5BB1, 0x5B9D, 0x5B88, 0x5B74, 0x5B60, 0x5B4C, 0x5B38, 0x5B23,
    0x5B0F, 0x5AFB, 0x5AE7, 0x5AD3, 0x5ABF, 0x5AAB, 0x5A97, 0x5A83, 0x5A6F, 0x5A5B, 0x5A47, 0x5A33,
    0x5A1F, 0x5A0B, 0x59F7, 0x59E3, 0x59CF, 0x59BC, 0x59A8, 0x5994, 0x5980, 0x596C, 0x5959, 0x5945,
    0x5931, 0x591D, 0x590A, 0x58F6, 0x58E2, 0x58CF, 0x58BB, 0x58A8, 0x5894, 0x5880, 0x586D, 0x5859,
    0x5846, 0x5832, 0x581F, 0x580B, 0x57F8, 0x57E4, 0x57D1, 0x57BE, 0x57AA, 0x5797, 0x5783, 0x5770,
    0x575D, 0x574A, 0x5736, 0x5723, 0x5710, 0x56FC, 0x56E9, 0x56D6, 0x56C3, 0x56B0, 0x569D, 0x5689,
    0x5676, 0x5663, 0x5650, 0x563D, 0x562A, 0x5617, 0x5604, 0x55F1, 0x55DE, 0x55CB, 0x55B8, 0x55A5,
    0x5592, 0x557F, 0x556C, 0x5559, 0x5547, 0x5534, 0x5521, 0x550E, 0x54FB, 0x54E9, 0x54D6, 0x54C3,
    0x54B0, 0x549E, 0x548B, 0x5478, 0x5466, 0x5453, 0x5440, 0x542E, 0x541B, 0x5408, 0x53F6, 0x53E3,
    0x53D1, 0x53BE, 0x53AC, 0x5399, 0x5387, 0x5374, 0x5362, 0x534F, 0x533D, 0x532B, 0x5318, 0x5306,
    0x52F4, 0x52E1, 0x52CF, 0x52BD, 0x52AA, 0x5298, 0x5286, 0x5274, 0x5261, 0x524F, 0x523D, 0x522B,
    0x5219, 0x5207, 0x51F4, 0x51E2, 0x51D0, 0x51BE, 0x51AC, 0x519A, 0x5188, 0x5176, 0x5164, 0x5152,
    0x5140, 0x512E, 0x511C, 0x510A, 0x50F8, 0x50E6, 0x50D5, 0x50C3, 0x5880, 0x5873, 0x5866, 0x5859,
    0x584C, 0x583F, 0x5832, 0x5825, 0x5818, 0x580B, 0x57FE, 0x57F1, 0x57E4, 0x57D7, 0x57CB, 0x57BE,
    0x57B1, 0x57A4, 0x5797, 0x578A, 0x577D, 0x5770, 0x5763, 0x5756, 0x574A, 0x573D, 0x5730, 0x5723,
    0x5716, 0x5709, 0x56FC, 0x56F0, 0x56E3, 0x56D6, 0x56C9, 0x56BC, 0x56B0, 0x56A3, 0x5696, 0x5689,
    0x567D, 0x5670, 0x5663, 0x5656, 0x564A, 0x563D, 0x5630, 0x5624, 0x5617, 0x560A, 0x55FE, 0x55F1,
    0x55E4, 0x55D8, 0x55CB, 0x55BE, 0x55B2, 0x55A5, 0x5598, 0x558C, 0x557F, 0x5573, 0x5566, 0x5559,
    0x554D, 0x5540, 0x5534, 0x5527, 0x551B, 0x550E, 0x5502, 0x54F5, 0x54E9, 0x54DC, 0x54D0, 0x54C3,
    0x54B7, 0x54AA, 0x549E, 0x5491, 0x5485, 0x5478, 0x546C, 0x545F, 0x5453, 0x5446, 0x543A, 0x542E,
    0x5421, 0x5415, 0x5408, 0x53FC, 0x53F0, 0x53E3, 0x53D7, 0x53CB, 0x53BE, 0x53B2, 0x53A6, 0x5399,
    0x538D, 0x5381, 0x5374, 0x5368, 0x535C, 0x534F, 0x5343, 0x5337, 0x532B, 0x531E, 0x5312, 0x5306,
    0x52FA, 0x52ED, 0x52E1, 0x52D5, 0x52C9, 0x52BD, 0x52B0, 0x52A4, 0x5298, 0x528C, 0x5280, 0x5274,
    0x5267, 0x525B, 0x524F, 0x5243, 0x5237, 0x522B, 0x521F, 0x5213, 0x5207, 0x51FA, 0x51EE, 0x51E2,
    0x51D6, 0x51CA, 0x51BE, 0x51B2, 0x51A6, 0x519A, 0x518E, 0x5182, 0x5176, 0x516A, 0x515E, 0x5152,
    0x5146, 0x513A, 0x512E, 0x5122, 0x5116, 0x510A, 0x50FE, 0x50F2, 0x50E6, 0x50DB, 0x50CF, 0x50C3,
    0x548B, 0x5485, 0x547E, 0x5478, 0x5472, 0x546C, 0x5466, 0x545F, 0x5459, 0x5453, 0x544D, 0x5446,
    0x5440, 0x543A, 0x5434, 0x542E, 0x5427, 0x5421, 0x541B, 0x5415, 0x540F, 0x5408, 0x5402, 0x53FC,
    0x53F6, 0x53F0, 0x53EA, 0x53E3, 0x53DD, 0x53D7, 0x53D1, 0x53CB, 0x53C4, 0x53BE, 0x53B8, 0x53B2,
    0x53AC, 0x53A6, 0x539F, 0x5399, 0x5393, 0x538D, 0x5387, 0x5381, 0x537B, 0x5374, 0x536E, 0x5368,
    0x5362, 0x535C, 0x5356, 0x534F, 0x5349, 0x5343, 0x533D, 0x5337, 0x5331, 0x532B, 0x5325, 0x531E,
    0x5318, 0x5312, 0x530C, 0x5306, 0x5300, 0x52FA, 0x52F4, 0x52ED, 0x52E7, 0x52E1, 0x52DB, 0x52D5,
    0x52CF, 0x52C9, 0x52C3, 0x52BD, 0x52B7, 0x52B0, 0x52AA, 0x52A4, 0x529E, 0x5298, 0x5292, 0x528C,
    0x5286, 0x5280, 0x527A, 0x5274, 0x526E, 0x5267, 0x5261, 0x525B, 0x5255, 0x524F, 0x5249, 0x5243,
    0x523D, 0x5237, 0x5231, 0x522B, 0x5225, 0x521F, 0x5219, 0x5213, 0x520D, 0x5207, 0x5201, 0x51FA,
    0x51F4, 0x51EE, 0x51E8, 0x51E2, 0x51DC, 0x51D6, 0x51D0, 0x51CA, 0x51C4, 0x51BE, 0x51B8, 0x51B2,
    0x51AC, 0x51A6, 0x51A0, 0x519A, 0x5194, 0x518E, 0x5188, 0x5182, 0x517C, 0x5176, 0x5170, 0x516A,
    0x5164, 0x515E, 0x5158, 0x5152, 0x514C, 0x5146, 0x5140, 0x513A, 0x5134, 0x512E, 0x5128, 0x5122,
    0x511C, 0x5116, 0x5110, 0x510A, 0x5104, 0x50FE, 0x50F8, 0x50F2, 0x50EC, 0x50E6, 0x50E0, 0x50DB,
    0x50D5, 0x50CF, 0x50C9, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3,
    0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x50C3, 0x7A46, 0x7A4F, 0x7A58, 0x7A61,
    0x7A6A, 0x7A73, 0x7A7C, 0x7A85, 0x7A8E, 0x7A97, 0x7AA0, 0x7AA9, 0x7AB2, 0x7ABB, 0x7AC4, 0x7ACD,
    0x7AD6, 0x7ADF, 0x7AE8, 0x7AF1, 0x7AFA, 0x7B03, 0x7B0D, 0x7B16, 0x7B1F, 0x7B28, 0x7B31, 0x7B3A,
    0x7B43, 0x7B4C, 0x7B55, 0x7B5E, 0x7B67, 0x7B70, 0x7B7A, 0x7B83, 0x7B8C, 0x7B95, 0x7B9E, 0x7BA7,
    0x7BB0, 0x7BB9, 0x7BC2, 0x7BCC, 0x7BD5, 0x7BDE, 0x7BE7, 0x7BF0, 0x7BF9, 0x7C02, 0x7C0B, 0x7C15,
    0x7C1E, 0x7C27, 0x7C30, 0x7C39, 0x7C42, 0x7C4B, 0x7C55, 0x7C5E, 0x7C67, 0x7C70, 0x7C79, 0x7C82,
    0x7C8C, 0x7C95, 0x7C9E, 0x7CA7, 0x7CB0, 0x7CBA, 0x7CC3, 0x7CCC, 0x7CD5, 0x7CDE, 0x7CE8, 0x7CF1,
    0x7CFA, 0x7D03, 0x7D0C, 0x7D16, 0x7D1F, 0x7D28, 0x7D31, 0x7D3A, 0x7D44, 0x7D4D, 0x7D56, 0x7D5F,
    0x7D69, 0x7D72, 0x7D7B, 0x7D84, 0x7D8E, 0x7D97, 0x7DA0, 0x7DA9, 0x7DB3, 0x7DBC, 0x7DC5, 0x7DCE,
    0x7DD8, 0x7DE1, 0x7DEA, 0x7DF4, 0x7DFD, 0x7E06, 0x7E0F, 0x7E19, 0x7E22, 0x7E2B, 0x7E35, 0x7E3E,
    0x7E47, 0x7E51, 0x7E5A, 0x7E63, 0x7E6C, 0x7E76, 0x7E7F, 0x7E88, 0x7E92, 0x7E9B, 0x7EA4, 0x7EAE,
    0x7EB7, 0x7EC0, 0x7ECA, 0x7ED3, 0x7EDC, 0x7EE6, 0x7EEF, 0x7EF8, 0x7F02, 0x7F0B, 0x7F15, 0x7F1E,
    0x7F27, 0x7F31, 0x7F3A, 0x7F43, 0x7F4D, 0x7F56, 0x7F60, 0x7F69, 0x7F72, 0x7F7C, 0x7F85, 0x7F8F,
    0x7F98, 0x7FA1, 0x7FAB, 0x7FB4, 0x7FBE, 0x7FC7, 0x7FD0, 0x7FDA, 0x7FE3, 0x7FED, 0x7FF6, 0x8000,
    0x74C5, 0x74CD, 0x74D6, 0x74DF, 0x74E7, 0x74F0, 0x74F9, 0x7501, 0x750A, 0x7512, 0x751B, 0x7524,
    0x752C, 0x7535, 0x753E, 0x7546, 0x754F, 0x7558, 0x7560, 0x7569, 0x7571, 0x757A, 0x7583, 0x758B,
    0x7594, 0x759D, 0x75A5, 0x75AE, 0x75B7, 0x75BF, 0x75C8, 0x75D1, 0x75D9, 0x75E2, 0x75EB, 0x75F4,
    0x75FC, 0x7605, 0x760E, 0x7616, 0x761F, 0x7628, 0x7630, 0x7639, 0x7642, 0x764B, 0x7653, 0x765C,
    0x7665, 0x766D, 0x7676, 0x767F, 0x7688, 0x7690, 0x7699, 0x76A2, 0x76AB, 0x76B3, 0x76BC, 0x76C5,
    0x76CE, 0x76D6, 0x76DF, 0x76E8, 0x76F1, 0x76F9, 0x7702, 0x770B, 0x7714, 0x771C, 0x7725, 0x772E,
    0x7737, 0x7740, 0x7748, 0x7751, 0x775A, 0x7763, 0x776C, 0x7774, 0x777D, 0x7786, 0x778F, 0x7798,
    0x77A0, 0x77A9, 0x77B2, 0x77BB, 0x77C4, 0x77CD, 0x77D5, 0x77DE, 0x77E7, 0x77F0, 0x77F9, 0x7802,
    0x780A, 0x7813, 0x781C, 0x7825, 0x782E, 0x7837, 0x783F, 0x7848, 0x7851, 0x785A, 0x7863, 0x786C,
    0x7875, 0x787E, 0x7886, 0x788F, 0x7898, 0x78A1, 0x78AA, 0x78B3, 0x78BC, 0x78C5, 0x78CE, 0x78D6,
    0x78DF, 0x78E8, 0x78F1, 0x78FA, 0x7903, 0x790C, 0x7915, 0x791E, 0x7927, 0x7930, 0x7939, 0x7942,
    0x794A, 0x7953, 0x795C, 0x7965, 0x796E, 0x7977, 0x7980, 0x7989, 0x7992, 0x799B, 0x79A4, 0x79AD,
    0x79B6, 0x79BF, 0x79C8, 0x79D1, 0x79DA, 0x79E3, 0x79EC, 0x79F5, 0x79FE, 0x7A07, 0x7A10, 0x7A19,
    0x7A22, 0x7A2B, 0x7A34, 0x7A3D, 0x6F83, 0x6F8C, 0x6F94, 0x6F9C, 0x6FA4, 0x6FAD, 0x6FB5, 0x6FBD,
    0x6FC5, 0x6FCD, 0x6FD6, 0x6FDE, 0x6FE6, 0x6FEE, 0x6FF7, 0x6FFF, 0x7007, 0x700F, 0x7018, 0x7020,
    0x7028, 0x7031, 0x7039, 0x7041, 0x7049, 0x7052, 0x705A, 0x7062, 0x706A, 0x7073, 0x707B, 0x7083,
    0x708C, 0x7094, 0x709C, 0x70A4, 0x70AD, 0x70B5, 0x70BD, 0x70C6, 0x70CE, 0x70D6, 0x70DF, 0x70E7,
    0x70EF, 0x70F8, 0x7100, 0x7108, 0x7111, 0x7119, 0x7121, 0x712A, 0x7132, 0x713A, 0x7143, 0x714B,
    0x7153, 0x715C, 0x7164, 0x716C, 0x7175, 0x717D, 0x7185, 0x718E, 0x7196, 0x719F, 0x71A7, 0x71AF,
    0x71B8, 0x71C0, 0x71C8, 0x71D1, 0x71D9, 0x71E2, 0x71EA, 0x71F2, 0x71FB, 0x7203, 0x720C, 0x7214,
    0x721C, 0x7225, 0x722D, 0x7236, 0x723E, 0x7246, 0x724F, 0x7257, 0x7260, 0x7268, 0x7271, 0x7279,
    0x7281, 0x728A, 0x7292, 0x729B, 0x72A3, 0x72AC, 0x72B4, 0x72BD, 0x72C5, 0x72CE, 0x72D6, 0x72DE,
    0x72E7, 0x72EF, 0x72F8, 0x7300, 0x7309, 0x7311, 0x731A, 0x7322, 0x732B, 0x7333, 0x733C, 0x7344,
    0x734D, 0x7355, 0x735E, 0x7366, 0x736F, 0x7377, 0x7380, 0x7388, 0x7391, 0x7399, 0x73A2, 0x73AA,
    0x73B3, 0x73BB, 0x73C4, 0x73CC, 0x73D5, 0x73DD, 0x73E6, 0x73EF, 0x73F7, 0x7400, 0x7408, 0x7411,
    0x7419, 0x7422, 0x742A, 0x7433, 0x743C, 0x7444, 0x744D, 0x7455, 0x745E, 0x7466, 0x746F, 0x7478,
    0x7480, 0x7489, 0x7491, 0x749A, 0x74A2, 0x74AB, 0x74B4, 0x74BC, 0x6A7F, 0x6A86, 0x6A8E, 0x6A96,
    0x6A9E, 0x6AA6, 0x6AAE, 0x6AB6, 0x6ABD, 0x6AC5, 0x6ACD, 0x6AD5, 0x6ADD, 0x6AE5, 0x6AED, 0x6AF5,
    0x6AFC, 0x6B04, 0x6B0C, 0x6B14, 0x6B1C, 0x6B24, 0x6B2C, 0x6B34, 0x6B3C, 0x6B43, 0x6B4B, 0x6B53,
    0x6B5B, 0x6B63, 0x6B6B, 0x6B73, 0x6B7B, 0x6B83, 0x6B8B, 0x6B93, 0x6B9B, 0x6BA2, 0x6BAA, 0x6BB2,
    0x6BBA, 0x6BC2, 0x6BCA, 0x6BD2, 0x6BDA, 0x6BE2, 0x6BEA, 0x6BF2, 0x6BFA, 0x6C02, 0x6C0A, 0x6C12,
    0x6C1A, 0x6C22, 0x6C2A, 0x6C32, 0x6C3A, 0x6C42, 0x6C4A, 0x6C52, 0x6C59, 0x6C61, 0x6C69, 0x6C71,
    0x6C79, 0x6C81, 0x6C89, 0x6C91, 0x6C99, 0x6CA1, 0x6CA9, 0x6CB1, 0x6CB9, 0x6CC1, 0x6CC9, 0x6CD2,
    0x6CDA, 0x6CE2, 0x6CEA, 0x6CF2, 0x6CFA, 0x6D02, 0x6D0A, 0x6D12, 0x6D1A, 0x6D22, 0x6D2A, 0x6D32,
    0x6D3A, 0x6D42, 0x6D4A, 0x6D52, 0x6D5A, 0x6D62, 0x6D6A, 0x6D72, 0x6D7A, 0x6D82, 0x6D8B, 0x6D93,
    0x6D9B, 0x6DA3, 0x6DAB, 0x6DB3, 0x6DBB, 0x6DC3, 0x6DCB, 0x6DD3, 0x6DDB, 0x6DE3, 0x6DEC, 0x6DF4,
    0x6DFC, 0x6E04, 0x6E0C, 0x6E14, 0x6E1C, 0x6E24, 0x6E2C, 0x6E35, 0x6E3D, 0x6E45, 0x6E4D, 0x6E55,
    0x6E5D, 0x6E65, 0x6E6D, 0x6E76, 0x6E7E, 0x6E86, 0x6E8E, 0x6E96, 0x6E9E, 0x6EA6, 0x6EAF, 0x6EB7,
    0x6EBF, 0x6EC7, 0x6ECF, 0x6ED7, 0x6EE0, 0x6EE8, 0x6EF0, 0x6EF8, 0x6F00, 0x6F08, 0x6F11, 0x6F19,
    0x6F21, 0x6F29, 0x6F31, 0x6F3A, 0x6F42, 0x6F4A, 0x6F52, 0x6F5A, 0x6F63, 0x6F6B, 0x6F73, 0x6F7B,
    0x65B4, 0x65BB, 0x65C3, 0x65CA, 0x65D2, 0x65D9, 0x65E1, 0x65E8, 0x65F0, 0x65F7, 0x65FF, 0x6606,
    0x660E, 0x6615, 0x661D, 0x6624, 0x662C, 0x6633, 0x663B, 0x6642, 0x664A, 0x6651, 0x6659, 0x6660,
    0x6668, 0x6670, 0x6677, 0x667F, 0x6686, 0x668E, 0x6695, 0x669D, 0x66A4, 0x66AC, 0x66B4, 0x66BB,
    0x66C3, 0x66CA, 0x66D2, 0x66D9, 0x66E1, 0x66E9, 0x66F0, 0x66F8, 0x66FF, 0x6707, 0x670F, 0x6716,
    0x671E, 0x6725, 0x672D, 0x6735, 0x673C, 0x6744, 0x674B, 0x6753, 0x675B, 0x6762, 0x676A, 0x6771,
    0x6779, 0x6781, 0x6788, 0x6790, 0x6798, 0x679F, 0x67A7, 0x67AE, 0x67B6, 0x67BE, 0x67C5, 0x67CD,
    0x67D5, 0x67DC, 0x67E4, 0x67EC, 0x67F3, 0x67FB, 0x6803, 0x680A, 0x6812, 0x681A, 0x6821, 0x6829,
    0x6831, 0x6838, 0x6840, 0x6848, 0x684F, 0x6857, 0x685F, 0x6866, 0x686E, 0x6876, 0x687E, 0x6885,
    0x688D, 0x6895, 0x689C, 0x68A4, 0x68AC, 0x68B4, 0x68BB, 0x68C3, 0x68CB, 0x68D2, 0x68DA, 0x68E2,
    0x68EA, 0x68F1, 0x68F9, 0x6901, 0x6909, 0x6910, 0x6918, 0x6920, 0x6927, 0x692F, 0x6937, 0x693F,
    0x6947, 0x694E, 0x6956, 0x695E, 0x6966, 0x696D, 0x6975, 0x697D, 0x6985, 0x698C, 0x6994, 0x699C,
    0x69A4, 0x69AC, 0x69B3, 0x69BB, 0x69C3, 0x69CB, 0x69D2, 0x69DA, 0x69E2, 0x69EA, 0x69F2, 0x69FA,
    0x6A01, 0x6A09, 0x6A11, 0x6A19, 0x6A21, 0x6A28, 0x6A30, 0x6A38, 0x6A40, 0x6A48, 0x6A50, 0x6A57,
    0x6A5F, 0x6A67, 0x6A6F, 0x6A77, 0x6120, 0x6127, 0x612E, 0x6135, 0x613C, 0x6144, 0x614B, 0x6152,
    0x6159, 0x6160, 0x6167, 0x616F, 0x6176, 0x617D, 0x6184, 0x618B, 0x6192, 0x619A, 0x61A1, 0x61A8,
    0x61AF, 0x61B6, 0x61BE, 0x61C5, 0x61CC, 0x61D3, 0x61DA, 0x61E2, 0x61E9, 0x61F0, 0x61F7, 0x61FF,
    0x6206, 0x620D, 0x6214, 0x621B, 0x6223, 0x622A, 0x6231, 0x6238, 0x6240, 0x6247, 0x624E, 0x6255,
    0x625D, 0x6264, 0x626B, 0x6272, 0x627A, 0x6281, 0x6288, 0x628F, 0x6297, 0x629E, 0x62A5, 0x62AC,
    0x62B4, 0x62BB, 0x62C2, 0x62CA, 0x62D1, 0x62D8, 0x62DF, 0x62E7, 0x62EE, 0x62F5, 0x62FD, 0x6304,
    0x630B, 0x6312, 0x631A, 0x6321, 0x6328, 0x6330, 0x6337, 0x633E, 0x6346, 0x634D, 0x6354, 0x635C,
    0x6363, 0x636A, 0x6372, 0x6379, 0x6380, 0x6388, 0x638F, 0x6396, 0x639E, 0x63A5, 0x63AC, 0x63B4,
    0x63BB, 0x63C2, 0x63CA, 0x63D1, 0x63D8, 0x63E0, 0x63E7, 0x63EE, 0x63F6, 0x63FD, 0x6405, 0x640C,
    0x6413, 0x641B, 0x6422, 0x6429, 0x6431, 0x6438, 0x6440, 0x6447, 0x644E, 0x6456, 0x645D, 0x6464,
    0x646C, 0x6473, 0x647B, 0x6482, 0x648A, 0x6491, 0x6498, 0x64A0, 0x64A7, 0x64AF, 0x64B6, 0x64BD,
    0x64C5, 0x64CC, 0x64D4, 0x64DB, 0x64E3, 0x64EA, 0x64F1, 0x64F9, 0x6500, 0x6508, 0x650F, 0x6517,
    0x651E, 0x6526, 0x652D, 0x6534, 0x653C, 0x6543, 0x654B, 0x6552, 0x655A, 0x6561, 0x6569, 0x6570,
    0x6578, 0x657F, 0x6587, 0x658E, 0x6596, 0x659D, 0x65A5, 0x65AC, 0x5CC1, 0x5CC7, 0x5CCE, 0x5CD5,
    0x5CDC, 0x5CE3, 0x5CEA, 0x5CF1, 0x5CF7, 0x5CFE, 0x5D05, 0x5D0C, 0x5D13, 0x5D1A, 0x5D21, 0x5D27,
    0x5D2E, 0x5D35, 0x5D3C, 0x5D43, 0x5D4A, 0x5D51, 0x5D57, 0x5D5E, 0x5D65, 0x5D6C, 0x5D73, 0x5D7A,
    0x5D81, 0x5D88, 0x5D8F, 0x5D95, 0x5D9C, 0x5DA3, 0x5DAA, 0x5DB1, 0x5DB8, 0x5DBF, 0x5DC6, 0x5DCD,
    0x5DD4, 0x5DDB, 0x5DE1, 0x5DE8, 0x5DEF, 0x5DF6, 0x5DFD, 0x5E04, 0x5E0B, 0x5E12, 0x5E19, 0x5E20,
    0x5E27, 0x5E2E, 0x5E35, 0x5E3C, 0x5E42, 0x5E49, 0x5E50, 0x5E57, 0x5E5E, 0x5E65, 0x5E6C, 0x5E73,
    0x5E7A, 0x5E81, 0x5E88, 0x5E8F, 0x5E96, 0x5E9D, 0x5EA4, 0x5EAB, 0x5EB2, 0x5EB9, 0x5EC0, 0x5EC7,
    0x5ECE, 0x5ED5, 0x5EDC, 0x5EE3, 0x5EEA, 0x5EF1, 0x5EF8, 0x5EFF, 0x5F06, 0x5F0D, 0x5F14, 0x5F1B,
    0x5F22, 0x5F29, 0x5F30, 0x5F37, 0x5F3E, 0x5F45, 0x5F4C, 0x5F53, 0x5F5A, 0x5F61, 0x5F68, 0x5F6F,
    0x5F76, 0x5F7D, 0x5F84, 0x5F8B, 0x5F92, 0x5F99, 0x5FA0, 0x5FA7, 0x5FAE, 0x5FB5, 0x5FBC, 0x5FC4,
    0x5FCB, 0x5FD2, 0x5FD9, 0x5FE0, 0x5FE7, 0x5FEE, 0x5FF5, 0x5FFC, 0x6003, 0x600A, 0x6011, 0x6018,
    0x601F, 0x6027, 0x602E, 0x6035, 0x603C, 0x6043, 0x604A, 0x6051, 0x6058, 0x605F, 0x6066, 0x606D,
    0x6075, 0x607C, 0x6083, 0x608A, 0x6091, 0x6098, 0x609F, 0x60A6, 0x60AD, 0x60B5, 0x60BC, 0x60C3,
    0x60CA, 0x60D1, 0x60D8, 0x60DF, 0x60E7, 0x60EE, 0x60F5, 0x60FC, 0x6103, 0x610A, 0x6111, 0x6119,
    0x5894, 0x589A, 0x58A1, 0x58A8, 0x58AE, 0x58B5, 0x58BB, 0x58C2, 0x58C8, 0x58CF, 0x58D5, 0x58DC,
    0x58E2, 0x58E9, 0x58F0, 0x58F6, 0x58FD, 0x5903, 0x590A, 0x5910, 0x5917, 0x591D, 0x5924, 0x592B,
    0x5931, 0x5938, 0x593E, 0x5945, 0x594B, 0x5952, 0x5959, 0x595F, 0x5966, 0x596C, 0x5973, 0x597A,
    0x5980, 0x5987, 0x598D, 0x5994, 0x599B, 0x59A1, 0x59A8, 0x59AE, 0x59B5, 0x59BC, 0x59C2, 0x59C9,
    0x59CF, 0x59D6, 0x59DD, 0x59E3, 0x59EA, 0x59F1, 0x59F7, 0x59FE, 0x5A04, 0x5A0B, 0x5A12, 0x5A18,
    0x5A1F, 0x5A26, 0x5A2C, 0x5A33, 0x5A3A, 0x5A40, 0x5A47, 0x5A4D, 0x5A54, 0x5A5B, 0x5A61, 0x5A68,
    0x5A6F, 0x5A75, 0x5A7C, 0x5A83, 0x5A89, 0x5A90, 0x5A97, 0x5A9D, 0x5AA4, 0x5AAB, 0x5AB2, 0x5AB8,
    0x5ABF, 0x5AC6, 0x5ACC, 0x5AD3, 0x5ADA, 0x5AE0, 0x5AE7, 0x5AEE, 0x5AF4, 0x5AFB, 0x5B02, 0x5B09,
    0x5B0F, 0x5B16, 0x5B1D, 0x5B23, 0x5B2A, 0x5B31, 0x5B38, 0x5B3E, 0x5B45, 0x5B4C, 0x5B52, 0x5B59,
    0x5B60, 0x5B67, 0x5B6D, 0x5B74, 0x5B7B, 0x5B82, 0x5B88, 0x5B8F, 0x5B96, 0x5B9D, 0x5BA3, 0x5BAA,
    0x5BB1, 0x5BB8, 0x5BBE, 0x5BC5, 0x5BCC, 0x5BD3, 0x5BD9, 0x5BE0, 0x5BE7, 0x5BEE, 0x5BF5, 0x5BFB,
    0x5C02, 0x5C09, 0x5C10, 0x5C16, 0x5C1D, 0x5C24, 0x5C2B, 0x5C32, 0x5C38, 0x5C3F, 0x5C46, 0x5C4D,
    0x5C54, 0x5C5A, 0x5C61, 0x5C68, 0x5C6F, 0x5C76, 0x5C7C, 0x5C83, 0x5C8A, 0x5C91, 0x5C98, 0x5C9F,
    0x5CA5, 0x5CAC, 0x5CB3, 0x5CBA, 0x5497, 0x549E, 0x54A4, 0x54AA, 0x54B0, 0x54B7, 0x54BD, 0x54C3,
    0x54C9, 0x54D0, 0x54D6, 0x54DC, 0x54E2, 0x54E9, 0x54EF, 0x54F5, 0x54FB, 0x5502, 0x5508, 0x550E,
    0x5514, 0x551B, 0x5521, 0x5527, 0x552D, 0x5534, 0x553A, 0x5540, 0x5547, 0x554D, 0x5553, 0x5559,
    0x5560, 0x5566, 0x556C, 0x5573, 0x5579, 0x557F, 0x5585, 0x558C, 0x5592, 0x5598, 0x559F, 0x55A5,
    0x55AB, 0x55B2, 0x55B8, 0x55BE, 0x55C5, 0x55CB, 0x55D1, 0x55D8, 0x55DE, 0x55E4, 0x55EB, 0x55F1,
    0x55F7, 0x55FE, 0x5604, 0x560A, 0x5611, 0x5617, 0x561D, 0x5624, 0x562A, 0x5630, 0x5637, 0x563D,
    0x5643, 0x564A, 0x5650, 0x5656, 0x565D, 0x5663, 0x566A, 0x5670, 0x5676, 0x567D, 0x5683, 0x5689,
    0x5690, 0x5696, 0x569D, 0x56A3, 0x56A9, 0x56B0, 0x56B6, 0x56BC, 0x56C3, 0x56C9, 0x56D0, 0x56D6,
    0x56DC, 0x56E3, 0x56E9, 0x56F0, 0x56F6, 0x56FC, 0x5703, 0x5709, 0x5710, 0x5716, 0x571D, 0x5723,
    0x5729, 0x5730, 0x5736, 0x573D, 0x5743, 0x574A, 0x5750, 0x5756, 0x575D, 0x5763, 0x576A, 0x5770,
    0x5777, 0x577D, 0x5783, 0x578A, 0x5790, 0x5797, 0x579D, 0x57A4, 0x57AA, 0x57B1, 0x57B7, 0x57BE,
    0x57C4, 0x57CB, 0x57D1, 0x57D7, 0x57DE, 0x57E4, 0x57EB, 0x57F1, 0x57F8, 0x57FE, 0x5805, 0x580B,
    0x5812, 0x5818, 0x581F, 0x5825, 0x582C, 0x5832, 0x5839, 0x583F, 0x5846, 0x584C, 0x5853, 0x5859,
    0x5860, 0x5866, 0x586D, 0x5873, 0x587A, 0x5880, 0x5887, 0x588D, 0x50C9, 0x50CF, 0x50D5, 0x50DB,
    0x50E0, 0x50E6, 0x50EC, 0x50F2, 0x50F8, 0x50FE, 0x5104, 0x510A, 0x5110, 0x5116, 0x511C, 0x5122,
    0x5128, 0x512E, 0x5134, 0x513A, 0x5140, 0x5146, 0x514C, 0x5152, 0x5158, 0x515E, 0x5164, 0x516A,
    0x5170, 0x5176, 0x517C, 0x5182, 0x5188, 0x518E, 0x5194, 0x519A, 0x51A0, 0x51A6, 0x51AC, 0x51B2,
    0x51B8, 0x51BE, 0x51C4, 0x51CA, 0x51D0, 0x51D6, 0x51DC, 0x51E2, 0x51E8, 0x51EE, 0x51F4, 0x51FA,
    0x5201, 0x5207, 0x520D, 0x5213, 0x5219, 0x521F, 0x5225, 0x522B, 0x5231, 0x5237, 0x523D, 0x5243,
    0x5249, 0x524F, 0x5255, 0x525B, 0x5261, 0x5267, 0x526E, 0x5274, 0x527A, 0x5280, 0x5286, 0x528C,
    0x5292, 0x5298, 0x529E, 0x52A4, 0x52AA, 0x52B0, 0x52B7, 0x52BD, 0x52C3, 0x52C9, 0x52CF, 0x52D5,
    0x52DB, 0x52E1, 0x52E7, 0x52ED, 0x52F4, 0x52FA, 0x5300, 0x5306, 0x530C, 0x5312, 0x5318, 0x531E,
    0x5325, 0x532B, 0x5331, 0x5337, 0x533D, 0x5343, 0x5349, 0x534F, 0x5356, 0x535C, 0x5362, 0x5368,
    0x536E, 0x5374, 0x537B, 0x5381, 0x5387, 0x538D, 0x5393, 0x5399, 0x539F, 0x53A6, 0x53AC, 0x53B2,
    0x53B8, 0x53BE, 0x53C4, 0x53CB, 0x53D1, 0x53D7, 0x53DD, 0x53E3, 0x53EA, 0x53F0, 0x53F6, 0x53FC,
    0x5402, 0x5408, 0x540F, 0x5415, 0x541B, 0x5421, 0x5427, 0x542E, 0x5434, 0x543A, 0x5440, 0x5446,
    0x544D, 0x5453, 0x5459, 0x545F, 0x5466, 0x546C, 0x5472, 0x5478, 0x547E, 0x5485, 0x548B, 0x5491,
};
#endif

extern u8 salFrame;
extern u8 salAuxFrame;

DSPstudioinfo dspStudio[8];

static u32 dspARAMZeroBuffer = 0;

u16* dspCmdLastLoad = NULL;

u16* dspCmdLastBase = NULL;

u16 dspCmdLastSize = 0;

u16* dspCmdCurBase = NULL;

u16* dspCmdMaxPtr = NULL;

u16* dspCmdPtr = NULL;

u16 dspCmdFirstSize = 0;

u16* dspCmdList = NULL;

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
/* PORT: bool, as dspvoice.h declares it, MusyX's bool is unsigned long, which is u32's size on the console and not here. */
bool dspCompressorOn = 0;
#endif

u32 dspHRTFOn = FALSE;

s16* dspHrtfHistoryBuffer = NULL;

s32* dspSurround = NULL;

s16* dspITDBuffer = NULL;

DSPvoice* dspVoice = NULL;

SND_MESSAGE_CALLBACK salMessageCallback = NULL;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
#define SAL_MALLOC salMalloc
#else
#define SAL_MALLOC salMallocPhysical
#endif

bool salInitDspCtrl(u8 numVoices, u8 numStudios, u32 defaultStudioDPL2) {
  u32 i;         // r31
  u32 j;         // r27
  size_t itdPtr; // r28

  salNumVoices = numVoices;
  salMaxStudioNum = numStudios;

  MUSY_ASSERT(salMaxStudioNum <= SAL_MAX_STUDIONUM);
  dspARAMZeroBuffer = aramGetZeroBuffer();
  if ((dspCmdList = SAL_MALLOC(1024 * sizeof(u16)))) {
    MUSY_DEBUG("Allocated dspCmdList.\n\n");
    if ((dspSurround = SAL_MALLOC(160 * sizeof(s32)))) {
      MUSY_DEBUG("Allocated surround buffer.\n\n");
      memset(dspSurround, 0, 160 * sizeof(s32));
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
      DCFlushRange(dspSurround, 160 * sizeof(long));
#endif
      if ((dspVoice = salMalloc(salNumVoices * sizeof(DSPvoice)))) {
        MUSY_DEBUG("Allocated HW voice array.\n\n");
        if ((dspITDBuffer = SAL_MALLOC(salNumVoices * 64))) {
          MUSY_DEBUG("Allocated ITD buffers for voice array.\n\n");
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
          DCInvalidateRange(dspITDBuffer, salNumVoices * 64);
#endif
          itdPtr = (u32)dspITDBuffer;
          for (i = 0; i < salNumVoices; ++i) {
            MUSY_DEBUG("Initializing voice %d...\n", i);
            dspVoice[i].state = 0;
            dspVoice[i].postBreak = 0;
            dspVoice[i].startupBreak = 0;
            dspVoice[i].lastUpdate.pitch = 0xff;
            dspVoice[i].lastUpdate.vol = 0xff;
            dspVoice[i].lastUpdate.volA = 0xff;
            dspVoice[i].lastUpdate.volB = 0xff;
            dspVoice[i].pb = SAL_MALLOC(sizeof(_PB));
            memset(dspVoice[i].pb, 0, sizeof(_PB));
            dspVoice[i].patchData = SAL_MALLOC(0x80);
            dspVoice[i].pb->currHi = ((u32)dspVoice[i].pb >> 16);
            dspVoice[i].pb->currLo = (u16)dspVoice[i].pb;
            dspVoice[i].pb->update.dataHi = ((u32)dspVoice[i].patchData >> 16);
            dspVoice[i].pb->update.dataLo = ((u16)dspVoice[i].patchData);
            dspVoice[i].pb->itd.bufferHi = ((u32)itdPtr >> 16);
            dspVoice[i].pb->itd.bufferLo = ((u16)itdPtr);
            dspVoice[i].itdBuffer = (void*)itdPtr;
            itdPtr += 0x40;
            dspVoice[i].virtualSampleID = 0xFFFFFFFF;
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
            DCStoreRangeNoSync(dspVoice[i].pb, sizeof(_PB));
#endif
            for (j = 0; j < 5; ++j) {
              dspVoice[i].changed[j] = 0;
            }
          }

          MUSY_DEBUG("All voices initialized.\n\n");

          for (i = 0; i < salMaxStudioNum; ++i) {
            MUSY_DEBUG("Initializing studio %d...\n", i);
            dspStudio[i].state = 0;
            if (!(dspStudio[i].spb = (_SPB*)SAL_MALLOC(sizeof(_SPB)))) {
              return FALSE;
            }

            if (!(dspStudio[i].main[0] = (void*)SAL_MALLOC(0x3c00))) {
              return FALSE;
            }

            memset(dspStudio[i].main[0], 0, 0x3c00);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
            DCFlushRangeNoSync(dspStudio[i].main[0], 0x3c00);
#endif
            dspStudio[i].main[1] = dspStudio[i].main[0] + 0x1e0;
            dspStudio[i].auxA[0] = dspStudio[i].main[1] + 0x1e0;
            dspStudio[i].auxA[1] = dspStudio[i].auxA[0] + 0x1e0;
            dspStudio[i].auxA[2] = dspStudio[i].auxA[1] + 0x1e0;
            dspStudio[i].auxB[0] = dspStudio[i].auxA[2] + 0x1e0;
            dspStudio[i].auxB[1] = dspStudio[i].auxB[0] + 0x1e0;
            dspStudio[i].auxB[2] = dspStudio[i].auxB[1] + 0x1e0;
            memset(dspStudio[i].spb, 0, sizeof(_SPB));
            dspStudio[i].hostDPopSum.l = dspStudio[i].hostDPopSum.r = dspStudio[i].hostDPopSum.s =
                0;
            dspStudio[i].hostDPopSum.lA = dspStudio[i].hostDPopSum.rA =
                dspStudio[i].hostDPopSum.sA = 0;
            dspStudio[i].hostDPopSum.lB = dspStudio[i].hostDPopSum.rB =
                dspStudio[i].hostDPopSum.sB = 0;
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
            DCFlushRangeNoSync(dspStudio[i].spb, sizeof(_SPB));
#endif
          }
          MUSY_DEBUG("All studios are initialized.\n\n");
          salActivateStudio(
              0, 1, defaultStudioDPL2 != FALSE ? SND_STUDIO_TYPE_DPL2 : SND_STUDIO_TYPE_STD);
          MUSY_DEBUG("Default studio is active.\n\n");
          if (!(dspHrtfHistoryBuffer = SAL_MALLOC(0x100))) {
            return FALSE;
          }

          salInitHRTFBuffer();
          return TRUE;
        }
      }
    }
  }

  return FALSE;
}

void salInitHRTFBuffer() {
  memset(dspHrtfHistoryBuffer, 0, 0x100);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCFlushRangeNoSync(dspHrtfHistoryBuffer, 0x100);
#endif
}

bool salExitDspCtrl() {
  u8 i; // r31
  salFree(dspHrtfHistoryBuffer);

  for (i = 0; i < salNumVoices; ++i) {
    salFree(dspVoice[i].pb);
    salFree(dspVoice[i].patchData);
  }

  for (i = 0; i < salMaxStudioNum; ++i) {
    salFree(dspStudio[i].spb);
    salFree(dspStudio[i].main[0]);
  }

  salFree(dspITDBuffer);
  salFree(dspVoice);
  salFree(dspSurround);
  salFree(dspCmdList);
  return TRUE;
}

void salActivateStudio(u8 studio, u32 isMaster, SND_STUDIO_TYPE type) {
  memset(dspStudio[studio].main[0], 0, 0x3c00);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCFlushRangeNoSync(dspStudio[studio].main[0], 0x3c00);
#endif
  memset(dspStudio[studio].spb, 0, sizeof(_SPB));
  dspStudio[studio].hostDPopSum.l = dspStudio[studio].hostDPopSum.r =
      dspStudio[studio].hostDPopSum.s = 0;
  dspStudio[studio].hostDPopSum.lA = dspStudio[studio].hostDPopSum.rA =
      dspStudio[studio].hostDPopSum.sA = 0;
  dspStudio[studio].hostDPopSum.lB = dspStudio[studio].hostDPopSum.rB =
      dspStudio[studio].hostDPopSum.sB = 0;
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCFlushRangeNoSync(dspStudio[studio].spb, sizeof(_SPB));
#endif
  memset(dspStudio[studio].auxA[0], 0, 0x780);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCFlushRangeNoSync(dspStudio[studio].auxA[0], 0x780);
#endif
  memset(dspStudio[studio].auxB[0], 0, 0x780);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCFlushRangeNoSync(dspStudio[studio].auxB[0], 0x780);
#endif
  dspStudio[studio].voiceRoot = NULL;
  dspStudio[studio].alienVoiceRoot = NULL;
  dspStudio[studio].state = 1;
  dspStudio[studio].isMaster = isMaster;
  dspStudio[studio].numInputs = 0;
  dspStudio[studio].type = type;
  dspStudio[studio].auxAHandler = dspStudio[studio].auxBHandler = NULL;
}

static u16 dspSRCCycles[3][3] = {
    {2990, 2990, 1115},
    {3300, 3300, 1115},
    {3700, 3700, 1115},
};

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
static const u16 dspMixerCycles[32] = {
    1470, 2940, 2940, 4410, 2230, 4460, 4460, 6690, 2470, 4940, 4940, 7410, 3735, 7470, 7470, 11205,
    2940, 3386, 2940, 3386, 2940, 3386, 2940, 3386, 4940, 5687, 4940, 5687, 4940, 5687, 4940, 5687,
};
#else
static const u16 dspMixerCyclesMain[16] = {
    0, 760, 760, 1470, 760, 1520, 1520, 2230, 0, 1265, 1265, 2470, 1265, 2530, 2530, 3735,
};
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2) // dropped in the SMS 2.0.3 fork (unused; kept for upstream <= 2.0.2)
static const u16 dspMixerCyclesAux[32] = {
    0, 760, 760, 1470, 0, 1265, 1265, 2470, 760,  1520, 1520, 2230, 760,  2025, 2025, 3230,
    0, 760, 760, 1470, 0, 1265, 1265, 2470, 1265, 2025, 2025, 2735, 1265, 2530, 2530, 3735,
};
#endif
#endif

/* PORT: kept at 2.0.3; hardware.c's hwDeactivateStudio calls it unconditionally. */
#if 1
void salDeactivateStudio(u8 studio) { dspStudio[studio].state = 0; }
#endif

static u32 salCheckVolErrorAndResetDelta(u16* dsp_vol, u16* dsp_delta, u16* last_vol, u16 targetVol,
                                         u16* resetFlags, u16 resetMask) {
  s16 d; // r31
  s16 x; // r30

  if (targetVol != *last_vol) {
    d = (s16)targetVol - (s16)*last_vol;
    if ((s16)d >= 32 && (s16)d < 160) {
      x = (s16)d >> 5;
      if ((s16)x < 5) {
        resetFlags[x] |= resetMask;
      }

      *dsp_delta = 1;
      *last_vol += (x << 5);
      return 1;
    }

    if (-32 >= (s16)d && -160 < (s16)d) {
      x = -(s16)d >> 5;
      if (x < 5) {
        resetFlags[x] |= resetMask;
      }
      *dsp_delta = 0xFFFF;
      *last_vol -= x << 5;
      return 1;
    }

    if (targetVol == 0 && (s16)d > -32) {
      *dsp_vol = *last_vol = 0;
    }
  }

  *dsp_delta = 0;
  return 0;
}

static void sal_setup_dspvol(u16* dsp_delta, u16* last_vol, u16 vol) {
  *dsp_delta = ((s16)vol - (s16)*last_vol) / 160;
  *last_vol += (s16)*dsp_delta * 160;
}

static void sal_update_hostplayinfo(DSPvoice* dsp_vptr) {
  u32 old_lo; // r30
  u32 pitch;  // r31

  if (dsp_vptr->smp_info.loopLength != 0) {
    return;
  }
  if (dsp_vptr->pb->srcSelect != 2) {
    pitch = dsp_vptr->playInfo.pitch << 5;

  } else {
    pitch = 0x200000;
  }

  old_lo = dsp_vptr->playInfo.posLo;
  dsp_vptr->playInfo.posLo += pitch * 0x10000;

  if (old_lo > dsp_vptr->playInfo.posLo) {
    dsp_vptr->playInfo.posHi += (pitch >> 16) + 1;
  } else {
    dsp_vptr->playInfo.posHi += (pitch >> 16);
  }
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 3)
static inline void AddDpop(s32* sum, s16 delta) {
#else
static void AddDpop(s32* sum, s16 delta) {
#endif
  *sum += (int)delta;
  *sum = (*sum > 0x7fffff) ? 0x7fffff : (*sum < -0x7fffff ? -0x7fffff : *sum);
}

static void DoDepopFade(long* dspStart, s16* dspDelta, long* hostSum) {
  if (*hostSum <= -160) {
    *dspDelta = (*hostSum <= -3200) ? 0x14 : (s16)(-*hostSum / 160);
  } else if (*hostSum >= 160) {
    *dspDelta = (*hostSum >= 3200) ? -0x14 : (s16)(-*hostSum / 160);
  } else {
    *dspDelta = 0;
  }

  *dspStart = *hostSum;
  *hostSum += *dspDelta * 160;
}

static void HandleDepopVoice(DSPstudioinfo* stp, DSPvoice* dsp_vptr) {
  _PB* pb; // r31
  dsp_vptr->postBreak = 0;
  dsp_vptr->pb->state = 0;
  pb = dsp_vptr->pb;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  AddDpop(&stp->hostDPopSum.l, pb->dpop.aL);
  AddDpop(&stp->hostDPopSum.r, pb->dpop.aR);

  if ((pb->mixerCtrl & 0x04) != 0) {
    AddDpop(&stp->hostDPopSum.s, pb->dpop.aS);
  }

  if ((pb->mixerCtrl & 0x01) != 0) {
    AddDpop(&stp->hostDPopSum.lA, pb->dpop.aAuxAL);
    AddDpop(&stp->hostDPopSum.rA, pb->dpop.aAuxAR);

    if ((pb->mixerCtrl & 0x14) != 0) {
      AddDpop(&stp->hostDPopSum.sA, pb->dpop.aAuxAS);
    }
  }

  if ((pb->mixerCtrl & 0x12) != 0) {
    AddDpop(&stp->hostDPopSum.lB, pb->dpop.aAuxBL);
    AddDpop(&stp->hostDPopSum.rB, pb->dpop.aAuxBR);

    if ((pb->mixerCtrl & 0x4) != 0) {
      AddDpop(&stp->hostDPopSum.sB, pb->dpop.aAuxBS);
    }
  }
#else
  if ((pb->mixerCtrl & 1) != 0) {
    AddDpop(&stp->hostDPopSum.l, pb->dpop.aL);
  }
  if ((pb->mixerCtrl & 2) != 0) {
    AddDpop(&stp->hostDPopSum.r, pb->dpop.aR);
  }
  if ((pb->mixerCtrl & 4) != 0) {
    AddDpop(&stp->hostDPopSum.s, pb->dpop.aS);
  }
  if ((pb->mixerCtrl & 0x10) != 0) {
    AddDpop(&stp->hostDPopSum.lA, pb->dpop.aAuxAL);
  }
  if ((pb->mixerCtrl & 0x20) != 0) {
    AddDpop(&stp->hostDPopSum.rA, pb->dpop.aAuxAR);
  }
  if ((pb->mixerCtrl & 0x80) != 0) {
    AddDpop(&stp->hostDPopSum.sA, pb->dpop.aAuxAS);
  }
  if ((pb->mixerCtrl & 0x200) != 0) {
    AddDpop(&stp->hostDPopSum.lB, pb->dpop.aAuxBL);
  }
  if ((pb->mixerCtrl & 0x400) != 0) {
    AddDpop(&stp->hostDPopSum.rB, pb->dpop.aAuxBR);
  }
  if ((pb->mixerCtrl & 0x1000) != 0) {
    AddDpop(&stp->hostDPopSum.sB, pb->dpop.aAuxBS);
  }
#endif
}

static void SortVoices(DSPvoice** voices, long l, long r) {
  long i;        // r28
  long last;     // r29
  DSPvoice* tmp; // r27

  if (l >= r) {
    return;
  }

  tmp = voices[l];
  voices[l] = voices[(l + r) / 2];
  voices[(l + r) / 2] = tmp;
  last = l;
  i = l + 1;

  for (; i <= r; ++i) {
    if (voices[i]->prio < voices[l]->prio) {
      last += 1;
      tmp = voices[last];
      voices[last] = voices[i];
      voices[i] = tmp;
    }
  }

  tmp = voices[l];
  voices[l] = voices[last];
  voices[last] = tmp;
  SortVoices(voices, l, last - 1);
  SortVoices(voices, last + 1, r);
}

void salBuildCommandList(s16* dest, u32 nsDelay) {
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  static const u16 pbOffsets[9] = {10, 12, 24, 14, 16, 26, 18, 20, 22};
  static DSPvoice* voices[64];

  u8 s;                                         // r27
  u8 mix_start;                                 // r1+0x17
  u8 st;                                        // r21
  u8 st1;                                       // r1+0x16
  u8 getAuxFrame;                               // r1+0x15
  u16 rampResetOffsetFlags[5];                  // r1+0xE4
  DSPvoice* dsp_vptr;                           // r30
  DSPvoice* next_dsp_vptr;                      // r1+0xE0
  u32 tmp_addr;                                 // r1+0xDC
  u32 addr;                                     // r1+0xD8
  u32 base;                                     // r17
  u32 in;                                       // r1+0xD4
  u32 voiceNum;                                 // r1+0xD0
  u32 cyclesUsed;                               // r24
  u16* pptr;                                    // r28
  u16* pend;                                    // r1+0xCC
  u16 adsr_start;                               // r1+0x34
  u16 adsr_delta;                               // r1+0x32
  u16 old_adsr_delta;                           // r1+0x30
  s32 current_delta;                            // r1+0xC8
  s32 v;                                        // r25
  _PB* pb;                                      // r31
  _PB* last_pb;                                 // r20
  u32 VoiceDone;                                // r1+0xC4
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // TODO
  u32 needsDelta;                               // r19
#endif

  u32 newVoice;       // r1+0xC0
  _SPB* spb;          // r26
  DSPstudioinfo* stp; // r29
#ifdef _DEBUG
  u32 dbgActiveVoices; // r1+0xBC
#endif
  u32 procVoiceFlag; // r1+0xB8
  u32 offset;        // r1+0xB4
  u32 endAddr;       // r1+0xB0
  u32 loopAddr;      // r1+0xAC
  u32 zeroAddr;      // r1+0x98
  DSPvoice* sp78;
  DSPvoice* sp74;

#ifdef _DEBUG
  dbgActiveVoices = 0;
#endif
  dspCmdCurBase = dspCmdPtr = dspCmdList;
  dspCmdMaxPtr = dspCmdPtr + 0xC0;
  dspCmdLastLoad = NULL;
  if (nsDelay < 200) {
    cyclesUsed = 10430;
  } else {
    cyclesUsed = ((nsDelay - 200) * ((__OSBusClock / 400) / 5000)) + 10430;
  }
  if (dspHRTFOn != FALSE) {
    cyclesUsed += 45000;
  }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if (dspCompressorOn != 0) {
    cyclesUsed += 3000;
  }
#endif
  rampResetOffsetFlags[0] = 0;
  for (st = 0; st < salMaxStudioNum; st++) {
    if (dspStudio[st].state == 1) {
      stp = &dspStudio[st];
      for (dsp_vptr = stp->voiceRoot; dsp_vptr; dsp_vptr = next_dsp_vptr) {
        next_dsp_vptr = dsp_vptr->next;
        if ((dsp_vptr->postBreak != 0) || ((dsp_vptr->changed[0] & 0x20) != 0)) {
          HandleDepopVoice(stp, dsp_vptr);
          if (dsp_vptr->virtualSampleID != -1) {
            salSynthSendMessage(dsp_vptr, 3);
          }
          if ((dsp_vptr->state != 1) || (dsp_vptr->startupBreak != 0)) {
            salDeactivateVoice(dsp_vptr);
            dsp_vptr->startupBreak = 0;
          }
        }
      }
      dsp_vptr = stp->alienVoiceRoot;
      while (dsp_vptr) {
        HandleDepopVoice(stp, dsp_vptr);
        dsp_vptr = dsp_vptr->nextAlien;
      }
      stp->alienVoiceRoot = NULL;
      if ((dspCmdPtr + 3) > (dspCmdMaxPtr - 4)) {
        u16 size;          // r1+0x2E
        dspCmdPtr[0] = 13; // MORE
        dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
        dspCmdPtr[2] = (u32)dspCmdMaxPtr;
        size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
        if (dspCmdLastLoad) {
          dspCmdLastLoad[3] = size;
          DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
        } else {
          dspCmdFirstSize = size;
        }
        dspCmdLastLoad = dspCmdPtr;
        dspCmdLastSize = size;
        dspCmdLastBase = dspCmdCurBase;
        dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
        dspCmdMaxPtr = dspCmdPtr + 0xC0;
      }
      dspCmdPtr[0] = 0; // SETUP
      dspCmdPtr[1] = (u32)stp->spb >> 16;
      dspCmdPtr[2] = (u32)stp->spb;
      dspCmdPtr += 3;
      cyclesUsed += 0x2C62;
      for (in = 0; in < stp->numInputs; in++) {
        if ((dspCmdPtr + 6) > (dspCmdMaxPtr - 4)) {
          u16 size;           // r1+0x2C
          dspCmdPtr[0] = 0xD; // MORE
          dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
          dspCmdPtr[2] = (u32)dspCmdMaxPtr;
          size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
          if (dspCmdLastLoad) {
            dspCmdLastLoad[3] = size;
            DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
          } else {
            dspCmdFirstSize = size;
          }
          dspCmdLastLoad = dspCmdPtr;
          dspCmdLastSize = size;
          dspCmdLastBase = dspCmdCurBase;
          dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
          dspCmdMaxPtr = dspCmdPtr + 0xC0;
        }
        dspCmdPtr[0] = 1; // DL_AND_VOL_MIX
        dspCmdPtr[1] = (u32)dspStudio[stp->in[in].studio].main[salFrame ^ 1] >> 16;
        dspCmdPtr[2] = (u32)dspStudio[stp->in[in].studio].main[salFrame ^ 1];
        dspCmdPtr[3] = stp->in[in].vol;
        dspCmdPtr[4] = stp->in[in].volA;
        dspCmdPtr[5] = stp->in[in].volB;
        dspCmdPtr += 6;
        cyclesUsed += 0x294D;
      }
      last_pb = NULL;
      for (v = 0, dsp_vptr = stp->voiceRoot, sp78 = dsp_vptr; dsp_vptr;
           v++, dsp_vptr = dsp_vptr->next, sp74 = dsp_vptr) {
        voices[v] = dsp_vptr;
      }
      voiceNum = (int)v;
      SortVoices(voices, 0, voiceNum - 1);
      procVoiceFlag = 0;
      for (v = voiceNum; v > 0; v--) {
        dsp_vptr = voices[v - 1];
        if (dsp_vptr->state != 0) {
          u8 i; // r1+0x14
          pb = dsp_vptr->pb;
          for (s = 1; s < 5; s++) {
            rampResetOffsetFlags[s] = 0;
          }
          if (dsp_vptr->state == 1) {
            dsp_vptr->virtualSampleID = -1;
            dsp_vptr->pb->ve.currentDelta = 0x8000;
            if (adsrSetup(&dsp_vptr->adsr) != 0) {
              salSynthSendMessage(dsp_vptr, 0);
              salDeactivateVoice(dsp_vptr);
              continue;
            }
            dsp_vptr->virtualSampleID = -1;
            switch (dsp_vptr->smp_info.compType) {
            case 5:
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
            case 6:
#endif
              dsp_vptr->vSampleInfo.loopBufferLength = 0;
              dsp_vptr->virtualSampleID = salSynthSendMessage(dsp_vptr, 2);
              if (dsp_vptr->vSampleInfo.loopBufferLength == 0) {
                salSynthSendMessage(dsp_vptr, 1);
                salDeactivateVoice(dsp_vptr);
                continue;
              }
              break;
            }
            pb->src.currentAddressFrac = 0;
            pb->src.last_samples[0] = 0;
            pb->src.last_samples[1] = 0;
            pb->src.last_samples[2] = 0;
            pb->src.last_samples[3] = 0;
            if ((dsp_vptr->flags & 0x80000000) != 0) {
              memset(dsp_vptr->itdBuffer, 0, 0x40);
              DCFlushRange(dsp_vptr->itdBuffer, 0x40);
              pb->itd.targetShiftL = dsp_vptr->itdShiftL;
              pb->itd.shiftL = dsp_vptr->itdShiftL;
              pb->itd.targetShiftR = dsp_vptr->itdShiftR;
              pb->itd.shiftR = dsp_vptr->itdShiftR;
              pb->itd.flag = 1;
            } else {
              pb->itd.flag = 0;
            }
            switch (dsp_vptr->smp_info.compType) { // MUSYXTODO
            case 0:
            case 4:
            case 5: {
              SNDADPCMinfo* adpcmInfo; // r18
              u8 i;                    // r1+0x13
              pb->addr.format = 0;
              pb->adpcm.gain = 0;
              adpcmInfo = dsp_vptr->smp_info.extraData;
              pb->adpcm.yn2 = 0;
              pb->adpcm.yn1 = 0;
              pb->adpcm.pred_scale = adpcmInfo->initialPS;
              for (i = 0; i < 8; i++) {
                pb->adpcm.a[i][0] = adpcmInfo->coefTab[i][0];
                pb->adpcm.a[i][1] = adpcmInfo->coefTab[i][1];
              }
              base = (u32)dsp_vptr->smp_info.addr * 2;
              addr = base + 2;
              dsp_vptr->playInfo.posHi = dsp_vptr->playInfo.posLo = 0;
              if ((dsp_vptr->smp_info.compType == 4) || (dsp_vptr->smp_info.compType == 5)) {
                pb->loopType = 1;
              } else {
                pb->adpcmLoop.loop_yn2 = adpcmInfo->loopY0;
                pb->adpcmLoop.loop_yn1 = adpcmInfo->loopY1;
                pb->adpcmLoop.loop_pred_scale = adpcmInfo->loopPS;
                pb->loopType = 0;
              }
            } break;
            case 1: {
              DSPADPCMplusInfo* adpcmInfo; // r23
              u8 i;                        // r1+0x12
              pb->addr.format = 0;
              pb->adpcm.gain = 0;
              offset = (dsp_vptr->smp_info.offset + 0xD) / 14;
              adpcmInfo = dsp_vptr->smp_info.extraData;
              pb->adpcm.yn2 = adpcmInfo->blk[offset].Y0;
              pb->adpcm.yn1 = adpcmInfo->blk[offset].Y1;
              pb->adpcm.pred_scale = adpcmInfo->blk[offset].PS;
              pb->adpcmLoop.loop_yn2 = adpcmInfo->loopY0;
              pb->adpcmLoop.loop_yn1 = adpcmInfo->loopY1;
              pb->adpcmLoop.loop_pred_scale = adpcmInfo->loopPS;
              for (i = 0; i < 8; i++) {
                pb->adpcm.a[i][0] = adpcmInfo->coefTab[i][0];
                pb->adpcm.a[i][1] = adpcmInfo->coefTab[i][1];
              }
              base = (u32)dsp_vptr->smp_info.addr * 2;
              addr = base + offset * 16 + 2;
              dsp_vptr->playInfo.posHi = offset * 0xE;
              dsp_vptr->playInfo.posLo = 0;
            } break;
            case 3: {
              u8 i; // r1+0x11
              pb->addr.format = 0x19;
              pb->adpcm.gain = 0x100;
              for (i = 0; i < 8; i++) {
                pb->adpcm.a[i][0] = 0;
                pb->adpcm.a[i][1] = 0;
              }
              addr = (u32)dsp_vptr->smp_info.offset + (base = (u32)dsp_vptr->smp_info.addr);
              dsp_vptr->playInfo.posHi = dsp_vptr->smp_info.offset;
              dsp_vptr->playInfo.posLo = 0;
            } break;
            case 2:
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
            case 6:
#endif
            {
              u8 i; // r1+0x10
              pb->addr.format = 0xA;
              pb->adpcm.gain = 0x800;
              for (i = 0; i < 8; i++) {
                pb->adpcm.a[i][0] = 0;
                pb->adpcm.a[i][1] = 0;
              }
              addr = dsp_vptr->smp_info.offset + (base = (u32)dsp_vptr->smp_info.addr >> 1);
              dsp_vptr->playInfo.posHi = dsp_vptr->smp_info.offset;
              dsp_vptr->playInfo.posLo = 0;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
              if (dsp_vptr->smp_info.compType == 6) {
                pb->loopType = 1;
              }
#endif
            } break;
            default:
              MUSY_ASSERT(FALSE);
              break;
            }
            pb->addr.currentAddressHi = addr >> 0x10;
            pb->addr.currentAddressLo = addr;
            dsp_vptr->currentAddr = addr;
            if (dsp_vptr->smp_info.loopLength != 0) {
              pb->addr.loopFlag = 1;
              switch (dsp_vptr->smp_info.compType) {
              case 0:
              case 1:
              case 4: {
                u32 bn; // r1+0xA8
                u32 bo; // r1+0xA4
                bn = dsp_vptr->smp_info.loop / 14;
                bo = dsp_vptr->smp_info.loop - (bn * 0xE);
                loopAddr = base + bn * 16 + 2 + bo;
                endAddr = dsp_vptr->smp_info.loop + dsp_vptr->smp_info.loopLength - 1;
                bn = endAddr / 14;
                bo = endAddr - (bn * 0xE);
                endAddr = base + bn * 16 + 2 + bo;
              } break;
              case 5: {
                u32 bn; // r1+0xA0
                u32 bo; // r1+0x9C
                loopAddr = ((u32)dsp_vptr->vSampleInfo.loopBufferAddr * 2) + 2;
                endAddr = dsp_vptr->smp_info.loop + dsp_vptr->smp_info.loopLength - 1;
                bn = endAddr / 14;
                bo = endAddr - (bn * 0xE);
                endAddr = base + bn * 16 + 2 + bo;
                dsp_vptr->vSampleInfo.inLoopBuffer = 0;
              } break;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
              case 6: {
                loopAddr = (u32)dsp_vptr->vSampleInfo.loopBufferAddr >> 1;
                endAddr = dsp_vptr->smp_info.loop + dsp_vptr->smp_info.loopLength + base - 1;
                dsp_vptr->vSampleInfo.inLoopBuffer = 0;
              } break;
#endif
              case 2:
              case 3:
              default:
                loopAddr = base + dsp_vptr->smp_info.loop;
                endAddr = base + dsp_vptr->smp_info.loop + dsp_vptr->smp_info.loopLength - 1;
                break;
              }
              pb->addr.loopAddressHi = loopAddr >> 16;
              pb->addr.loopAddressLo = loopAddr;
              pb->addr.endAddressHi = endAddr >> 16;
              pb->addr.endAddressLo = endAddr;
              pb->streamLoopCnt = 0;
            } else {
              pb->addr.loopFlag = 0;
              switch (dsp_vptr->smp_info.compType) {
              case 0:
              case 1:
              case 4:
              case 5: {
                u32 bn;                         // r1+0x94
                u32 bo;                         // r1+0x90
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
                bn = dsp_vptr->smp_info.length / 14;
                bo = dsp_vptr->smp_info.length - (bn * 0xE);
#else
                bn = (dsp_vptr->smp_info.length - 1) / 14;
                bo = (dsp_vptr->smp_info.length - 1) - (bn * 0xE);
#endif
                tmp_addr = base + bn * 16 + 2 + bo;
                zeroAddr = (dspARAMZeroBuffer * 2) + 2;
              } break;
              case 3:
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
                tmp_addr = base + dsp_vptr->smp_info.length;
#else
                tmp_addr = base + dsp_vptr->smp_info.length - 1;
#endif
                zeroAddr = dspARAMZeroBuffer;
                break;
              case 2:
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2) // MUSYXTODO
              case 6:
#endif
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
                tmp_addr = base + dsp_vptr->smp_info.length - 1;
#else
                tmp_addr = base + dsp_vptr->smp_info.length;
#endif
                zeroAddr = dspARAMZeroBuffer >> 1;
                break;
              }
              pb->addr.loopAddressHi = zeroAddr >> 16;
              pb->addr.loopAddressLo = zeroAddr;
              pb->addr.endAddressHi = tmp_addr >> 16;
              pb->addr.endAddressLo = tmp_addr;
            }
            pb->srcSelect = dsp_vptr->srcTypeSelect;
            pb->coefSelect = dsp_vptr->srcCoefSelect;

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
            pb->lpf.flag = dsp_vptr->filter.on ? 1 : 0;
            if (dsp_vptr->filter.on != 0) {
              pb->lpf.a0 = dsp_vptr->filter.coefA;
              pb->lpf.b0 = dsp_vptr->filter.coefB;
              pb->lpf.yn1 = 0;
            }
#endif
            pb->state = (mix_start = dsp_vptr->singleOffset) ? 0 : 1;
            pb->mix.vL = dsp_vptr->lastVolL = dsp_vptr->volL;
            pb->mix.vR = dsp_vptr->lastVolR = dsp_vptr->volR;
            pb->mix.vS = dsp_vptr->lastVolS = dsp_vptr->volS;
            pb->mix.vAuxAL = dsp_vptr->lastVolLa = dsp_vptr->volLa;
            pb->mix.vAuxAR = dsp_vptr->lastVolRa = dsp_vptr->volRa;
            pb->mix.vAuxAS = dsp_vptr->lastVolSa = dsp_vptr->volSa;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
            pb->mixerCtrl = (pb->mix.vAuxAS | (pb->mix.vAuxAL | pb->mix.vAuxAR)) != 0 ? 1 : 0;
#else
            pb->mixerCtrl = 0;
            if ((pb->mix.vL | pb->mix.vR) != 0) {
              pb->mixerCtrl |= 3;
            }
            if ((pb->mix.vAuxAL | pb->mix.vAuxAR) != 0) {
              pb->mixerCtrl |= 0x30;
            }
            if (pb->mix.vAuxAS != 0) {
              pb->mixerCtrl |= 0x80;
            }
#endif
            pb->mix.vAuxBL = dsp_vptr->lastVolLb = dsp_vptr->volLb;
            pb->mix.vAuxBR = dsp_vptr->lastVolRb = dsp_vptr->volRb;
            pb->mix.vAuxBS = dsp_vptr->lastVolSb = dsp_vptr->volSb;
            pb->mix.vDeltaL = 0;
            pb->mix.vDeltaR = 0;
            pb->mix.vDeltaS = 0;
            pb->mix.vDeltaAuxAL = 0;
            pb->mix.vDeltaAuxAR = 0;
            pb->mix.vDeltaAuxAS = 0;
            pb->mix.vDeltaAuxBL = 0;
            pb->mix.vDeltaAuxBR = 0;
            pb->mix.vDeltaAuxBS = 0;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
            if (stp->type == SND_STUDIO_TYPE_STD) {
              if ((pb->mix.vAuxBS | (pb->mix.vAuxBL | pb->mix.vAuxBR)) != 0) {
                pb->mixerCtrl |= 2;
              }
              if ((pb->mix.vAuxBS | (pb->mix.vS | pb->mix.vAuxAS)) != 0) {
                pb->mixerCtrl |= 4;
              }
            } else if ((pb->mix.vAuxAS | (pb->mix.vAuxBL | pb->mix.vAuxBR)) != 0) {
              pb->mixerCtrl |= 0x10;
            }
#else
            if (stp->type == SND_STUDIO_TYPE_STD) {
              if ((pb->mix.vAuxBL | pb->mix.vAuxBR) != 0) {
                pb->mixerCtrl |= 0x600;
              }
              if (pb->mix.vS != 0) {
                pb->mixerCtrl |= 4;
              }
              if (pb->mix.vAuxAS != 0) {
                pb->mixerCtrl |= 0x80;
              }
              if (pb->mix.vAuxBS != 0) {
                pb->mixerCtrl |= 0x1000;
              }
            } else {
              pb->mixerCtrl |= 0x4000;
              if ((pb->mix.vAuxBL | pb->mix.vAuxBR) != 0) {
                pb->mixerCtrl |= 0x600;
              }
              if (pb->mix.vAuxAS != 0) {
                pb->mixerCtrl |= 0x80;
              }
            }
#endif
            dsp_vptr->state = 2;
            newVoice = 1;
            goto block_186;
          }
          if ((dsp_vptr->smp_info.compType == 4) || (dsp_vptr->smp_info.compType == 5)) {
            pb->adpcmLoop.loop_pred_scale = dsp_vptr->streamLoopPS;
            if ((dsp_vptr->smp_info.compType == 5) && (dsp_vptr->vSampleInfo.inLoopBuffer == 0) &&
                (pb->streamLoopCnt != 0)) {
              u32 bn; // r1+0x8C
              u32 bo; // r1+0x88
              bn = (dsp_vptr->vSampleInfo.loopBufferLength - 1) / 14;
              bo = (dsp_vptr->vSampleInfo.loopBufferLength - 1) - (bn * 14);
              tmp_addr = ((u32)dsp_vptr->vSampleInfo.loopBufferAddr * 2) + bn * 16 + 2 + bo;
              dsp_vptr->smp_info.addr = dsp_vptr->vSampleInfo.loopBufferAddr;
              pb->addr.endAddressHi = tmp_addr >> 0x10;
              pb->addr.endAddressLo = tmp_addr;
              dsp_vptr->vSampleInfo.inLoopBuffer = 1;
            }
          }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
          else if (dsp_vptr->smp_info.compType == 6) {
            if ((dsp_vptr->vSampleInfo.inLoopBuffer == 0) && (pb->streamLoopCnt != 0)) {
              tmp_addr = ((u32)dsp_vptr->vSampleInfo.loopBufferAddr >> 1) +
                         (dsp_vptr->vSampleInfo.loopBufferLength - 1);
              dsp_vptr->smp_info.addr = dsp_vptr->vSampleInfo.loopBufferAddr;
              pb->addr.endAddressHi = tmp_addr >> 16;
              pb->addr.endAddressLo = tmp_addr;
              dsp_vptr->vSampleInfo.inLoopBuffer = 1;
            }
          }
#endif
          if ((dsp_vptr->smp_info.loopLength == 0) &&
              (dsp_vptr->playInfo.posHi >= dsp_vptr->smp_info.length)) {
            salSynthSendMessage(dsp_vptr, 0);
            salDeactivateVoice(dsp_vptr);
            continue;
          }
          if (((dsp_vptr->changed[0] & 0x10) != 0) && (adsrSetup(&dsp_vptr->adsr) != 0)) {
            salSynthSendMessage(dsp_vptr, 0);
            salDeactivateVoice(dsp_vptr);
            continue;
          }
          if ((dsp_vptr->changed[0] & 1) != 0) {
            sal_setup_dspvol(&pb->mix.vDeltaL, &dsp_vptr->lastVolL, dsp_vptr->volL);
            sal_setup_dspvol(&pb->mix.vDeltaR, &dsp_vptr->lastVolR, dsp_vptr->volR);
            sal_setup_dspvol(&pb->mix.vDeltaS, &dsp_vptr->lastVolS, dsp_vptr->volS);
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
            needsDelta = 1;
#else
            if ((pb->mix.vDeltaL | pb->mix.vDeltaR) != 0) {
              pb->mixerCtrl |= 0xB;
            } else if ((pb->mix.vL | pb->mix.vR) != 0) {
              pb->mixerCtrl |= 3;
            } else {
              pb->mixerCtrl &= ~3;
            }
            if (stp->type == SND_STUDIO_TYPE_STD) {
              if (pb->mix.vDeltaS != 0) {
                pb->mixerCtrl |= 0xC;
              } else if (pb->mix.vS != 0) {
                pb->mixerCtrl |= 4;
              } else {
                pb->mixerCtrl &= ~4;
              }
            } else {
              pb->mixerCtrl &= ~4;
            }
#endif
          } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
            needsDelta =
                salCheckVolErrorAndResetDelta(&pb->mix.vL, &pb->mix.vDeltaL, &dsp_vptr->lastVolL,
                                              dsp_vptr->volL, rampResetOffsetFlags, 1);
            needsDelta |=
                salCheckVolErrorAndResetDelta(&pb->mix.vR, &pb->mix.vDeltaR, &dsp_vptr->lastVolR,
                                              dsp_vptr->volR, rampResetOffsetFlags, 2);
            needsDelta |=
                salCheckVolErrorAndResetDelta(&pb->mix.vS, &pb->mix.vDeltaS, &dsp_vptr->lastVolS,
                                              dsp_vptr->volS, rampResetOffsetFlags, 4);
#else
            if ((pb->mixerCtrl & 3) != 0) {
              u32 localNeedsDelta; // r1+0x98
              localNeedsDelta =
                  salCheckVolErrorAndResetDelta(&pb->mix.vL, &pb->mix.vDeltaL, &dsp_vptr->lastVolL,
                                                dsp_vptr->volL, rampResetOffsetFlags, 1);
              localNeedsDelta |=
                  salCheckVolErrorAndResetDelta(&pb->mix.vR, &pb->mix.vDeltaR, &dsp_vptr->lastVolR,
                                                dsp_vptr->volR, rampResetOffsetFlags, 2);
              if ((localNeedsDelta | (pb->mix.vL | pb->mix.vR)) == 0) {
                pb->mixerCtrl &= 0xFFFFFFFC;
              } else {
                pb->mixerCtrl |= 8;
              }
            } else {
              pb->mix.vDeltaL = 0;
              pb->mix.vDeltaR = 0;
            }
            if ((pb->mixerCtrl & 4) != 0) {
              if (stp->type == SND_STUDIO_TYPE_STD) {
                u32 localNeedsDelta; // r1+0x94
                localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vS, &pb->mix.vDeltaS,
                                                                &dsp_vptr->lastVolS, dsp_vptr->volS,
                                                                rampResetOffsetFlags, 4);
                if ((pb->mix.vS | localNeedsDelta) == 0) {
                  pb->mixerCtrl &= 0xFFFFFFFB;
                } else {
                  pb->mixerCtrl |= 8;
                }
              } else {
                pb->mixerCtrl &= 0xFFFFFFFB;
              }
            } else {
              pb->mix.vDeltaS = 0;
            }
#endif
          }
          if ((dsp_vptr->changed[0] & 2) != 0) {
            sal_setup_dspvol(&pb->mix.vDeltaAuxAL, &dsp_vptr->lastVolLa, dsp_vptr->volLa);
            sal_setup_dspvol(&pb->mix.vDeltaAuxAR, &dsp_vptr->lastVolRa, dsp_vptr->volRa);
            sal_setup_dspvol(&pb->mix.vDeltaAuxAS, &dsp_vptr->lastVolSa, dsp_vptr->volSa);

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
            if ((pb->mix.vDeltaAuxAS | (pb->mix.vDeltaAuxAL | pb->mix.vDeltaAuxAR)) != 0) {
              pb->mixerCtrl |= 1;
              needsDelta = 1;
            } else if ((pb->mix.vAuxAS | (pb->mix.vAuxAL | pb->mix.vAuxAR)) != 0) {
              pb->mixerCtrl |= 1;
            } else {
              pb->mixerCtrl &= ~1;
            }
#else
            if ((pb->mix.vDeltaAuxAL | pb->mix.vDeltaAuxAR) != 0) {
              pb->mixerCtrl |= 0x70;
            } else if ((pb->mix.vAuxAL | pb->mix.vAuxAR) != 0) {
              pb->mixerCtrl |= 0x30;
            } else {
              pb->mixerCtrl &= ~0x30;
            }
            if (pb->mix.vDeltaAuxAS != 0) {
              pb->mixerCtrl |= 0x180;
            } else if (pb->mix.vAuxAS != 0) {
              pb->mixerCtrl |= 0x80;
            } else {
              pb->mixerCtrl &= ~0x80;
            }
#endif
          }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
          else if ((pb->mixerCtrl & 1) != 0) {
            u32 localNeedsDelta; // r1+0x84
            localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxAL, &pb->mix.vDeltaAuxAL,
                                                            &dsp_vptr->lastVolLa, dsp_vptr->volLa,
                                                            rampResetOffsetFlags, 8);
            localNeedsDelta |= salCheckVolErrorAndResetDelta(&pb->mix.vAuxAR, &pb->mix.vDeltaAuxAR,
                                                             &dsp_vptr->lastVolRa, dsp_vptr->volRa,
                                                             rampResetOffsetFlags, 0x10);
            localNeedsDelta |= salCheckVolErrorAndResetDelta(&pb->mix.vAuxAS, &pb->mix.vDeltaAuxAS,
                                                             &dsp_vptr->lastVolSa, dsp_vptr->volSa,
                                                             rampResetOffsetFlags, 0x20);
            if ((localNeedsDelta | (pb->mix.vAuxAS | (pb->mix.vAuxAL | pb->mix.vAuxAR))) == 0) {
              pb->mixerCtrl &= ~1;
            } else {
              needsDelta = 1;
            }
          } else {
            pb->mix.vDeltaAuxAL = 0;
            pb->mix.vDeltaAuxAR = 0;
            pb->mix.vDeltaAuxAS = 0;
          }
#else
          else {
            if ((pb->mixerCtrl & 0x30) != 0) {
              u32 localNeedsDelta; // r1+0x90
              localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxAL, &pb->mix.vDeltaAuxAL,
                                                              &dsp_vptr->lastVolLa, dsp_vptr->volLa,
                                                              rampResetOffsetFlags, 8);
              localNeedsDelta |= salCheckVolErrorAndResetDelta(
                  &pb->mix.vAuxAR, &pb->mix.vDeltaAuxAR, &dsp_vptr->lastVolRa, dsp_vptr->volRa,
                  rampResetOffsetFlags, 0x10);
              if ((localNeedsDelta | (pb->mix.vAuxAL | pb->mix.vAuxAR)) == 0) {
                pb->mixerCtrl &= ~0x30;
              } else {
                pb->mixerCtrl |= 0x40;
              }
            } else {
              pb->mix.vDeltaAuxAL = 0;
              pb->mix.vDeltaAuxAR = 0;
            }
            if ((pb->mixerCtrl & 0x80) != 0) {
              u32 localNeedsDelta; // r1+0x8C
              localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxAS, &pb->mix.vDeltaAuxAS,
                                                              &dsp_vptr->lastVolSa, dsp_vptr->volSa,
                                                              rampResetOffsetFlags, 0x20);
              if ((pb->mix.vAuxAS | localNeedsDelta) == 0) {
                pb->mixerCtrl &= ~0x80;
              } else {
                pb->mixerCtrl |= 0x100;
              }
            } else {
              pb->mix.vDeltaAuxAS = 0;
            }
          }
#endif
          if ((dsp_vptr->changed[0] & 4) != 0) {
            if (stp->type == SND_STUDIO_TYPE_STD) {
              sal_setup_dspvol(&pb->mix.vDeltaAuxBL, &dsp_vptr->lastVolLb, dsp_vptr->volLb);
              sal_setup_dspvol(&pb->mix.vDeltaAuxBR, &dsp_vptr->lastVolRb, dsp_vptr->volRb);
              sal_setup_dspvol(&pb->mix.vDeltaAuxBS, &dsp_vptr->lastVolSb, dsp_vptr->volSb);

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
              if ((pb->mix.vDeltaAuxBS | (pb->mix.vDeltaAuxBL | pb->mix.vDeltaAuxBR)) != 0) {
                pb->mixerCtrl |= 2;
                needsDelta = 1;
              } else if ((pb->mix.vAuxBS | (pb->mix.vAuxBL | pb->mix.vAuxBR)) != 0) {
                pb->mixerCtrl |= 2;
              } else {
                pb->mixerCtrl &= ~2;
              }
#else
              if ((pb->mix.vDeltaAuxBL | pb->mix.vDeltaAuxBR) != 0) {
                pb->mixerCtrl |= 0xE00;
              } else if ((pb->mix.vAuxBL | pb->mix.vAuxBR) != 0) {
                pb->mixerCtrl |= 0x600;
              } else {
                pb->mixerCtrl &= ~0x600;
              }
              if (pb->mix.vDeltaAuxBS != 0) {
                pb->mixerCtrl |= 0x3000;
              } else if (pb->mix.vAuxBS != 0) {
                pb->mixerCtrl |= 0x1000;
              } else {
                pb->mixerCtrl &= ~0x1000;
              }
#endif
            } else {
              sal_setup_dspvol(&pb->mix.vDeltaAuxBL, &dsp_vptr->lastVolLb, dsp_vptr->volLb);
              sal_setup_dspvol(&pb->mix.vDeltaAuxBR, &dsp_vptr->lastVolRb, dsp_vptr->volRb);
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
              if ((pb->mix.vDeltaAuxBL | pb->mix.vDeltaAuxBR) != 0) {
                pb->mixerCtrl |= 0x10;
                needsDelta = 1;
              } else if ((pb->mix.vDeltaAuxAS |
                          (pb->mix.vAuxAS | (pb->mix.vAuxBL | pb->mix.vAuxBR))) != 0) {
                pb->mixerCtrl |= 0x10;
              } else {
                pb->mixerCtrl &= ~0x10;
              }
#else
              if ((pb->mix.vDeltaAuxBL | pb->mix.vDeltaAuxBR) != 0) {
                pb->mixerCtrl |= 0x4E00;
              } else if ((pb->mix.vDeltaAuxAS |
                          (pb->mix.vAuxAS | (pb->mix.vAuxBL | pb->mix.vAuxBR))) != 0) {
                pb->mixerCtrl |= 0x4600;
              } else {
                pb->mixerCtrl &= ~0x4600;
              }
#endif
            }
          } else if (stp->type == SND_STUDIO_TYPE_STD) {
            if ((pb->mixerCtrl & (MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) ? 2 : 0x600)) != 0) {
              u32 localNeedsDelta; // r1+0x80
              localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxBL, &pb->mix.vDeltaAuxBL,
                                                              &dsp_vptr->lastVolLb, dsp_vptr->volLb,
                                                              rampResetOffsetFlags, 0x40);
              localNeedsDelta |= salCheckVolErrorAndResetDelta(
                  &pb->mix.vAuxBR, &pb->mix.vDeltaAuxBR, &dsp_vptr->lastVolRb, dsp_vptr->volRb,
                  rampResetOffsetFlags, 0x80);
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
              localNeedsDelta |= salCheckVolErrorAndResetDelta(
                  &pb->mix.vAuxBS, &pb->mix.vDeltaAuxBS, &dsp_vptr->lastVolSb, dsp_vptr->volSb,
                  rampResetOffsetFlags, 0x100);
              if ((localNeedsDelta | (pb->mix.vAuxBS | (pb->mix.vAuxBL | pb->mix.vAuxBR))) == 0) {
                pb->mixerCtrl &= ~2;
              } else {
                needsDelta = 1;
              }
#else
              if ((localNeedsDelta | (pb->mix.vAuxBL | pb->mix.vAuxBR)) == 0) {
                pb->mixerCtrl &= ~0x600;
              } else {
                pb->mixerCtrl |= 0x800;
              }
#endif
            } else {
              pb->mix.vDeltaAuxBL = 0;
              pb->mix.vDeltaAuxBR = 0;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
              pb->mix.vDeltaAuxBS = 0;
#endif
            }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
            if ((pb->mixerCtrl & 0x1000) != 0) {
              u32 localNeedsDelta; // r1+0x84
              localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxBS, &pb->mix.vDeltaAuxBS,
                                                              &dsp_vptr->lastVolSb, dsp_vptr->volSb,
                                                              rampResetOffsetFlags, 0x100);
              if ((pb->mix.vAuxBS | localNeedsDelta) == 0) {
                pb->mixerCtrl &= ~0x1000;
              } else {
                pb->mixerCtrl |= 0x2000;
              }
            } else {
              pb->mix.vDeltaAuxBS = 0;
            }
#endif
          } else if ((pb->mixerCtrl &
                      (MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) ? 0x10 : 0x4600)) != 0) {
            u32 localNeedsDelta; // r1+0x7C
            localNeedsDelta = salCheckVolErrorAndResetDelta(&pb->mix.vAuxBL, &pb->mix.vDeltaAuxBL,
                                                            &dsp_vptr->lastVolLb, dsp_vptr->volLb,
                                                            rampResetOffsetFlags, 0x40);
            localNeedsDelta |= salCheckVolErrorAndResetDelta(&pb->mix.vAuxBR, &pb->mix.vDeltaAuxBR,
                                                             &dsp_vptr->lastVolRb, dsp_vptr->volRb,
                                                             rampResetOffsetFlags, 0x80);
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
            if ((localNeedsDelta | (pb->mix.vAuxBL | pb->mix.vAuxBR)) == 0) {
              if ((pb->mix.vAuxAS | pb->mix.vDeltaAuxAS) == 0) {
                pb->mixerCtrl &= ~0x10;
              }
            } else {
              needsDelta = 1;
            }
#else
            if ((localNeedsDelta | (pb->mix.vAuxBL | pb->mix.vAuxBR)) == 0) {
              pb->mixerCtrl &= ~0x4600;
            } else {
              pb->mixerCtrl |= 0x800;
            }
#endif
          } else {
            pb->mix.vDeltaAuxBL = 0;
            pb->mix.vDeltaAuxBR = 0;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
            if ((pb->mix.vAuxAS | pb->mix.vDeltaAuxAS) != 0) {
              pb->mixerCtrl |= 0x10;
            }
#else
            pb->mixerCtrl |= 0x4600;
#endif
          }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
          if (needsDelta != 0) {
            pb->mixerCtrl |= 8;
          } else {
            pb->mixerCtrl &= ~8;
          }
          if (stp->type == SND_STUDIO_TYPE_STD) {
            if ((pb->mix.vS != 0) || (pb->mix.vDeltaS != 0) || (pb->mix.vAuxAS != 0) ||
                (pb->mix.vDeltaAuxAS != 0) || (pb->mix.vAuxBS != 0) || (pb->mix.vDeltaAuxBS != 0)) {
              pb->mixerCtrl |= 4;
            } else {
              pb->mixerCtrl &= ~4;
            }
          }
#endif
          if ((dsp_vptr->changed[0] & 0x200) != 0) {
            pb->itd.targetShiftL = dsp_vptr->itdShiftL;
            pb->itd.targetShiftR = dsp_vptr->itdShiftR;
          }
          if ((dsp_vptr->changed[0] & 0x100) != 0) {
            pb->srcSelect = dsp_vptr->srcTypeSelect;
          }
          if ((dsp_vptr->changed[0] & 0x80) != 0) {
            pb->coefSelect = dsp_vptr->srcCoefSelect;
          }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
          if ((dsp_vptr->changed[0] & 0x400) != 0) {
            pb->lpf.flag = dsp_vptr->filter.on ? 1 : 0;
            pb->lpf.yn1 = 0;
          }
          if (((dsp_vptr->changed[0] & 0x800) != 0) && dsp_vptr->filter.on) {
            pb->lpf.a0 = dsp_vptr->filter.coefA;
            pb->lpf.b0 = dsp_vptr->filter.coefB;
          }
#endif
          mix_start = 0;
          newVoice = 0;
          dsp_vptr->currentAddr = (pb->addr.currentAddressHi << 0x10) | pb->addr.currentAddressLo;
        block_186:
          if ((dsp_vptr->changed[mix_start] & 0x40) != 0) {
            adsrRelease(&dsp_vptr->adsr);
          }
          if ((dsp_vptr->changed[mix_start] & 8) != 0) {
            pb->src.ratioHi = dsp_vptr->pitch[mix_start] >> 0x10;
            pb->src.ratioLo = dsp_vptr->pitch[mix_start];
            dsp_vptr->playInfo.pitch = dsp_vptr->pitch[mix_start];
          }
          VoiceDone = adsrHandle(&dsp_vptr->adsr, &pb->ve.currentVolume, &pb->ve.currentDelta);
          old_adsr_delta = pb->ve.currentDelta;
          for (s = 0; s < 5; s++) {
            pb->update.updNum[s] = 0;
          }
          pptr = dsp_vptr->patchData;
          pend = (u16*)((u32)dsp_vptr->patchData + 0x80);
          if (mix_start != 0) {
            MUSY_ASSERT((pptr + 2) <= pend);
            pptr[0] = 7;
            pptr[1] = 1;
            pptr += 2;
            pb->update.updNum[mix_start]++;
          }
          sal_update_hostplayinfo(dsp_vptr);
          for (s = mix_start + 1; s < 5; s++) {
            if (VoiceDone != 0) {
              MUSY_ASSERT((pptr + 2) <= pend);
              pptr[0] = 7;
              pptr[1] = 0;
              pptr += 2;
              pb->update.updNum[s]++;
              salSynthSendMessage(dsp_vptr, 0);
              salDeactivateVoice(dsp_vptr);
              break;
            } else {
              if (rampResetOffsetFlags[s] != 0) {
                for (i = 0; i < 9; i++) {
                  if (((1 << i) & rampResetOffsetFlags[s]) != 0) {
                    MUSY_ASSERT((pptr + 2) <= pend);
                    pptr[0] = pbOffsets[i];
                    pptr[1] = 0;
                    pptr += 2;
                    pb->update.updNum[s]++;
                  }
                }
              }
              if ((dsp_vptr->changed[s] & 0x20) != 0) {
                adsrStartRelease(&dsp_vptr->adsr, 10);
                dsp_vptr->postBreak = 1;
              } else if (dsp_vptr->postBreak == 0) {
                if ((dsp_vptr->changed[s] & 0x40) != 0) {
                  adsrRelease(&dsp_vptr->adsr);
                }
                if ((dsp_vptr->changed[s] & 8) != 0) {
                  MUSY_ASSERT((pptr + 4) <= pend);
                  pptr[0] = 0x53;
                  pptr[1] = dsp_vptr->pitch[s] >> 16;
                  pptr[2] = 0x54;
                  pptr[3] = dsp_vptr->pitch[s];
                  pptr += 4;
                  pb->update.updNum[s] += 2;
                  dsp_vptr->playInfo.pitch = dsp_vptr->pitch[s];
                }
              }
              current_delta = dsp_vptr->adsr.currentDelta;
              VoiceDone = adsrHandle(&dsp_vptr->adsr, &adsr_start, &adsr_delta);
              if (old_adsr_delta == adsr_delta) {
                if (current_delta != 0) {
                  MUSY_ASSERT((pptr + 2) <= pend);
                  pptr[0] = 0x32;
                  pptr[1] = adsr_start;
                  pptr += 2;
                  pb->update.updNum[s]++;
                }
              } else {
                MUSY_ASSERT((pptr + 4) <= pend);
                pptr[0] = 0x32;
                pptr[1] = adsr_start;
                pptr[2] = 0x33;
                pptr[3] = adsr_delta;
                pptr += 4;
                pb->update.updNum[s] += 2;
                old_adsr_delta = adsr_delta;
              }
              sal_update_hostplayinfo(dsp_vptr);
            }
          }
          if (VoiceDone != 0) {
            salSynthSendMessage(dsp_vptr, 0);
            salDeactivateVoice(dsp_vptr);
          }
          DCStoreRangeNoSync(dsp_vptr->patchData, (u32)pptr - (u32)dsp_vptr->patchData);
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) // MUSYXTODO
          cyclesUsed += dspMixerCycles[pb->mixerCtrl] + 0x4FE;
#else
          cyclesUsed += 0x4FE;
          cyclesUsed += dspMixerCyclesMain[(u16)(pb->mixerCtrl & 0xF)];
          cyclesUsed += dspMixerCyclesMain[(u16)((pb->mixerCtrl >> 4) & 0x1F)];
          cyclesUsed += dspMixerCyclesMain[(u16)((pb->mixerCtrl >> 9) & 0x1F)];
          if (pb->lpf.flag == 1) {
            cyclesUsed += 0x229;
          }
#endif
          switch (pb->src.ratioHi) {
          case 0:
          case 1:
            cyclesUsed += dspSRCCycles[pb->src.ratioHi][pb->srcSelect];
            break;
          default:
            cyclesUsed += dspSRCCycles[2][pb->srcSelect];
            break;
          }
          for (s = 0; s < 5; s++) {
            cyclesUsed += pb->update.updNum[s] * 4;
          }
          if (cyclesUsed > (__OSBusClock / 400)) {
            if ((newVoice == 0) && (VoiceDone == 0)) {
              HandleDepopVoice(stp, dsp_vptr);
            }
            salDeactivateVoice(dsp_vptr);
            salSynthSendMessage(dsp_vptr, 1);
            for (v = v - 1; v > 0; v--) {
              if (voices[v - 1]->state == 2) {
                HandleDepopVoice(stp, voices[v - 1]);
              }
              salDeactivateVoice(voices[v - 1]);
              salSynthSendMessage(voices[v - 1], 1);
            }
            for (st1 = st + 1; st1 < salMaxStudioNum; st1++) {
              if (dspStudio[st1].state == 1) {
                for (dsp_vptr = dspStudio[st1].voiceRoot; dsp_vptr; dsp_vptr = next_dsp_vptr) {
                  next_dsp_vptr = dsp_vptr->next;
                  if (dsp_vptr->state == 2) {
                    HandleDepopVoice(&dspStudio[st1], dsp_vptr);
                  }
                  salDeactivateVoice(dsp_vptr);
                  salSynthSendMessage(dsp_vptr, 1);
                }
              }
            }
            break;
          } else {
            if (!last_pb) {
              if ((dspCmdPtr + 3) > (dspCmdMaxPtr - 4)) {
                u16 size;          // r1+0x2A
                dspCmdPtr[0] = 13; // MORE
                dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
                dspCmdPtr[2] = (u32)dspCmdMaxPtr;
                size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
                if (dspCmdLastLoad) {
                  dspCmdLastLoad[3] = size;
                  DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
                } else {
                  dspCmdFirstSize = size;
                }
                dspCmdLastLoad = dspCmdPtr;
                dspCmdLastSize = size;
                dspCmdLastBase = dspCmdCurBase;
                dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
                dspCmdMaxPtr = dspCmdPtr + 0xC0;
              }
              dspCmdPtr[0] = 2; // PB_ADDR
              dspCmdPtr[1] = (u32)pb >> 0x10;
              dspCmdPtr[2] = (u32)pb;
              dspCmdPtr += 3; // PROCESS
#ifdef _DEBUG
              dbgActiveVoices++;
#endif
              procVoiceFlag = 1;
            } else {
              last_pb->nextHi = (u32)pb >> 16;
              last_pb->nextLo = (u32)pb;
#ifdef _DEBUG
              dbgActiveVoices++;
#endif
              procVoiceFlag = 1;
              DCFlushRangeNoSync(last_pb, sizeof(_PB));
            }
            last_pb = pb;
          }
        }
      }
      if (procVoiceFlag != 0) {
        if ((dspCmdPtr + 1) > (dspCmdMaxPtr - 4)) {
          u16 size;          // r1+0x28;
          dspCmdPtr[0] = 13; // MORE
          dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
          dspCmdPtr[2] = (u32)dspCmdMaxPtr;
          size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
          if (dspCmdLastLoad) {
            dspCmdLastLoad[3] = size;
            DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
          } else {
            dspCmdFirstSize = size;
          }
          dspCmdLastLoad = dspCmdPtr;
          dspCmdLastSize = size;
          dspCmdLastBase = dspCmdCurBase;
          dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
          dspCmdMaxPtr = dspCmdPtr + 0xC0;
        }
        *dspCmdPtr++ = 3; // PROCESS
      }
      if (last_pb) {
        last_pb->nextHi = 0;
        last_pb->nextLo = 0;
        DCFlushRangeNoSync(last_pb, sizeof(_PB));
      }
      getAuxFrame = (salAuxFrame + 1) % 3;
      if (stp->auxAHandler) {
        if ((dspCmdPtr + 5) > (dspCmdMaxPtr - 4)) {
          u16 size;          // r1+0x26
          dspCmdPtr[0] = 13; // MORE
          dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
          dspCmdPtr[2] = (u32)dspCmdMaxPtr;
          size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
          if (dspCmdLastLoad) {
            dspCmdLastLoad[3] = size;
            DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
          } else {
            dspCmdFirstSize = size;
          }
          dspCmdLastLoad = dspCmdPtr;
          dspCmdLastSize = size;
          dspCmdLastBase = dspCmdCurBase;
          dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
          dspCmdMaxPtr = dspCmdPtr + 0xC0;
        }
        dspCmdPtr[0] = 4; // MIX_AUXA
        dspCmdPtr[1] = (u32)stp->auxA[salAuxFrame] >> 16;
        dspCmdPtr[2] = (u32)stp->auxA[salAuxFrame];
        dspCmdPtr[3] = (u32)stp->auxA[getAuxFrame] >> 16;
        dspCmdPtr[4] = (u32)stp->auxA[getAuxFrame];
        dspCmdPtr += 5;
      }
      if (stp->type == SND_STUDIO_TYPE_STD) {
        if (stp->auxBHandler) {
          if ((dspCmdPtr + 5) > (dspCmdMaxPtr - 4)) {
            u16 size;          // r1+0x24
            dspCmdPtr[0] = 13; // MORE
            dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
            dspCmdPtr[2] = (u32)dspCmdMaxPtr;
            size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
            if (dspCmdLastLoad) {
              dspCmdLastLoad[3] = size;
              DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
            } else {
              dspCmdFirstSize = size;
            }
            dspCmdLastLoad = dspCmdPtr;
            dspCmdLastSize = size;
            dspCmdLastBase = dspCmdCurBase;
            dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
            dspCmdMaxPtr = dspCmdPtr + 0xC0;
          }
          dspCmdPtr[0] = 5; // MIX_AUXB
          dspCmdPtr[1] = (u32)stp->auxB[salAuxFrame] >> 16;
          dspCmdPtr[2] = (u32)stp->auxB[salAuxFrame];
          dspCmdPtr[3] = (u32)stp->auxB[getAuxFrame] >> 16;
          dspCmdPtr[4] = (u32)stp->auxB[getAuxFrame];
          dspCmdPtr += 5;
        }
      } else {
        if ((dspCmdPtr + 5) > (dspCmdMaxPtr - 4)) {
          u16 size;          // r1+0x22
          dspCmdPtr[0] = 13; // MORE
          dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
          dspCmdPtr[2] = (u32)dspCmdMaxPtr;
          size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
          if (dspCmdLastLoad) {
            dspCmdLastLoad[3] = size;
            DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
          } else {
            dspCmdFirstSize = size;
          }
          dspCmdLastLoad = dspCmdPtr;
          dspCmdLastSize = size;
          dspCmdLastBase = dspCmdCurBase;
          dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
          dspCmdMaxPtr = dspCmdPtr + 0xC0;
        }
        dspCmdPtr[0] = 16; // MIX_AUXB_LR
        dspCmdPtr[1] = (u32)stp->auxB[salFrame] >> 16;
        dspCmdPtr[2] = (u32)stp->auxB[salFrame];
        dspCmdPtr[3] = (u32)stp->auxB[salFrame ^ 1] >> 16;
        dspCmdPtr[4] = (u32)stp->auxB[salFrame ^ 1];
        dspCmdPtr += 5;
      }
      if ((dspCmdPtr + 3) > (dspCmdMaxPtr - 4)) {
        u16 size;          // r1+0x20
        dspCmdPtr[0] = 13; // MORE
        dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
        dspCmdPtr[2] = (u32)dspCmdMaxPtr;
        size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
        if (dspCmdLastLoad) {
          dspCmdLastLoad[3] = size;
          DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
        } else {
          dspCmdFirstSize = size;
        }
        dspCmdLastLoad = dspCmdPtr;
        dspCmdLastSize = size;
        dspCmdLastBase = dspCmdCurBase;
        dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
        dspCmdMaxPtr = dspCmdPtr + 0xC0;
      }
      dspCmdPtr[0] = 6; // UPLOAD_LRS
      dspCmdPtr[1] = (u32)stp->main[salFrame] >> 16;
      dspCmdPtr[2] = (u32)stp->main[salFrame];
      dspCmdPtr += 3;
      spb = stp->spb;
      DoDepopFade((long*)&spb->dpopLHi, (s16*)&spb->dpopLDelta, &stp->hostDPopSum.l);
      DoDepopFade((long*)&spb->dpopRHi, (s16*)&spb->dpopRDelta, &stp->hostDPopSum.r);
      DoDepopFade((long*)&spb->dpopSHi, (s16*)&spb->dpopSDelta, &stp->hostDPopSum.s);
      DoDepopFade((long*)&spb->dpopALHi, (s16*)&spb->dpopALDelta, &stp->hostDPopSum.lA);
      DoDepopFade((long*)&spb->dpopARHi, (s16*)&spb->dpopARDelta, &stp->hostDPopSum.rA);
      DoDepopFade((long*)&spb->dpopASHi, (s16*)&spb->dpopASDelta, &stp->hostDPopSum.sA);
      DoDepopFade((long*)&spb->dpopBLHi, (s16*)&spb->dpopBLDelta, &stp->hostDPopSum.lB);
      DoDepopFade((long*)&spb->dpopBRHi, (s16*)&spb->dpopBRDelta, &stp->hostDPopSum.rB);
      DoDepopFade((long*)&spb->dpopBSHi, (s16*)&spb->dpopBSDelta, &stp->hostDPopSum.sB);
      DCFlushRangeNoSync(spb, sizeof(_SPB));
    }
  }
  if ((dspCmdPtr + 3) > (dspCmdMaxPtr - 4)) {
    u16 size;          // r1+0x1E
    dspCmdPtr[0] = 13; // MORE
    dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
    dspCmdPtr[2] = (u32)dspCmdMaxPtr;
    size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
    if (dspCmdLastLoad) {
      dspCmdLastLoad[3] = size;
      DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
    } else {
      dspCmdFirstSize = size;
    }
    dspCmdLastLoad = dspCmdPtr;
    dspCmdLastSize = size;
    dspCmdLastBase = dspCmdCurBase;
    dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
    dspCmdMaxPtr = dspCmdPtr + 0xC0;
  }
  dspCmdPtr[0] = 17; // SET_OPPOSITE_LR
  dspCmdPtr[1] = (u32)dspSurround >> 16;
  dspCmdPtr[2] = (u32)dspSurround;
  dspCmdPtr += 3;
  for (st = 0; st < salMaxStudioNum; st++) {
    if ((dspStudio[st].state == 1) && (dspStudio[st].isMaster != 0)) {
      if ((dspCmdPtr + 3) > (dspCmdMaxPtr - 4)) {
        u16 size;          // r1+0x1C
        dspCmdPtr[0] = 13; // MORE
        dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
        dspCmdPtr[2] = (u32)dspCmdMaxPtr;
        size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
        if (dspCmdLastLoad) {
          dspCmdLastLoad[3] = size;
          DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
        } else {
          dspCmdFirstSize = size;
        }
        dspCmdLastLoad = dspCmdPtr;
        dspCmdLastSize = size;
        dspCmdLastBase = dspCmdCurBase;
        dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
        dspCmdMaxPtr = dspCmdPtr + 0xC0;
      }
      dspCmdPtr[0] = 9; // MIX_AUXB_NOWRITE
      dspCmdPtr[1] = (u32)dspStudio[st].main[salFrame] >> 16;
      dspCmdPtr[2] = (u32)dspStudio[st].main[salFrame];
      dspCmdPtr += 3;
    }
  }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) // MUSYXTODO
  if (dspCompressorOn != 0) {
    if ((dspCmdPtr + 5) > (dspCmdMaxPtr - 4)) {
      u16 size;
      dspCmdPtr[0] = 0xD;
      dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
      dspCmdPtr[2] = (u32)dspCmdMaxPtr;
      size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
      if (dspCmdLastLoad) {
        dspCmdLastLoad[3] = size;
        DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
      } else {
        dspCmdFirstSize = size;
      }
      dspCmdLastLoad = dspCmdPtr;
      dspCmdLastSize = size;
      dspCmdLastBase = dspCmdCurBase;
      dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
      dspCmdMaxPtr += 0xC0;
    }
    dspCmdPtr[0] = 18;
    dspCmdPtr[1] = 0x8000;
    dspCmdPtr[2] = 10;
    dspCmdPtr[3] = (u32)compressorTable >> 16;
    dspCmdPtr[4] = (u32)compressorTable;
    dspCmdPtr += 5;
  }
#endif
  if ((dspCmdPtr + 5) > (dspCmdMaxPtr - 4)) {
    u16 size;          // r1+0x1A
    dspCmdPtr[0] = 13; // MORE
    dspCmdPtr[1] = (u32)dspCmdMaxPtr >> 16;
    dspCmdPtr[2] = (u32)dspCmdMaxPtr;
    size = (((u32)(dspCmdPtr + 4) - (u32)dspCmdCurBase) + 3) & ~3;
    if (dspCmdLastLoad) {
      dspCmdLastLoad[3] = size;
      DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
    } else {
      dspCmdFirstSize = size;
    }
    dspCmdLastLoad = dspCmdPtr;
    dspCmdLastSize = size;
    dspCmdLastBase = dspCmdCurBase;
    dspCmdCurBase = dspCmdPtr = dspCmdMaxPtr;
    dspCmdMaxPtr = dspCmdPtr + 0xC0;
  }
  {
    u16 size;          // r1+0x18
    dspCmdPtr[0] = 14; // OUTPUT
    dspCmdPtr[1] = (u32)dspSurround >> 16;
    dspCmdPtr[2] = (u32)dspSurround;
    dspCmdPtr[3] = (u32)dest >> 16;
    dspCmdPtr[4] = (u32)dest;
    dspCmdPtr += 5;
    *dspCmdPtr++ = 15; // END
    size = (((u32)dspCmdPtr - (u32)dspCmdCurBase) + 3) & ~3;
    if (dspCmdLastLoad) {
      dspCmdLastLoad[3] = size;
      DCStoreRangeNoSync(dspCmdLastBase, dspCmdLastSize);
    } else {
      dspCmdFirstSize = size;
    }
  }
  DCStoreRangeNoSync(dspCmdCurBase, (u32)dspCmdPtr - (u32)dspCmdCurBase);
#ifdef _DEBUG
  if (dbgActiveVoices > dbgActiveVoicesMax) {
    dbgActiveVoicesMax = dbgActiveVoices;
  }
#endif
#else
  // TODO implement for PC
#endif
}

u32 salSynthSendMessage(DSPvoice* dsp_vptr, u32 mesg) {
  return salMessageCallback == NULL ? FALSE
                                    : salMessageCallback(mesg, dsp_vptr->mesgCallBackUserValue);
}

void salActivateVoice(DSPvoice* dsp_vptr, u8 studio) {
  if (dsp_vptr->state != 0) {
    salDeactivateVoice(dsp_vptr);
    dsp_vptr->changed[0] |= 0x20;
  }

  dsp_vptr->postBreak = 0;
  if ((dsp_vptr->next = dspStudio[studio].voiceRoot) != NULL) {
    dsp_vptr->next->prev = dsp_vptr;
  }

  dsp_vptr->prev = NULL;
  dspStudio[studio].voiceRoot = dsp_vptr;
  dsp_vptr->startupBreak = 0;
  dsp_vptr->state = 1;
  dsp_vptr->studio = studio;
}

void salDeactivateVoice(DSPvoice* dsp_vptr) {
  if (dsp_vptr->state == 0) {
    return;
  }

  if (dsp_vptr->prev != NULL) {
    dsp_vptr->prev->next = dsp_vptr->next;
  } else {
    dspStudio[dsp_vptr->studio].voiceRoot = dsp_vptr->next;
  }

  if (dsp_vptr->next != NULL) {
    dsp_vptr->next->prev = dsp_vptr->prev;
  }

  dsp_vptr->state = 0;
}

/* PORT: kept at 2.0.3; hwChangeStudio, hwAddInput and hwRemoveInput call these
   unconditionally. */
#if 1
void salReconnectVoice(DSPvoice* dsp_vptr, u8 studio) {
  if (dsp_vptr->state != 0) {
    if (dsp_vptr->prev != NULL) {
      dsp_vptr->prev->next = dsp_vptr->next;
    } else {
      dspStudio[dsp_vptr->studio].voiceRoot = dsp_vptr->next;
    }

    if (dsp_vptr->next != NULL) {
      dsp_vptr->next->prev = dsp_vptr->prev;
    }

    if ((dsp_vptr->next = dspStudio[studio].voiceRoot) != NULL) {
      dsp_vptr->next->prev = dsp_vptr;
    }

    dsp_vptr->prev = NULL;
    dspStudio[studio].voiceRoot = dsp_vptr;
    if (dsp_vptr->state == 2) {
      dsp_vptr->nextAlien = dspStudio[dsp_vptr->studio].alienVoiceRoot;
      dspStudio[dsp_vptr->studio].alienVoiceRoot = dsp_vptr;
    }
  }

  dsp_vptr->studio = studio;
}

bool salAddStudioInput(DSPstudioinfo* stp, SND_STUDIO_INPUT* desc) {
  if (stp->numInputs < 7) {
    stp->in[stp->numInputs].studio = desc->srcStudio;
    stp->in[stp->numInputs].vol = ((u16)desc->vol << 8) | ((u16)desc->vol << 1);
    stp->in[stp->numInputs].volA = ((u16)desc->volA << 8) | ((u16)desc->volA << 1);
    stp->in[stp->numInputs].volB = ((u16)desc->volB << 8) | ((u16)desc->volB << 1);
    stp->in[stp->numInputs].desc = desc;
    ++stp->numInputs;
    return 1;
  }

  return 0;
}

bool salRemoveStudioInput(DSPstudioinfo* stp, SND_STUDIO_INPUT* desc) {
  long i; // r31

  for (i = 0; i < stp->numInputs; ++i) {
    if (stp->in[i].desc == desc) {
      for (; i <= stp->numInputs - 2; ++i) {
        stp->in[i] = stp->in[i + 1];
      }
      --stp->numInputs;
      return 1;
    }
  }

  return 0;
}
#endif

void salHandleAuxProcessing() {
  DSPstudioinfo* r28;
  u8 st;             // r29
  s32* work;         // r30
  DSPstudioinfo* sp; // r31
  SND_AUX_INFO info; // r1+0x8

  for (sp = dspStudio, st = 0; st < salMaxStudioNum; ++st, r28 = sp++, r28 = r28) {

    if (sp->state != 1) {
      continue;
    }

    if (sp->auxAHandler != NULL) {
      work = sp->auxA[(salAuxFrame + 2) % 3];
      info.data.bufferUpdate.left = work;
      info.data.bufferUpdate.right = work + 0xa0;
      info.data.bufferUpdate.surround = work + 0x140;
      sp->auxAHandler(0, &info, sp->auxAUser);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
      DCFlushRangeNoSync(work, 0x780);
#endif
    }

    if (sp->type == 0 && sp->auxBHandler != 0) {
      work = sp->auxB[(salAuxFrame + 2) % 3];
      info.data.bufferUpdate.left = work;
      info.data.bufferUpdate.right = work + 0xa0;
      info.data.bufferUpdate.surround = work + 0x140;
      sp->auxBHandler(0, &info, sp->auxBUser);
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
      DCFlushRangeNoSync(work, 0x780);
#endif
    }
  }
}
