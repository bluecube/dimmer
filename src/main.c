#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>

enum PowerState {
    STATE_OFF,
    STATE_ON,
    STATE_BUTTON,
    STATE_SAVING
};

static const uint8_t PWM_MAX = 167; // maximum value of PWM counter (corresponds to 100% PWM, calculated in dimmer.ipynb)
static const uint8_t PWM_MIN = 10; // minimum value of PWM counter, used to limit minimum brightness of the light.
static const uint16_t TRIGGER_VOLTAGE = 304; // ADC reading below which the power off transition gets triggered (calculated in dimmer.ipynb)

enum PowerState state;
uint8_t buttonState, buttonCounter;

// Slow down clock and go to "idle" sleep mode
// Should be called with interrupts enabled
void go_to_sleep() {
    cli(); // Disable interupts to do the slowing down of system clock atomically
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

void setup() {
    setup_voltage_monitoring();
    setup_output();
}

// Transition to state STATE_OFF
// Must be called with interrupts disabled
void power_off() {
    state = STATE_SAVING;

    disable_output();
    disable_voltage_monitoring();
    set_int0_level_triggered(); // Only level triggered interrupt can wake us up from power-down sleep mode

    save_brightnes(get_brightness); // Trigger the EEPROM write

    // The rest of the transition will be done in the EEPROM ISR
}

void power_on() {
    state = STATE_ON;

    set_int0_edge_triggered(); // We need to detect both button press and release
    enable_voltage_monitoring();
    enable_outputs();

    set_brightness(load_brightness());
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
    uint8_t newButtonState = PINB & _BV(PB2);

    if (state == STATE_OFF)
    {
        power_on();
        buttonState = newButtonState;
        buttonCounter = 1;
        return;
    }

    if (newButtonState == buttonState)
        return; // Can this even happen?

    if (buttonState)
    {
        // Pressed the button

    }
    else
    {
        // Released the button
    }

}

ISR(TIM1_OVF_vect) {

}
