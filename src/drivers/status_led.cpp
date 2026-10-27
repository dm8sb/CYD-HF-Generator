#include "status_led.h"
#include <pins.h>

namespace StatusLed {

void begin() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    showPage(PageId::Main);
}

void showPage(PageId page) {
    digitalWrite(PIN_LED_R, page == PageId::CH1 ? LOW : HIGH);
    digitalWrite(PIN_LED_G, page == PageId::CH2 ? LOW : HIGH);
    digitalWrite(PIN_LED_B, page == PageId::CH3 ? LOW : HIGH);
}

}
