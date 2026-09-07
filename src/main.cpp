#include <avr/interrupt.h>
#include <avr/io.h>

constexpr uint32_t PWM_FREQ = 25000;
constexpr uint16_t MIN_RPM = 120;

constexpr uint16_t PWM_PERIOD_CLOCKS = F_CPU / PWM_FREQ;
constexpr uint32_t CLK_TICKS_PER_MIN = F_CPU / PWM_PERIOD_CLOCKS * 60;
constexpr uint32_t TIMEOUT_TICKS = CLK_TICKS_PER_MIN / MIN_RPM / 2;

volatile uint16_t time = 0;
volatile uint16_t lastPulse = 0;
volatile uint16_t pulseWidth = 0;

int main() {
    _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, ~CLKCTRL_PEN_bm);

    PORTB.DIRSET = PIN0_bm;
    PORTB.DIRCLR = PIN1_bm;
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;

    TCA0.SINGLE.CMP0 = PWM_PERIOD_CLOCKS;
    TCA0.SINGLE.PER = PWM_PERIOD_CLOCKS - 1;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;

    sei();
    while (1) {
        cli();
        const uint16_t ticks = pulseWidth;
        sei();
        uint16_t rpm;
        if (ticks == 0) {
            rpm = 0;
        } else {
            rpm = CLK_TICKS_PER_MIN / ticks / 2;
        }
        TCA0.SINGLE.CMP0BUF = (uint32_t)PWM_PERIOD_CLOCKS * rpm / 2000;
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
    if (time == lastPulse + TIMEOUT_TICKS) {
        pulseWidth = 0;
    }
}