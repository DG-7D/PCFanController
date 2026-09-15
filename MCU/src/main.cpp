#include <avr/interrupt.h>
#include <avr/io.h>

constexpr uint8_t POWER_STEP = 10;

constexpr uint32_t PWM_FREQ = 25000;
constexpr uint16_t MIN_RPM = 120;

constexpr uint16_t PWM_PERIOD_CLOCKS = F_CPU / PWM_FREQ;
constexpr uint16_t LED_ENABLE_CLOCKS = PWM_PERIOD_CLOCKS / 2;
constexpr uint32_t CLK_TICKS_PER_MIN = F_CPU / PWM_PERIOD_CLOCKS * 60;
constexpr uint32_t TIMEOUT_TICKS = CLK_TICKS_PER_MIN / MIN_RPM / 2;
constexpr uint8_t LED_TICKS_PER_DIGIT = 64; // 1桁391Hz、4桁98Hz
constexpr uint16_t BUTTON_DEBOUNCE_TICKS = 2500; // てきとー 0.1s

enum MODE : uint8_t {
    MODE_OFF,
    MODE_RPM,
    MODE_POWER,
    MODE_COUNT,
};

constexpr uint8_t LED_7SEG_NUM_BITS[] = {
    // GFEDCBA
    0b00111111,
    0b00000110,
    0b01011011,
    0b00011111,
    0b01100110,
    0b01101101,
    0b01111110,
    0b00000111,
    0b01111111,
    0b01011111,
    0b00000000,
};
constexpr uint8_t LED_7SEG_DP_BIT = 0b10000000;
constexpr uint16_t POW10[] = {1, 10, 100, 1000};

volatile uint16_t time = 0;
volatile uint16_t lastPulse = 0;
volatile uint16_t pulseWidth = 0;
volatile bool ledUpdateFlag = true;
volatile uint16_t buttonDebounceTicks = 0;

volatile uint8_t power = 100;
volatile MODE mode = MODE_RPM;

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
    uint16_t rpm = 0;
    uint8_t digit = 0;
    uint8_t nextDigit = 3;
    while (1) {
        if (mode == MODE_OFF) {
            continue;
        }
        if (ledUpdateFlag) {
            ledUpdateFlag = false;

            switch (mode) {
            case MODE_RPM:
                if (nextDigit == 0) {
                    cli();
                    const uint16_t ticks = pulseWidth;
                    sei();
                    if (ticks == 0) {
                        rpm = 0;
                    } else {
                        rpm = CLK_TICKS_PER_MIN / ticks / 2;
                    }
                }
                digit = rpm / POW10[nextDigit] % 10;
                break;

            case MODE_POWER:
                if (nextDigit == 3) {
                    digit = 10; // 明るさ変わらないよう虚無を表示
                } else {
                    digit = power / POW10[nextDigit] % 10;
                }
                break;
            }

            PORTA.OUTCLR = PIN4_bm;
            sendSPI(1 << nextDigit);
            sendSPI(LED_7SEG_NUM_BITS[digit]);
            PORTA.OUTSET = PIN4_bm;

            sendUART('0' + digit);
            if (nextDigit == 0) {
                sendUART('\n');
            }

            nextDigit = (nextDigit + 4 - 1) % 4;
        }
    }
}

ISR(PORTA_PORT_vect) {
    if (buttonDebounceTicks > 0) {
        PORTA.INTFLAGS = PIN5_bm | PIN6_bm | PIN7_bm;
        return;
    }
    buttonDebounceTicks = BUTTON_DEBOUNCE_TICKS;

    if (PORTA.INTFLAGS & PIN5_bm) {
        PORTA.INTFLAGS = PIN5_bm;
        power = power + POWER_STEP;
        if (power > 100) {
            power = 100;
        }
    } else if (PORTA.INTFLAGS & PIN6_bm) {
        PORTA.INTFLAGS = PIN6_bm;
        power = power - POWER_STEP;
        if (power > 100) {
            power = 0;
        }
    } else if (PORTA.INTFLAGS & PIN7_bm) {
        PORTA.INTFLAGS = PIN7_bm;
        mode = (MODE)(mode + 1);
        if (mode == MODE_COUNT) {
            mode = MODE_OFF;
            TCA0.SINGLE.CMP0 = PWM_PERIOD_CLOCKS;
        } else {
            TCA0.SINGLE.CMP0 = LED_ENABLE_CLOCKS;
        }
    }
    TCA0.SINGLE.CMP1 = PWM_PERIOD_CLOCKS * power / 100;
}

ISR(PORTB_PORT_vect) {
    PORTB.INTFLAGS = PIN0_bm;
    const uint16_t now = time;
    pulseWidth = now - lastPulse;
    lastPulse = now;
}

ISR(TCA0_OVF_vect) {
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
    time++;
    if (time % LED_TICKS_PER_DIGIT == 0) {
        ledUpdateFlag = true;
    }
    if (time == lastPulse + TIMEOUT_TICKS) {
        pulseWidth = 0;
    }
    if (buttonDebounceTicks > 0) {
        buttonDebounceTicks--;
    }
}