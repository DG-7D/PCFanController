#include <avr/interrupt.h>
#include <avr/io.h>

constexpr uint8_t POWER_STEP = 10;
constexpr uint16_t MIN_RPM = 120;
constexpr float LED_BRIGHTNESS = 0.5;

constexpr uint32_t PWM_FREQ = 25000;           // ticks/s
constexpr uint8_t LED_TICKS_PER_DIGIT = 64;    // 1桁391Hz、4桁98Hz
constexpr uint8_t BUTTON_DEBOUNCE_TICKS = 250; // てきとー 0.01s

constexpr uint16_t PWM_PERIOD_CLOCKS = F_CPU / PWM_FREQ;
constexpr uint16_t LED_ENABLE_CLOCKS = PWM_PERIOD_CLOCKS * LED_BRIGHTNESS;
constexpr uint32_t CLK_TICKS_PER_MIN = F_CPU / PWM_PERIOD_CLOCKS * 60;
constexpr uint32_t TIMEOUT_TICKS = CLK_TICKS_PER_MIN / MIN_RPM / 2;
constexpr uint16_t POW10[] = {1, 10, 100, 1000};

enum MODE : uint8_t {
    MODE_OFF,
    MODE_RPM,
    MODE_POWER,
    MODE_COUNT,
    MODE_STARTUP,
};

volatile uint16_t pulseWidth = 0;
volatile bool ledUpdateFlag = true;
volatile uint8_t power = 100;
volatile MODE mode = MODE_RPM;
volatile uint8_t segCharBuffer[4] = {};

int main() {
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, ~CLKCTRL_PEN_bm);

    PORTA.DIRSET = PIN1_bm | PIN3_bm | PIN4_bm;
    PORTA.DIRCLR = PIN5_bm | PIN6_bm | PIN7_bm;
    PORTA.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
    PORTA.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
    PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;

    PORTB.DIRSET = PIN1_bm | PIN2_bm | PIN3_bm;
    PORTB.DIRCLR = PIN0_bm;
    PORTB.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;

    PORTMUX.CTRLC = PORTMUX_TCA00_ALTERNATE_gc;
    TCA0.SINGLE.CMP0 = LED_ENABLE_CLOCKS;
    TCA0.SINGLE.CMP1 = PWM_PERIOD_CLOCKS;
    TCA0.SINGLE.PER = PWM_PERIOD_CLOCKS - 1;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_CMP1EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;

    SPI0.CTRLB = ~SPI_BUFEN_bm | SPI_SSD_bm | SPI_MODE_0_gc;
    SPI0.CTRLA = SPI_DORD_bm | SPI_MASTER_bm | ~SPI_CLK2X_bm | SPI_PRESC_DIV16_gc | SPI_ENABLE_bm;

    USART0.BAUD = 64 * F_CPU / 16 / 9600;
    USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
    USART0.CTRLB = USART_TXEN_bm | USART_RXMODE_NORMAL_gc;

    sei();

    set595(false, 0, 0);
    while (buttonDebounceTicks == 0) {
        ;
    }

    uint16_t rpm = 0;
    uint8_t digit = 3;
    while (1) {
        if (mode == MODE_OFF) {
            TCA0.SINGLE.CMP0 = PWM_PERIOD_CLOCKS;
            continue;
        } else {
            TCA0.SINGLE.CMP0 = LED_ENABLE_CLOCKS;
        }
        TCA0.SINGLE.CMP1 = PWM_PERIOD_CLOCKS * power / 100;

        if (ledUpdateFlag) {
            ledUpdateFlag = false;

            set595(true, digit, segCharBuffer[digit]);

            sendUART(segCharBuffer[digit]);
            if (digit == 0) {
                sendUART('\n');
            }

            if (digit == 0) {
                switch (mode) {
                case MODE_RPM:
                    cli();
                    const uint16_t ticks = pulseWidth;
                    sei();
                    if (ticks == 0) {
                        rpm = 0;
                    } else {
                        rpm = CLK_TICKS_PER_MIN / ticks / 2;
                    }
                    for (uint8_t i = 0; i < 4; i++) {
                        segCharBuffer[i] = '0' + (rpm / POW10[i] % 10);
                    }
                    break;

                case MODE_POWER:
                    for (uint8_t i = 0; i < 3; i++) {
                        segCharBuffer[i] = '0' + (power / POW10[i] % 10);
                    }
                    segCharBuffer[3] = LED_7SEG_CHAR_BITS[' '];
                    break;
                }
            }

            digit = (digit + 4 - 1) % 4;
                }
            }
        }

volatile uint16_t tick = 0;
volatile uint16_t lastPulseTick = 0;
volatile uint8_t buttonDebounceTicks = 0;
volatile uint8_t lastButtonState = 0;

ISR(PORTA_PORT_vect) {
    PORTA.INTFLAGS = PIN5_bm | PIN6_bm | PIN7_bm;
    lastButtonState = PORTA.IN & (PIN5_bm | PIN6_bm | PIN7_bm);
    buttonDebounceTicks = BUTTON_DEBOUNCE_TICKS;
}

ISR(PORTB_PORT_vect) {
    PORTB.INTFLAGS = PIN0_bm;
    const uint16_t now = tick;
    pulseWidth = now - lastPulseTick;
    lastPulseTick = now;
}

ISR(TCA0_OVF_vect) {
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
    tick++;
    if (tick % LED_TICKS_PER_DIGIT == 0) {
        ledUpdateFlag = true;
    }
    if (tick == lastPulseTick + TIMEOUT_TICKS) {
        pulseWidth = 0;
    }
    if (buttonDebounceTicks > 0) {
        buttonDebounceTicks--;

        if (buttonDebounceTicks == 0 && (PORTA.IN & (PIN5_bm | PIN6_bm | PIN7_bm)) == lastButtonState) {
            if (!(lastButtonState & PIN5_bm)) {
                if (power <= 100 - POWER_STEP) {
                    power = power + POWER_STEP;
                } else {
                    power = 100;
                }
            } else if (!(lastButtonState & PIN6_bm)) {
                if (POWER_STEP <= power) {
                    power = power - POWER_STEP;
                } else {
                    power = 0;
                }
            } else if (!(lastButtonState & PIN7_bm)) {
                if (mode != MODE_COUNT - 1) {
                    mode = (MODE)(mode + 1);
                } else {
                    mode = MODE_OFF;
                }
            }
        }
    }
}

static inline void sendSPI(uint8_t data) {
    SPI0.DATA = data;
    while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
        ;
    }
}

static inline void sendUART(uint8_t data) {
    while (!(USART0.STATUS & USART_DREIF_bm)) {
        ;
    }
    USART0.TXDATAL = data;
}

static inline void set595(bool fanEnable, uint8_t digit, uint8_t character) {
    PORTA.OUTCLR = PIN4_bm;
    sendSPI(1 << digit | (fanEnable ? 0 : FAN_SW_BIT));
    sendSPI(LED_7SEG_CHAR_BITS[character]);
    PORTA.OUTSET = PIN4_bm;
}
constexpr const uint8_t LED_7SEG_CHAR_BITS[] = {
    //        GCPDEFAB
    ['0'] = 0b01011111,
    ['1'] = 0b01000001,
    ['2'] = 0b10011011,
    ['3'] = 0b01011011,
    ['4'] = 0b11000101,
    ['5'] = 0b11010110,
    ['6'] = 0b11011101,
    ['7'] = 0b01000011,
    ['8'] = 0b11011111,
    ['9'] = 0b11011011,
    //        GCPDEFAB
    [' '] = 0b00000000,
    ['P'] = 0b10001111,
    ['U'] = 0b01011101,
    ['S'] = 0b11010110,
    ['H'] = 0b11001101,
};
constexpr uint8_t LED_7SEG_DP_BIT = 0b00100000;
constexpr uint8_t FAN_SW_BIT = 0b01000000; // Lowで駆動
