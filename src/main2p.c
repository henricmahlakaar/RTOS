#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>

/*
 * 2p suoritus:
 * - Punainen, keltainen ja vihreä liikennevalo
 * Tekijät: Henric M. ja Jere K.
 */

static const struct gpio_dt_spec red =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct gpio_dt_spec green =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct gpio_dt_spec blue =
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw4), gpios);

static struct gpio_callback button_cb_data;

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

/* Tila ennen pausea */
volatile int previous_led_state = 0;

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

    /* Kaikki LEDit aluksi pois */

    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    gpio_pin_set_dt(&blue, 0);

    printk("LEDs initialized OK\n");

    return 0;
}

void button_0_handler(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    printk("Play/Pause pressed\n");
    
    if (led_state != 4) {

        previous_led_state = led_state;

        led_state = 4;

        printk("Traffic lights PAUSED\n");
    }

    else {

        led_state = previous_led_state;

        printk("Traffic lights RESUMED\n");
    }
}

int init_button(void)
{
    int ret;

    if (!gpio_is_ready_dt(&button)) {

        printk("Error: Button device is not ready\n");

        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(
        &button,
        GPIO_INPUT
    );

    if (ret < 0) {

        printk("Error: Button configure failed\n");

        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {

        printk("Error: Button interrupt configure failed\n");

        return ret;
    }

    gpio_init_callback(
        &button_cb_data,
        button_0_handler,
        BIT(button.pin)
    );

    ret = gpio_add_callback(
        button.port,
        &button_cb_data
    );

    if (ret < 0) {

        printk("Error: Button callback failed\n");

        return ret;
    }

    printk("Play/Pause button initialized OK\n");

    return 0;
}

void red_led_task(void *, void *, void *)
{
    printk("Red LED thread started\n");

    while (true) {

        if (led_state == 0) {

            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 0);
            gpio_pin_set_dt(&blue, 0);

            printk("RED ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 0) {

                gpio_pin_set_dt(&red, 0);

                printk("RED OFF\n");

                k_sleep(K_SECONDS(1));

                if (led_state == 0) {

                    led_state = 1;
                }
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
            gpio_pin_set_dt(&blue, 0);

            printk("YELLOW ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 1) {

                gpio_pin_set_dt(&red, 0);
                gpio_pin_set_dt(&green, 0);

                printk("YELLOW OFF\n");

                k_sleep(K_SECONDS(1));

                if (led_state == 1) {

                    led_state = 2;
                }
            }
        }

        k_yield();
    }
}

void green_led_task(void *, void *, void *)
{
    printk("Green LED thread started\n");

    while (true) {

        if (led_state == 2) {

            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 1);
            gpio_pin_set_dt(&blue, 0);

            printk("GREEN ON\n");

            k_sleep(K_SECONDS(1));

            if (led_state == 2) {

                gpio_pin_set_dt(&green, 0);

                printk("GREEN OFF\n");

                k_sleep(K_SECONDS(1));

                if (led_state == 2) {

                    led_state = 0;
                }
            }
        }

        k_yield();
    }
}

int main(void)
{
    int ret;

    ret = init_led();

    if (ret < 0) {

        printk("LED initialization failed\n");

        return ret;
    }

    ret = init_button();

    if (ret < 0) {

        printk("Button initialization failed\n");

        return ret;
    }

    printk("--------------------------------\n");
    printk("Traffic light program started\n");
    printk("Button 1 = Play/Pause\n");
    printk("Press Play/Pause to pause/resume\n");
    printk("--------------------------------\n");

    return 0;
}
