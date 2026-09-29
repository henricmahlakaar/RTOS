#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Tässä 2p suoritus 2vk tehtävästä.
// Tekijä: Henric M. ja Jere M.

#define STACKSIZE 500
#define PRIORITY 5
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

// Ledit
static const struct gpio_dt_spec red =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct gpio_dt_spec green =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct gpio_dt_spec blue =
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static const struct device *const uart_dev =
    DEVICE_DT_GET(UART_DEVICE_NODE);

K_FIFO_DEFINE(dispatcher_fifo);

struct data_t {
    void *fifo_reserved;
    char msg[20];
};

K_FIFO_DEFINE(red_fifo);
K_FIFO_DEFINE(yellow_fifo);
K_FIFO_DEFINE(green_fifo);

struct led_data_t {
    void *fifo_reserved;
    int time_ms;
};

K_SEM_DEFINE(done_sem, 0, 1);

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);

static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);

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

K_THREAD_DEFINE(
    uart_thread,
    STACKSIZE,
    uart_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    dispatcher_thread,
    STACKSIZE,
    dispatcher_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);

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

    ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
    ret |= gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
    ret |= gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);

    if (ret < 0) {
        printk("Error: Led configure failed\n");
        return ret;
    }

    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&blue, 0);
    gpio_pin_set_dt(&green, 0);

    printk("Led initialized ok\n");

    return 0;
}

static void uart_task(void *, void *, void *)
{
    char rc = 0;
    char uart_msg[20];

    int uart_msg_cnt = 0;

    memset(uart_msg, 0, sizeof(uart_msg));

    printk("Uart thread started\n");

    while (true) {

        if (uart_poll_in(uart_dev, &rc) == 0) {

            if (rc == '\r' || rc == '\n') {

                if (uart_msg_cnt > 0) {

                    uart_msg[uart_msg_cnt] = '\0';

                    printk("UART msg: %s\n", uart_msg);

                    struct data_t *buf =
                        k_malloc(sizeof(struct data_t));

                    if (buf == NULL) {
                        printk("Malloc failed\n");
                        uart_msg_cnt = 0;
                        memset(uart_msg, 0, sizeof(uart_msg));
                        continue;
                    }

                    snprintf(
                        buf->msg,
                        sizeof(buf->msg),
                        "%s",
                        uart_msg
                    );

                    k_fifo_put(&dispatcher_fifo, buf);

                    uart_msg_cnt = 0;
                    memset(uart_msg, 0, sizeof(uart_msg));
                }
            }

            else {

                if (uart_msg_cnt < sizeof(uart_msg) - 1) {

                    uart_msg[uart_msg_cnt] = rc;
                    uart_msg_cnt++;
                }
            }
        }

        k_msleep(10);
    }
}

static void dispatcher_task(void *, void *, void *)
{
    printk("Dispatcher thread started\n");

    while (true) {

        struct data_t *rec_item =
            k_fifo_get(&dispatcher_fifo, K_FOREVER);

        if (rec_item == NULL) {
            continue;
        }

        char sequence[20];

        memcpy(
            sequence,
            rec_item->msg,
            sizeof(sequence)
        );

        k_free(rec_item);

        printk("Dispatcher: %s\n", sequence);

       char color = sequence[0];

        int time = atoi(sequence + 2);

        printk(
            "Data: color=%c time=%d ms\n",
            color,
            time
        );

        struct led_data_t *led_data =
            k_malloc(sizeof(struct led_data_t));

        if (led_data == NULL) {
            printk("LED malloc failed\n");
            continue;
        }

        led_data->time_ms = time;


        switch (color) {

            case 'R':
                k_fifo_put(&red_fifo, led_data);
                break;

            case 'Y':
                k_fifo_put(&yellow_fifo, led_data);
                break;

            case 'G':
                k_fifo_put(&green_fifo, led_data);
                break;

            default:
                printk("Unknown color: %c\n", color);
                k_free(led_data);
                continue;
        }

        k_sem_take(&done_sem, K_FOREVER);
    }
}

void red_led_task(void *, void *, void *)
{
    printk("Red led thread started\n");

    while (true) {

        struct led_data_t *data =
            k_fifo_get(&red_fifo, K_FOREVER);

        if (data == NULL) {
            continue;
        }

        gpio_pin_set_dt(&red, 1);

        printk(
            "Red ON for %d ms\n",
            data->time_ms
        );

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&red, 0);

        printk("Red OFF\n");

        k_free(data);

        k_sem_give(&done_sem);
    }
}

void yellow_led_task(void *, void *, void *)
{
    printk("Yellow led thread started\n");

    while (true) {

        struct led_data_t *data =
            k_fifo_get(&yellow_fifo, K_FOREVER);

        if (data == NULL) {
            continue;
        }

        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);

        printk(
            "Yellow ON for %d ms\n",
            data->time_ms
        );

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        printk("Yellow OFF\n");

        k_free(data);

        k_sem_give(&done_sem);
    }
}

void green_led_task(void *, void *, void *)
{
    printk("Green led thread started\n");

    while (true) {

        struct led_data_t *data =
            k_fifo_get(&green_fifo, K_FOREVER);

        if (data == NULL) {
            continue;
        }

        gpio_pin_set_dt(&green, 1);

        printk(
            "Green ON for %d ms\n",
            data->time_ms
        );

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&green, 0);

        printk("Green OFF\n");

        k_free(data);

        k_sem_give(&done_sem);
    }
}

int main(void)
{
    init_led();

    if (init_uart() != 0) {

        printk("UART INIT FAILED\n");

        return 1;
    }

    printk("Week 2 - 2p traffic light ready\n");

    return 0;
}