#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define STACKSIZE 500
#define PRIORITY 5

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev =
    DEVICE_DT_GET(UART_DEVICE_NODE);


#define RED_LED_NODE DT_ALIAS(led0)
#define YELLOW_LED_NODE DT_ALIAS(led1)
#define GREEN_LED_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec red_led =
    GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

static const struct gpio_dt_spec yellow_led =
    GPIO_DT_SPEC_GET(YELLOW_LED_NODE, gpios);

static const struct gpio_dt_spec green_led =
    GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);

K_FIFO_DEFINE(dispatcher_fifo);

struct data_t {
    void *fifo_reserved;
    char msg[50];
};

K_FIFO_DEFINE(red_fifo);
K_FIFO_DEFINE(yellow_fifo);
K_FIFO_DEFINE(green_fifo);

struct led_data_t {
    void *fifo_reserved;
    int time_ms;

    bool wait_done;
};

K_CONDVAR_DEFINE(red_cond);
K_CONDVAR_DEFINE(yellow_cond);
K_CONDVAR_DEFINE(green_cond);

K_MUTEX_DEFINE(red_mutex);
K_MUTEX_DEFINE(yellow_mutex);
K_MUTEX_DEFINE(green_mutex);

K_SEM_DEFINE(done_sem, 0, 1);

struct sequence_item {
    char color;
    int time_ms;
};

#define MAX_SEQUENCE 20

static struct sequence_item sequence[MAX_SEQUENCE];

static int sequence_length = 0;

int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        return 1;
    }

    return 0;
}

static void uart_task(void *unused1,
                      void *unused2,
                      void *unused3)
{
    char rc = 0;

    char uart_msg[50];

    memset(uart_msg, 0, sizeof(uart_msg));

    int uart_msg_cnt = 0;

    while (true) {

        if (uart_poll_in(uart_dev, &rc) == 0) {

            if (rc != '\r' && rc != '\n') {

                if (uart_msg_cnt < sizeof(uart_msg) - 1) {

                    uart_msg[uart_msg_cnt] = rc;
                    uart_msg_cnt++;
                }

            } else {

                if (uart_msg_cnt > 0) {

                    uart_msg[uart_msg_cnt] = '\0';

                    printk("UART msg: %s\n",
                           uart_msg);


                    struct data_t *buf =
                        k_malloc(sizeof(struct data_t));


                    if (buf != NULL) {

                        strcpy(buf->msg,
                               uart_msg);

                        k_fifo_put(&dispatcher_fifo,
                                   buf);
                    }


                    uart_msg_cnt = 0;

                    memset(uart_msg,
                           0,
                           sizeof(uart_msg));
                }
            }
        }

        k_msleep(10);
    }
}

static void dispatcher_task(void *unused1,
                            void *unused2,
                            void *unused3)
{
    while (true) {

        struct data_t *rec_item =
            k_fifo_get(&dispatcher_fifo,
                       K_FOREVER);


        char message[50];

        strcpy(message,
               rec_item->msg);

        k_free(rec_item);


        printk("Dispatcher: %s\n",
               message);

        if (strcmp(message, "T") == 0) {

            printk("Repeating sequence...\n");


            for (int i = 0;
                 i < sequence_length;
                 i++) {

                char color =
                    sequence[i].color;

                int time_ms =
                    sequence[i].time_ms;


                struct led_data_t *led_data =
                    k_malloc(sizeof(struct led_data_t));


                if (led_data == NULL) {
                    continue;
                }


                led_data->time_ms = time_ms;

                led_data->wait_done = true;

                if (color == 'R') {

                    printk("Repeat: R, %d ms\n",
                           time_ms);

                    k_fifo_put(&red_fifo,
                               led_data);

                    k_condvar_signal(&red_cond);

                } else if (color == 'Y') {

                    printk("Repeat: Y, %d ms\n",
                           time_ms);

                    k_fifo_put(&yellow_fifo,
                               led_data);

                    k_condvar_signal(&yellow_cond);

                } else if (color == 'G') {

                    printk("Repeat: G, %d ms\n",
                           time_ms);

                    k_fifo_put(&green_fifo,
                               led_data);

                    k_condvar_signal(&green_cond);


                } else {

                    printk("Unknown color: %c\n",
                           color);

                    k_free(led_data);

                    continue;
                }

                k_sem_take(&done_sem,
                           K_FOREVER);
            }


            continue;
        }

        char color = message[0];

        int time_ms = atoi(message + 2);


        printk("Data: color=%c time=%d ms\n",
               color,
               time_ms);

        if (sequence_length < MAX_SEQUENCE) {

            sequence[sequence_length].color =
                color;

            sequence[sequence_length].time_ms =
                time_ms;

            sequence_length++;


            printk("Sequence stored. Length=%d\n",
                   sequence_length);

        } else {

            printk("Sequence is full!\n");
        }

        struct led_data_t *led_data =
            k_malloc(sizeof(struct led_data_t));


        if (led_data == NULL) {
            continue;
        }


        led_data->time_ms = time_ms;

        led_data->wait_done = false;

        if (color == 'R') {

            k_fifo_put(&red_fifo,
                       led_data);

            k_condvar_signal(&red_cond);


        } else if (color == 'Y') {

            k_fifo_put(&yellow_fifo,
                       led_data);

            k_condvar_signal(&yellow_cond);


        } else if (color == 'G') {

            k_fifo_put(&green_fifo,
                       led_data);

            k_condvar_signal(&green_cond);


        } else {

            printk("Unknown color: %c\n",
                   color);

            k_free(led_data);
        }
    }
}

static void red_task(void *unused1,
                     void *unused2,
                     void *unused3)
{
    while (true) {

        k_mutex_lock(&red_mutex,
                     K_FOREVER);


        while (k_fifo_is_empty(&red_fifo)) {

            k_condvar_wait(&red_cond,
                           &red_mutex,
                           K_FOREVER);
        }


        k_mutex_unlock(&red_mutex);


        struct led_data_t *data =
            k_fifo_get(&red_fifo,
                       K_FOREVER);


        printk("Red ON for %d ms\n",
               data->time_ms);


        gpio_pin_set_dt(&red_led, 1);

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&red_led, 0);


        printk("Red OFF\n");

        bool wait_done =
            data->wait_done;


        k_free(data);

        if (wait_done) {

            k_sem_give(&done_sem);
        }
    }
}

static void yellow_task(void *unused1,
                        void *unused2,
                        void *unused3)
{
    while (true) {

        k_mutex_lock(&yellow_mutex,
                     K_FOREVER);


        while (k_fifo_is_empty(&yellow_fifo)) {

            k_condvar_wait(&yellow_cond,
                           &yellow_mutex,
                           K_FOREVER);
        }


        k_mutex_unlock(&yellow_mutex);


        struct led_data_t *data =
            k_fifo_get(&yellow_fifo,
                       K_FOREVER);


        printk("Yellow ON for %d ms\n",
               data->time_ms);


        gpio_pin_set_dt(&yellow_led, 1);

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&yellow_led, 0);


        printk("Yellow OFF\n");

        bool wait_done =
            data->wait_done;


        k_free(data);


        if (wait_done) {

            k_sem_give(&done_sem);
        }
    }
}

static void green_task(void *unused1,
                       void *unused2,
                       void *unused3)
{
    while (true) {

        k_mutex_lock(&green_mutex,
                     K_FOREVER);


        while (k_fifo_is_empty(&green_fifo)) {

            k_condvar_wait(&green_cond,
                           &green_mutex,
                           K_FOREVER);
        }


        k_mutex_unlock(&green_mutex);


        struct led_data_t *data =
            k_fifo_get(&green_fifo,
                       K_FOREVER);


        printk("Green ON for %d ms\n",
               data->time_ms);


        gpio_pin_set_dt(&green_led, 1);

        k_msleep(data->time_ms);

        gpio_pin_set_dt(&green_led, 0);


        printk("Green OFF\n");

        bool wait_done =
            data->wait_done;


        k_free(data);


        if (wait_done) {

            k_sem_give(&done_sem);
        }
    }
}

int main(void)
{
    int ret = init_uart();


    if (ret != 0) {

        printk("UART initialization failed!\n");

        return ret;
    }


    gpio_pin_configure_dt(&red_led,
                          GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure_dt(&yellow_led,
                          GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure_dt(&green_led,
                          GPIO_OUTPUT_INACTIVE);


    printk("Week 2 - 4p traffic light ready\n");


    return 0;
}

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
    red_thread,
    STACKSIZE,
    red_task,
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
    yellow_task,
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
    green_task,
    NULL,
    NULL,
    NULL,
    PRIORITY,
    0,
    0
);