#include "core/powerSave.h"
#include "core/utils.h"
#include <Wire.h>
#include <interface.h>

// -- DEBOUNCE TIME (ms) for buttons, to avoid multiple triggers on a single press
#define BUTTON_DEBOUNCE_MS 100
uint32_t LastNextPress = 0;
uint32_t LastPrevPress = 0;
uint32_t LastUpPress = 0;
uint32_t LastDownPress = 0;
uint32_t LastSelPress = 0;
uint32_t LastEscPress = 0;

static bool lastUpState = false;
static bool lastDownState = false;
static bool lastPrevState = false;
static bool lastNextState = false;
static bool lastSelState = false;
static bool lastEscState = false;
/***************************************************************************************
** Function name: _setup_gpio()
** Description:   Initial hardware setup called before tft.init()
***************************************************************************************/
void _setup_gpio() {
    // Start I2C: shared bus for OLED (0x3C) + PN532 (0x24) + Si5351 (0x60)
    Wire.begin(GROVE_SDA, GROVE_SCL);
    // Fast-mode I2C (400kHz): speeds up OLED rendering. All devices on the
    // bus (SSD1306, PN532, Si5351) support 400kHz.
    Wire.setClock(400000);

    // Hold SPI chip-select pins high to avoid bus contention
    pinMode(CC1101_SS_PIN, OUTPUT);
    digitalWrite(CC1101_SS_PIN, HIGH);
    pinMode(NRF24_SS_PIN, OUTPUT);
    digitalWrite(NRF24_SS_PIN, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);

    // Buttons: active-LOW, internal pull-up
    pinMode(SEL_BTN, INPUT_PULLUP);
    pinMode(UP_BTN, INPUT_PULLUP);
    pinMode(DW_BTN, INPUT_PULLUP);
    pinMode(L_BTN, INPUT_PULLUP);
    pinMode(R_BTN, INPUT_PULLUP);
    pinMode(ESC_BTN, INPUT_PULLUP);

    // Mark CC1101 as the default RF module
    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.irRx = RXLED;
}

/***************************************************************************************
** Function name: getBattery()
** Description:   No battery present, always return 0
***************************************************************************************/
int getBattery() { return 0; }

/***************************************************************************************
** Function name: isCharging()
***************************************************************************************/
bool isCharging() { return false; }

/***************************************************************************************
** Function name: _setBrightness()
** Description:   OLED has no built-in brightness control
***************************************************************************************/
void _setBrightness(uint8_t brightval) { (void)brightval; }

/***************************************************************************************
** Function name: InputHandler()
** Description:   6 buttons: UP, DOWN, LEFT, RIGHT, OK(SEL), BACK(ESC)
**                Active-LOW - all buttons wired to GND, internal pull-up enabled
***************************************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 150 && !LongPress) return;

    bool _up = digitalRead(UP_BTN) == BTN_ACT;
    bool _dn = digitalRead(DW_BTN) == BTN_ACT;
    bool _l = digitalRead(L_BTN) == BTN_ACT;
    bool _r = digitalRead(R_BTN) == BTN_ACT;
    bool _sel = digitalRead(SEL_BTN) == BTN_ACT;
    bool _esc = digitalRead(ESC_BTN) == BTN_ACT;

    bool anyPressed = _up || _dn || _l || _r || _sel || _esc;

    if (anyPressed) {
        tm = millis();
        if (wakeUpScreen()) return;
        AnyKeyPress = true;
    }

    // Ce bloc tourne à CHAQUE appel, appui ou pas, pour ne jamais rester bloqué
    if (_up && !lastUpState && (millis() - LastUpPress > BUTTON_DEBOUNCE_MS)) {
        UpPress = true;
        PrevPagePress = true;
    }
    if (_up != lastUpState) LastUpPress = millis();
    lastUpState = _up;

    if (_dn && !lastDownState && (millis() - LastDownPress > BUTTON_DEBOUNCE_MS)) {
        DownPress = true;
        NextPagePress = true;
    }
    if (_dn != lastDownState) LastDownPress = millis();
    lastDownState = _dn;

    if (_l && !lastPrevState && (millis() - LastPrevPress > BUTTON_DEBOUNCE_MS)) { PrevPress = true; }
    if (_l != lastPrevState) LastPrevPress = millis();
    lastPrevState = _l;

    if (_r && !lastNextState && (millis() - LastNextPress > BUTTON_DEBOUNCE_MS)) { NextPress = true; }
    if (_r != lastNextState) LastNextPress = millis();
    lastNextState = _r;

    if (_sel && !lastSelState && (millis() - LastSelPress > BUTTON_DEBOUNCE_MS)) { SelPress = true; }
    if (_sel != lastSelState) LastSelPress = millis();
    lastSelState = _sel;

    if (_esc && !lastEscState && (millis() - LastEscPress > BUTTON_DEBOUNCE_MS)) { EscPress = true; }
    if (_esc != lastEscState) LastEscPress = millis();
    lastEscState = _esc;
}

/***************************************************************************************
** Function name: powerOff()
** Description:   Deep sleep, wake up with the OK button
***************************************************************************************/
void powerOff() {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SEL_BTN, LOW);
    esp_deep_sleep_start();
}

/***************************************************************************************
** Function name: checkReboot()
** Description:   Holding OK + BACK together for 3 seconds restarts the device
***************************************************************************************/
void checkReboot() {
    if (digitalRead(SEL_BTN) != BTN_ACT || digitalRead(ESC_BTN) != BTN_ACT) return;

    uint32_t holdStart = millis();
    int countDown = 0;

    while (digitalRead(SEL_BTN) == BTN_ACT && digitalRead(ESC_BTN) == BTN_ACT) {
        uint32_t elapsed = millis() - holdStart;
        if (elapsed > 500) {
            int step = elapsed / 1000 + 1;
            if (step != countDown) {
                countDown = step;
                tft.setTextSize(1);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.drawCentreString("OFF IN " + String(4 - countDown) + "...", tftWidth / 2, 10, 1);
            }
        }
        if (elapsed >= 3000) {
            tft.fillScreen(bruceConfig.bgColor);
            while (digitalRead(SEL_BTN) == BTN_ACT || digitalRead(ESC_BTN) == BTN_ACT);
            delay(100);
            powerOff();
            return;
        }
        delay(20);
    }
    // Clear the countdown text if the buttons were released
    if (countDown > 0) {
        tft.fillRect(0, 5, tftWidth, 18, bruceConfig.bgColor);
        drawStatusBar();
    }
}
