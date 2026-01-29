#include "uart-driver.h"

#define BAUD_RATE    115200        // setting baud rate
#define UART_TX_PIN  0             // multiplexing pin 0 as tx
#define UART_RX_PIN  1

struct repeating_timer rx_timer;
bool rx_timer_callback(struct repeating_timer *t);
int64_t rx_start_alarm_callback(alarm_id_t id, void *user_data);

typedef enum {  // enumerating states of tx state machine
    TX_IDLE,
    TX_START,
    TX_DATA,
    TX_STOP
} tx_state_t;

typedef enum {  // enumerating states of rx state machine
    RX_IDLE,
    RX_START,
    RX_DATA,
    RX_STOP
} rx_state_t;

// always read/write from memory
volatile tx_state_t tx_state = TX_IDLE;
volatile uint8_t tx_byte = 0;
volatile int bit_index = 0;
volatile bool tx_busy = false;

volatile rx_state_t rx_state = RX_IDLE;
volatile uint8_t rx_byte = 0;
volatile int rx_bit_index = 0;
volatile bool rx_ready = false;

// bit period calculator; rounds instead of truncating
const int bit_period_us = (1000000 + BAUD_RATE / 2) / BAUD_RATE;

bool tx_timer_transitions(struct repeating_timer *t) {  // parameter defined in pico/sdklib.h
    switch (tx_state) {
        case TX_IDLE:
            gpio_put(UART_TX_PIN, 1);   // line is high when held idle
            break;

        case TX_START:
            gpio_put(UART_TX_PIN, 0);   // triggers on the falling edge
            tx_state = TX_DATA; // state transition (next baud tick will send data bit)
            bit_index = 0;  // lsb first
            break;

        case TX_DATA: {
            int bit = (tx_byte >> bit_index) & 0x01;    // send current bit data by bit shifting & masking
            gpio_put(UART_TX_PIN, bit);

            bit_index++;
            if (bit_index >= 8) {   // loop until msb
                tx_state = TX_STOP; // transition to stop state
            }
            break;
        }

        case TX_STOP:
            gpio_put(UART_TX_PIN, 1);   // returns back to idle
            tx_state = TX_IDLE;
            tx_busy = false;
            break;
    }

    return true;    // keeping the timer repeating
}

void uart_tx_transmit(uint8_t b) {
    while (tx_busy) {
        tight_loop_contents();
    }

    tx_byte = b;
    tx_busy = true;
    tx_state = TX_START;
}

bool rx_timer_callback(struct repeating_timer *t) {
    switch (rx_state) {
        case RX_IDLE:   // shouldn't be running
            return true;

        case RX_START: {
            int level = gpio_get(UART_RX_PIN);
            if (level == 0) {   // falling edge detected
                rx_state = RX_DATA;
                rx_bit_index = 0;
            } else {    // false start
                rx_state = RX_IDLE;
                cancel_repeating_timer(t);  // not inside UART frame
            }
            break;
        }

        case RX_DATA: {
            int bit = gpio_get(UART_RX_PIN) & 0x01;
            rx_byte |= (bit << rx_bit_index);  // lsb first
            rx_bit_index++;

            if (rx_bit_index >= 8) {
                rx_state = RX_STOP;
            }
            break;
        }

        case RX_STOP: {
            int level = gpio_get(UART_RX_PIN);
            if (level == 1) {
                rx_ready = true;  // rising edge detected
            }
            rx_state = RX_IDLE; // return back to idle state
            cancel_repeating_timer(t);
            break;
        }
    }

    return true;  // keep timer running until we explicitly cancel in STOP/IDLE
}

int64_t rx_start_alarm_callback(alarm_id_t id, void *user_data) {
    if (gpio_get(UART_RX_PIN) == 0) {
        rx_state = RX_DATA;
        add_repeating_timer_us(-bit_period_us, rx_timer_callback, NULL, &rx_timer);
    } else {
        rx_state = RX_IDLE;
    }
    return 0;
}

void rx_gpio_irq(uint gpio, uint32_t events) {
    if (gpio != UART_RX_PIN) return;

    if (rx_state == RX_IDLE && gpio_get(UART_RX_PIN) == 0) {    // triggers interrupt request
        rx_state = RX_START;
        rx_bit_index = 0;
        rx_byte = 0;
        rx_ready = false;

        add_alarm_in_us(bit_period_us / 2, rx_start_alarm_callback, NULL, true);    // begins 0.5 bit into the start bit
    }
}

int main() {
    stdio_init_all();

    gpio_init(UART_TX_PIN);
    gpio_init(UART_RX_PIN);
    gpio_set_dir(UART_TX_PIN, GPIO_OUT);
    gpio_set_dir(UART_RX_PIN, GPIO_IN);
    gpio_put(UART_TX_PIN, 1);   // idle high
    gpio_pull_up(UART_RX_PIN);  // idle high when line is disconnected

    // testing
    const char *msg = "Hello\r\n";  //  iterating through each character until \0

    while (true) {
        const char *p = msg;
        while (*p) {
            uart_tx_transmit((uint8_t)*p++);
        }
        sleep_ms(1000); // waits one second
    }
}

// cmake -DPICO_BOARD=pico2 -DPICO_SDK_PATH=/Users/yingyedeng/pico/pico-sdk ..