#include <avr/interrupt.h>
#include <avr/io.h>

constexpr uint32_t PWM_FREQ = 25000;
constexpr uint16_t MIN_RPM = 120;

constexpr uint16_t PWM_PERIOD_CLOCKS = F_CPU / PWM_FREQ;
constexpr uint32_t CLK_TICKS_PER_MIN = F_CPU / PWM_PERIOD_CLOCKS * 60;
constexpr uint32_t TIMEOUT_TICKS = CLK_TICKS_PER_MIN / MIN_RPM / 2;
constexpr uint8_t LED_TICKS_PER_DIGIT = 64; // 1桁391Hz、4桁98Hz

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
};
constexpr uint8_t LED_7SEG_DP_BIT = 0b10000000;

volatile uint16_t time = 0;
volatile uint16_t lastPulse = 0;
volatile uint16_t pulseWidth = 0;
volatile bool ledUpdateFlag = true;
uint8_t nextDigit = 0;

int main() {
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, ~CLKCTRL_PEN_bm);

    PORTA.DIRSET = PIN1_bm | PIN3_bm | PIN4_bm;

    PORTB.DIRSET = PIN0_bm;
    PORTB.DIRCLR = PIN1_bm;
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;

    TCA0.SINGLE.CMP0 = PWM_PERIOD_CLOCKS;
    TCA0.SINGLE.PER = PWM_PERIOD_CLOCKS - 1;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;

    SPI0.CTRLB = ~SPI_BUFEN_bm | SPI_SSD_bm | SPI_MODE_0_gc;
    SPI0.CTRLA = SPI_DORD_bm | SPI_MASTER_bm | ~SPI_CLK2X_bm | SPI_PRESC_DIV16_gc | SPI_ENABLE_bm;

    sei();
    uint16_t rpm = 0;
    while (1) {
        if (ledUpdateFlag) {
            ledUpdateFlag = false;

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

            PORTA.OUTCLR = PIN4_bm;
            SPI0.DATA = 1 << nextDigit;
            nextDigit = (nextDigit + 1) % 4;
            while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
                ;
            }
            SPI0.DATA = rpm / 8; // 仮
            while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
                ;
            }
            PORTA.OUTSET = PIN4_bm;
        }
    }
}

ISR(PORTB_PORT_vect) {
    PORTB.INTFLAGS = PIN1_bm;
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
}