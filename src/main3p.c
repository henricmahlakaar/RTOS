#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <stdio.h>

/*
 * Viikko 3 - 3p suoritus
 * Tekijät: Henric M. ja Jere K.
 */

#define STACKSIZE 500
#define PRIORITY 5
#define DEBUG_PRIORITY 6

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define DEBUG

static const struct gpio_dt_spec red =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct gpio_dt_spec green =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct gpio_dt_spec blue =
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static const struct device *const uart_dev =
    DEVICE_DT_GET(UART_DEVICE_NODE);

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);

static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);
static void debug_task(void *, void *, void *);

K_THREAD_DEFINE(
    red_thread,
    STACKSIZE,
    red_led_task,
    NULL, NULL, NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    yellow_thread,
    STACKSIZE,
    yellow_led_task,
    NULL, NULL, NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    green_thread,
    STACKSIZE,
    green_led_task,
    NULL, NULL, NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    uart_thread,
    STACKSIZE,
    uart_task,
    NULL, NULL, NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    dispatcher_thread,
    STACKSIZE,
    dispatcher_task,
    NULL, NULL, NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    debug_thread,
    STACKSIZE,
    debug_task,
    NULL, NULL, NULL,
    DEBUG_PRIORITY,
    0,
    0
);

K_SEM_DEFINE(red_sem, 0, 1);
K_SEM_DEFINE(yellow_sem, 0, 1);
K_SEM_DEFINE(green_sem, 0, 1);

K_SEM_DEFINE(done_sem, 0, 1);

K_FIFO_DEFINE(data_fifo);

K_FIFO_DEFINE(debug_fifo);

struct data_t {
    void *fifo_reserved;
    char color;
};

struct debug_data_t {
    void *fifo_reserved;
    char message[64];
};

static uint64_t sequence_total = 0;

void debug_print(const char *message)
{
#ifdef DEBUG

    struct debug_data_t *data =
        k_malloc(sizeof(struct debug_data_t));

    if (data == NULL) {
        return;
    }

    snprintf(
        data->message,
        sizeof(data->message),
        "%s",
        message
    );

    k_fifo_put(&debug_fifo, data);

#endif
}

int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        return 1;
    }

    return 0;
}

int init_led(void)
{
    int ret;

    ret = gpio_pin_configure_dt(
        &red,
        GPIO_OUTPUT_ACTIVE
    );

    if (ret < 0) {
        printk("Error: Red LED configure failed\n");
        return ret;
    }

    ret = gpio_pin_configure_dt(
        &green,
        GPIO_OUTPUT_ACTIVE
    );

    if (ret < 0) {
        printk("Error: Green LED configure failed\n");
        return ret;
    }

    ret = gpio_pin_configure_dt(
        &blue,
        GPIO_OUTPUT_ACTIVE
    );

    if (ret < 0) {
        printk("Error: Blue LED configure failed\n");
        return ret;
    }

    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    gpio_pin_set_dt(&blue, 0);

    printk("LED initialized ok\n");

    return 0;
}

void add_sequence_time(uint64_t task_ns)
{
    sequence_total += task_ns;
}

void print_sequence_time(void)
{
    uint64_t total_us =
        sequence_total / 1000;

    char message[64];

    snprintf(
        message,
        sizeof(message),
        "Total sequence time: %lld us",
        total_us
    );

    debug_print(message);

    sequence_total = 0;
}

static void debug_task(void *, void *, void *)
{
    struct debug_data_t *data;

    printk("Debug thread started\n");

    while (true) {

        data = k_fifo_get(
            &debug_fifo,
            K_FOREVER
        );

        if (data != NULL) {

            printk(
                "%s\n",
                data->message
            );

            k_free(data);
        }
    }
}

static void uart_task(void *, void *, void *)
{
    char c = 0;

    debug_print("UART thread started");

    while (true) {

        if (uart_poll_in(
                uart_dev,
                &c) == 0) {

            if (c == 'R' ||
                c == 'Y' ||
                c == 'G') {

                timing_start();

                timing_t uart_start_time =
                    timing_counter_get();

                struct data_t *data =
                    k_malloc(
                        sizeof(struct data_t)
                    );

                if (data == NULL) {

                    debug_print(
                        "Malloc failed"
                    );

                    timing_t uart_end_time =
                        timing_counter_get();

                    timing_stop();

                    continue;
                }

                data->color = c;

                k_fifo_put(
                    &data_fifo,
                    data
                );

                timing_t uart_end_time =
                    timing_counter_get();

                timing_stop();

                uint64_t uart_ns =
                    timing_cycles_to_ns(
                        timing_cycles_get(
                            &uart_start_time,
                            &uart_end_time
                        )
                    );

                uint64_t uart_us =
                    uart_ns / 1000;

                char message[64];

                snprintf(
                    message,
                    sizeof(message),
                    "UART processing time: %lld us",
                    uart_us
                );

                debug_print(message);

#ifdef DEBUG

                snprintf(
                    message,
                    sizeof(message),
                    "Data sent to FIFO: %c",
                    c
                );

                debug_print(message);

#endif
            }
        }

        k_msleep(10);
    }
}

static void dispatcher_task(void *, void *, void *)
{
    debug_print("Dispatcher thread started");

    while (true) {

        struct data_t *data =
            k_fifo_get(
                &data_fifo,
                K_FOREVER
            );

        if (data != NULL) {

            timing_start();

            timing_t dispatcher_start_time =
                timing_counter_get();

#ifdef DEBUG

            char message[64];

            snprintf(
                message,
                sizeof(message),
                "Dispatcher received: %c",
                data->color
            );

            debug_print(message);

#endif

            switch (data->color) {

                case 'R':
                    k_sem_give(&red_sem);
                    break;

                case 'Y':
                    k_sem_give(&yellow_sem);
                    break;

                case 'G':
                    k_sem_give(&green_sem);
                    break;

                default:
                    break;
            }

            k_free(data);

            timing_t dispatcher_end_time =
                timing_counter_get();

            timing_stop();

            uint64_t dispatcher_ns =
                timing_cycles_to_ns(
                    timing_cycles_get(
                        &dispatcher_start_time,
                        &dispatcher_end_time
                    )
                );

            uint64_t dispatcher_us =
                dispatcher_ns / 1000;

            char message2[64];

            snprintf(
                message2,
                sizeof(message2),
                "Dispatcher processing time: %lld us",
                dispatcher_us
            );

            debug_print(message2);

            k_sem_take(
                &done_sem,
                K_FOREVER
            );

            if (k_fifo_is_empty(&data_fifo)) {
                print_sequence_time();
            }
        }
    }
}

void red_led_task(void *, void *, void *)
{
    debug_print("Red LED thread started");

    while (true) {

        k_sem_take(
            &red_sem,
            K_FOREVER
        );

        timing_start();

        timing_t red_start_time =
            timing_counter_get();

        gpio_pin_set_dt(&red, 1);

        debug_print("Red ON");

        k_sleep(K_SECONDS(1));

        gpio_pin_set_dt(&red, 0);

        debug_print("Red OFF");

        timing_t red_end_time =
            timing_counter_get();

        timing_stop();

        uint64_t timing_ns =
            timing_cycles_to_ns(
                timing_cycles_get(
                    &red_start_time,
                    &red_end_time
                )
            );

        uint64_t timing_us =
            timing_ns / 1000;

        add_sequence_time(timing_ns);

        char message[64];

        snprintf(
            message,
            sizeof(message),
            "Red LED task time: %lld us",
            timing_us
        );

        debug_print(message);

        k_sem_give(&done_sem);
    }
}

void yellow_led_task(void *, void *, void *)
{
    debug_print("Yellow LED thread started");

    while (true) {

        k_sem_take(
            &yellow_sem,
            K_FOREVER
        );

        timing_start();

        timing_t yellow_start_time =
            timing_counter_get();

        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);

        debug_print("Yellow ON");

        k_sleep(K_SECONDS(1));

        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        debug_print("Yellow OFF");

        timing_t yellow_end_time =
            timing_counter_get();

        timing_stop();

        uint64_t timing_ns =
            timing_cycles_to_ns(
                timing_cycles_get(
                    &yellow_start_time,
                    &yellow_end_time
                )
            );

        uint64_t timing_us =
            timing_ns / 1000;

        add_sequence_time(timing_ns);

        char message[64];

        snprintf(
            message,
            sizeof(message),
            "Yellow LED task time: %lld us",
            timing_us
        );

        debug_print(message);

        k_sem_give(&done_sem);
    }
}

void green_led_task(void *, void *, void *)
{
    debug_print("Green LED thread started");

    while (true) {

        k_sem_take(
            &green_sem,
            K_FOREVER
        );

        timing_start();

        timing_t green_start_time =
            timing_counter_get();

        gpio_pin_set_dt(&green, 1);

        debug_print("Green ON");

        k_sleep(K_SECONDS(1));

        gpio_pin_set_dt(&green, 0);

        debug_print("Green OFF");

        timing_t green_end_time =
            timing_counter_get();

        timing_stop();

        uint64_t timing_ns =
            timing_cycles_to_ns(
                timing_cycles_get(
                    &green_start_time,
                    &green_end_time
                )
            );

        uint64_t timing_us =
            timing_ns / 1000;

        add_sequence_time(timing_ns);

        char message[64];

        snprintf(
            message,
            sizeof(message),
            "Green LED task time: %lld us",
            timing_us
        );

        debug_print(message);

        k_sem_give(&done_sem);
    }
}

int main(void)
{
    timing_init();

    timing_start();

    timing_t start_time =
        timing_counter_get();

    if (init_led() != 0) {

        printk("LED INIT FAILED\n");

        return 1;
    }

    if (init_uart() != 0) {

        printk("UART INIT FAILED\n");

        return 1;
    }

    timing_t end_time =
        timing_counter_get();

    timing_stop();

    uint64_t timing_ns =
        timing_cycles_to_ns(
            timing_cycles_get(
                &start_time,
                &end_time
            )
        );

    uint64_t timing_us =
        timing_ns / 1000;

    printk(
        "Initialization time: %lld us\n",
        timing_us
    );

    while (true) {
        k_msleep(1000);
    }

    return 0;
}