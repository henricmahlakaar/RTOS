#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec red =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct gpio_dt_spec green =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct gpio_dt_spec blue =
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);


static const struct gpio_dt_spec button1 =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static const struct gpio_dt_spec button2 =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const struct gpio_dt_spec button3 =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);

static const struct gpio_dt_spec button4 =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);

static const struct gpio_dt_spec button5 =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw4), gpios);

static struct gpio_callback button1_cb_data;
static struct gpio_callback button2_cb_data;
static struct gpio_callback button3_cb_data;
static struct gpio_callback button4_cb_data;
static struct gpio_callback button5_cb_data;

#define STACKSIZE 500
#define PRIORITY 5

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);

K_THREAD_DEFINE(
    red_thread,
    STACKSIZE,
    red_led_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    yellow_thread,
    STACKSIZE,
    yellow_led_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    green_thread,
    STACKSIZE,
    green_led_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);

volatile int led_state = 0;
volatile int previous_led_state = 0;


volatile bool manual_red = false;
volatile bool manual_yellow = false;
volatile bool manual_green = false;

static int64_t last_button1_time = 0;
static int64_t last_button2_time = 0;
static int64_t last_button3_time = 0;
static int64_t last_button4_time = 0;
static int64_t last_button5_time = 0;

#define DEBOUNCE_TIME_MS 200

static void all_leds_off(void)
{
    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    gpio_pin_set_dt(&blue, 0);
}

void button_0_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now = k_uptime_get();

    if ((now - last_button1_time) < DEBOUNCE_TIME_MS) {
        return;
    }

    last_button1_time = now;

    printk("BTN 1 pressed\n");

    if (led_state != 4) {

        previous_led_state = led_state;
        led_state = 4;

        printk("Traffic lights PAUSED\n");
    }
    else {

        manual_red = false;
        manual_yellow = false;
        manual_green = false;

        all_leds_off();

        led_state = 0;

        printk("Traffic lights RESUMED - starting from RED\n");
    }
}

void button_1_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now = k_uptime_get();

    if ((now - last_button2_time) < DEBOUNCE_TIME_MS) {
        return;
    }

    last_button2_time = now;

    led_state = 4;

    manual_red = !manual_red;

    if (manual_red) {

        manual_yellow = false;
        manual_green = false;

        gpio_pin_set_dt(&green, 0);
        gpio_pin_set_dt(&blue, 0);
        gpio_pin_set_dt(&red, 1);

        printk("BTN 2: RED ON\n");
    }
    else {

        gpio_pin_set_dt(&red, 0);

        printk("BTN 2: RED OFF\n");
    }
}

void button_2_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now = k_uptime_get();

    if ((now - last_button3_time) < DEBOUNCE_TIME_MS) {
        return;
    }

    last_button3_time = now;

    led_state = 4;

    manual_yellow = !manual_yellow;

    if (manual_yellow) {

        manual_red = false;
        manual_green = false;

        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);
        gpio_pin_set_dt(&blue, 0);

        printk("BTN 3: YELLOW ON\n");
    }
    else {

        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        printk("BTN 3: YELLOW OFF\n");
    }
}

void button_3_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now = k_uptime_get();

    if ((now - last_button4_time) < DEBOUNCE_TIME_MS) {
        return;
    }

    last_button4_time = now;

    led_state = 4;

    manual_green = !manual_green;

    if (manual_green) {

        manual_red = false;
        manual_yellow = false;

        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&blue, 0);
        gpio_pin_set_dt(&green, 1);

        printk("BTN 4: GREEN ON\n");
    }
    else {

        gpio_pin_set_dt(&green, 0);

        printk("BTN 4: GREEN OFF\n");
    }
}

void button_4_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    int64_t now = k_uptime_get();

    if ((now - last_button5_time) < DEBOUNCE_TIME_MS) {
        return;
    }

    last_button5_time = now;

    printk("BTN 5 pressed\n");

    if (led_state != 5) {

        previous_led_state = led_state;

        manual_red = false;
        manual_yellow = false;
        manual_green = false;

        led_state = 5;

        all_leds_off();

        printk("Blinking yellow STARTED\n");
    }
    else {

        led_state = previous_led_state;

        all_leds_off();

        printk("Blinking yellow STOPPED\n");
    }
}

void red_led_task(void *, void *, void *)
{
    printk("Red LED thread started\n");

    while (true) {

        if (led_state == 0) {

            gpio_pin_set_dt(&red, 1);

            printk("Red ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state != 0) {
                k_yield();
                continue;
            }

            gpio_pin_set_dt(&red, 0);

            printk("Red OFF\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 0) {
                led_state = 1;
            }
        }

        k_yield();
    }
}

void yellow_led_task(void *, void *, void *)
{
    printk("Yellow LED thread started\n");

    while (true) {

        if (led_state == 1) {

            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);

            printk("Yellow ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state != 1) {
                k_yield();
                continue;
            }

            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);

            printk("Yellow OFF\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 1) {
                led_state = 2;
            }
        }

        if (led_state == 5) {

            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);

            printk("Blinking yellow ON\n");

            k_sleep(K_MSEC(500));

            if (led_state != 5) {
                k_yield();
                continue;
            }

            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);

            printk("Blinking yellow OFF\n");

            k_sleep(K_MSEC(500));
        }

        k_yield();
    }
}

void green_led_task(void *, void *, void *)
{
    printk("Green LED thread started\n");

    while (true) {

        if (led_state == 2) {

            gpio_pin_set_dt(&green, 1);

            printk("Green ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state != 2) {
                k_yield();
                continue;
            }

            gpio_pin_set_dt(&green, 0);

            printk("Green OFF\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 2) {
                led_state = 0;
            }
        }

        k_yield();
    }
}

int init_led(void)
{
    int ret;

    ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error: Red LED configure failed\n");
        return ret;
    }

    ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error: Green LED configure failed\n");
        return ret;
    }

    ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error: Blue LED configure failed\n");
        return ret;
    }

    all_leds_off();

    printk("LEDs initialized OK\n");

    return 0;
}

int init_buttons(void)
{
    int ret;

    /* BTN 1 */

    ret = gpio_pin_configure_dt(
        &button1,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Error: BTN 1 configure failed\n");
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button1,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Error: BTN 1 interrupt configure failed\n");
        return ret;
    }

    gpio_init_callback(
        &button1_cb_data,
        button_0_handler,
        BIT(button1.pin)
    );

    gpio_add_callback(
        button1.port,
        &button1_cb_data
    );

    ret = gpio_pin_configure_dt(
        &button2,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Error: BTN 2 configure failed\n");
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button2,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Error: BTN 2 interrupt configure failed\n");
        return ret;
    }

    gpio_init_callback(
        &button2_cb_data,
        button_1_handler,
        BIT(button2.pin)
    );

    gpio_add_callback(
        button2.port,
        &button2_cb_data
    );

    ret = gpio_pin_configure_dt(
        &button3,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Error: BTN 3 configure failed\n");
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button3,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Error: BTN 3 interrupt configure failed\n");
        return ret;
    }

    gpio_init_callback(
        &button3_cb_data,
        button_2_handler,
        BIT(button3.pin)
    );

    gpio_add_callback(
        button3.port,
        &button3_cb_data
    );

    ret = gpio_pin_configure_dt(
        &button4,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Error: BTN 4 configure failed\n");
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button4,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Error: BTN 4 interrupt configure failed\n");
        return ret;
    }

    gpio_init_callback(
        &button4_cb_data,
        button_3_handler,
        BIT(button4.pin)
    );

    gpio_add_callback(
        button4.port,
        &button4_cb_data
    );

    ret = gpio_pin_configure_dt(
        &button5,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Error: BTN 5 configure failed\n");
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button5,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Error: BTN 5 interrupt configure failed\n");
        return ret;
    }

    gpio_init_callback(
        &button5_cb_data,
        button_4_handler,
        BIT(button5.pin)
    );

    gpio_add_callback(
        button5.port,
        &button5_cb_data
    );


    printk("All buttons initialized OK\n");

    return 0;
}

int main(void)
{
    int ret;

    printk("\n");
    printk("==============================\n");
    printk("Traffic Light FSM - 3p\n");
    printk("==============================\n");

    ret = init_led();

    if (ret < 0) {
        printk("LED initialization failed\n");
        return ret;
    }

    ret = init_buttons();

    if (ret < 0) {
        printk("Button initialization failed\n");
        return ret;
    }

    printk("System started\n");
    printk("BTN 1 = Pause / Resume\n");
    printk("BTN 2 = Red ON/OFF\n");
    printk("BTN 3 = Yellow ON/OFF\n");
    printk("BTN 4 = Green ON/OFF\n");
    printk("BTN 5 = Blinking Yellow\n");

    return 0;
}
