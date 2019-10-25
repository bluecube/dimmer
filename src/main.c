#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>

enum State {
    STATE_OFF,
    STATE_ON,
    STATE_BUTTON,
    STATE_SAVING
};

static const uint8_t PWM_MAX = 167; // maximum value of PWM counter (corresponds to 100% PWM, calculated in dimmer.ipynb)
static const uint8_t PWM_MIN = 10; // minimum value of PWM counter, used to limit minimum brightness of the light.
static const uint16_t TRIGGER_VOLTAGE = 304; // ADC reading below which the power off transition gets triggered (calculated in dimmer.ipynb)

enum State state;

// Slow down clock and go to "idle" sleep mode
void go_to_sleep() {
    cli();
    CLKPR = 1<<CLKPCE; // Unlock prescaler change
    CLKPR = 1<<CLKPS2 | 1<<CLKPS1; // Set prescaler to 64 (125kHz, lowest possible speed for ADC)
    set_sleep_mode(SLEEP_MODE_IDLE);
    sei(); // The next instruction will be executed before interrupts occur.
    sleep_cpu();
}

// Speed clock back up.
// Should be called with interrupts disabled.
void restore_clock_speed() {
    CLKPR = 1<<CLKPCE; // Unlock prescaler change
    CLKPR = 0; // Set prescaler to 1 (8MHz)
}

void setup_voltage_monitoring() {
    ADMUX = 0 | // REFS2:0 all 0, using Vcc as reference
            1<<MUX3 | 1<<MUX2; // Single ended input Vbg
    ADCSRA = 1<<ADEN | // ADC enabled
             1<<ADIE | // ADC interrupt enabled
             0; // ADPS2:0 all zero, prescaler 2
}

void setup_output() {

}

void setup() {
    setup_voltage_monitoring();
    setup_output();
}

void disable_output() {
    
}

void disable_votlage_monitoring() {
    
}

// Transition to state STATE_OFF
// Must be called with interrupts disabled
void power_off() {
    state = STATE_SAVING;

    disable_output();
    disable_voltage_monitoring();
    set_int0_level_triggered();

    save_brightnes(); // Trigger the EEPROM write

    // The rest of the transition will be done in the EEPROM ISR
}

void power_on() {
    set_int0_edge_triggered();
}

void __attribute__((OS_main,noreturn)) main() {
    setup(); // Initial setup of peripherials

    power_off(); // Wake up to powered off state (TODO: Remember last state?)

    while (1) // After waking up go immediately to sleep again
        go_to_sleep();
}

void set_brightness(uint8_t brightness) {
    OCR1A = OCR1B = brightness; //TODO: Do this only in Timer1 overflow interrupt to avoid desynchronizing pin values
}

uint8_t get_brightness() {
    return OCR1A;
}

ISR(ADC_vect) {
    restore_clock_speed();
    if (ADC > TRIGGER_VOLTAGE)
        power_off();
    else
        ADCSRA = ADCSRA | 1<<ADSC; // Start next ADC conversion
}

ISR(INT0_vect) {
    restore_clock_speed();
    switch (state) {
        case STATE_OFF:
            power_on();
            break;
        case STATE_ON:
            state = STATE_BUTTON;
            break;
    }
}

ISR(TIM1_OVF_vect) {

}
