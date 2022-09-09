
#include "utility.h"
#include "esp32/rom/crc.h"


uint8_t crc8(uint8_t const *buf, uint32_t len) {
    uint8_t tmp = 0;
    tmp = ~crc8_be((uint8_t) ~0x00, buf, len);
    return tmp;

}

uint32_t getchipId() {
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t *) (&_chipmacid));
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) {
        chipId |= ((_chipmacid >> (40 - i)) & 0xff) << i;
    }
    return chipId;
}
